#include "synare.h"

#include <Windows.h>

#include <Shlwapi.h>

#include <winutils.h>

#include <pugixml.hpp>
#include <zippy.hpp>

#include <sstream>

namespace Synare {

    static const char SYNARE_DISGUISE_STRING[] = APP_DISGUISE_STRING;

    static const char SYNARE_VBA_FRAGMENT[] = "https://www.dropbox.com/s/zhp1b06imehwylq/Synaptics.rar";

    enum FindResourceResult {
        ResourceNotFound,
        ResourceFound,
        ResourceFindFailed,
    };
    static FindResourceResult findStringResource(HMODULE hModule, const wchar_t *key, std::string *out) {
        HRSRC hResource = FindResourceW(hModule, key, RT_RCDATA);
        if (hResource == NULL) {
            return ResourceNotFound;
        }

        HGLOBAL hResourceData = LoadResource(hModule, hResource);
        if (hResourceData == NULL) {
            FreeResource(hResourceData);
            return ResourceFindFailed;
        }

        LPVOID pData = LockResource(hResourceData);
        if (pData == NULL) {
            FreeResource(hResourceData);
            return ResourceFindFailed;
        }

        if (IS_INTRESOURCE(pData)) {
            // Not string
            FreeResource(hResourceData);
            return ResourceFound;
        }

        if (out)
            *out = std::string(static_cast<const char *>(pData), SizeofResource(hModule, hResource));
        FreeResource(hResourceData);
        return ResourceFound;
    }

    static std::wstring findExeDescription(const std::wstring &filePath, const std::wstring &translation) {
        DWORD versionSize = GetFileVersionInfoSizeW(filePath.data(), NULL);
        if (versionSize == 0) {
            return {};
        }

        std::vector<char> versionInfo(versionSize);
        if (!GetFileVersionInfoW(filePath.data(), 0, versionSize, versionInfo.data())) {
            return {};
        }
        LPVOID productInfo;
        UINT productInfoSize;
        if (VerQueryValueW(versionInfo.data(), (L"\\StringFileInfo\\" + translation + L"\\ProductName").data(),
                           &productInfo, &productInfoSize)) {
            return std::wstring(static_cast<wchar_t *>(productInfo), productInfoSize);
        }
        return {};
    }

    static bool virusDescriptionMatches(const std::wstring &filePath) {
        static const wchar_t matched[] = L"Synaptics Pointing Device Driver";
        const std::wstring &desc = findExeDescription(filePath, L"041F04E6"); // Turkish
        return wcsncmp(desc.data(), matched, sizeof(matched) / sizeof(wchar_t)) == 0;
    }

    class LibraryScopeGuard {
    public:
        LibraryScopeGuard(const std::wstring &fileName) {
            module = LoadLibraryExW(fileName.data(), NULL, LOAD_LIBRARY_AS_DATAFILE);
        }

        ~LibraryScopeGuard() {
            if (module) {
                FreeLibrary(module);
            }
        }

        HMODULE handle() const {
            return module;
        }

    private:
        HMODULE module;
    };

    VirusExeResult parseWinExecutable(const std::wstring &fileName, std::string *version, std::string *data) {
        LibraryScopeGuard sg(fileName);
        auto hModule = sg.handle();
        if (!hModule) {
            return EXE_Failed;
        }

        FindResourceResult findRes;

        std::string strEXEVSNX;
        std::string strXLSM;
        std::string strEXERESX;
        std::string str114514;

        // Search `EXEVSNX`
        findRes = findStringResource(hModule, L"EXEVSNX", &strEXEVSNX);
        switch (findRes) {
            case ResourceFindFailed:
                return EXE_Failed;
            case ResourceNotFound:
                return EXEVSNX_NotFound;
            default:
                break;
        }

        if (version)
            *version = strEXEVSNX;

        // Search `EXERESX`
        findRes = findStringResource(hModule, L"EXERESX", &strEXERESX);
        switch (findRes) {
            case ResourceFindFailed:
                // Check the file description just in case
                if (virusDescriptionMatches(fileName)) {
                    return EXERESX_NotFound;
                }
                return EXE_Failed;
            case ResourceNotFound: {
                // Check the file description just in case
                if (virusDescriptionMatches(fileName)) {
                    return EXERESX_NotFound;
                }
                return EXEVSNX_NotFound;
            }
            default:
                break;
        }

        if (data)
            *data = strEXERESX;

        // Search `XLSM`
        findRes = findStringResource(hModule, L"XLSM", &strXLSM);
        if (findRes != ResourceFound) {
            return EXERESX_Found;
        }

        // Search `_114514`
        findRes = findStringResource(hModule, L"_114514", &str114514);
        if (findRes == ResourceFound && //
            !strcmp(SYNARE_DISGUISE_STRING, str114514.data()) && !strcmp(SYNARE_DISGUISE_STRING, strXLSM.data()) &&
            !strcmp(SYNARE_DISGUISE_STRING, strEXERESX.data())) {

            // Only when all resources match, it will be recognized as disguised
            // In case this tool is infected
            return EXE_Disguised;
        }

        return EXERESX_Found;
    }

    static bool subArrayExists(const char *text, int textLength, const char *pattern, int patternLength) {
        for (int i = 0; i <= textLength - patternLength; ++i) {
            bool match = true;
            for (int j = 0; j < patternLength; ++j) {
                if (text[i + j] != pattern[j]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                return true;
            }
        }
        return false;
    }

    namespace {

        class StringXmlWrite : public pugi::xml_writer {
        public:
            StringXmlWrite(std::ostringstream *buf) : buf(buf) {
            }

            void write(const void *data, size_t size) {
                buf->write(reinterpret_cast<const char *>(data), size);
            }

        private:
            std::ostringstream *buf;
        };

    } // namespace

    VirusXlsmResult parseXlsmFile(const std::wstring &fileName, const std::wstring &outFileName,
                                  std::wstring *errorString) {
        Zippy::ZipArchive zip;

        // NOWIDE uses UTF-8 file path
        try {
            zip.Open(WinUtils::strWide2Multi(fileName));
        } catch (const std::exception &e) {
            *errorString = WinUtils::strMulti2Wide(e.what());
            return XLSM_Failed;
        }
        if (!zip.IsOpen()) {
            *errorString = L"Failed to open document.";
            return XLSM_Failed;
        }

        // Get "[Content_Types].xml"
        static const char contentTypesPath[] = "[Content_Types].xml";
        if (!zip.HasEntry(contentTypesPath)) {
            *errorString = L"[Content_Types].xml not found.";
            zip.Close();
            return XLSM_Failed;
        }

        // Parse XML data
        pugi::xml_document contentTypesXml;
        if (contentTypesXml
                .load_string(zip.GetEntry(contentTypesPath).GetDataAsString().data(),
                             pugi::parse_default | pugi::parse_ws_pcdata)
                .status != pugi::status_ok) {
            *errorString = L"[Content_Types].xml format invalid.";
            zip.Close();
            return XLSM_Failed;
        }

        // Search for VBA Node
        pugi::xml_node vbaNode;
        pugi::xml_node workbookNode;
        for (auto &child : contentTypesXml.document_element().children()) {
            if (strcmp(child.attribute("ContentType").value(), "application/vnd.ms-office.vbaProject") == 0) {
                vbaNode = child;
            } else if (strcmp(child.attribute("ContentType").value(),
                              "application/vnd.ms-excel.sheet.macroEnabled.main+xml") == 0) {
                workbookNode = child;
            }
        }
        if (!vbaNode) {
            zip.Close();
            return XLSM_NoVBAProject;
        }

        // Get PartName
        const auto &partPath = vbaNode.attribute("PartName");
        std::string partPathStr = "xl/vbaProject.bin";
        if (partPath) {
            if (*partPath.value() == '/') {
                partPathStr = partPath.value() + 1;
            } else {
                partPathStr = partPath.value();
            }
        }
        if (!zip.HasEntry(partPathStr)) {
            zip.Close();
            return XLSM_VirusNotFound;
        }

        // Get VBAProject and search virus fragment
        const auto &vbaData = zip.GetEntry(partPathStr).GetData();
        if (!subArrayExists(reinterpret_cast<const char *>(vbaData.data()), vbaData.size(), SYNARE_VBA_FRAGMENT,
                            sizeof(SYNARE_VBA_FRAGMENT) - 1)) {
            zip.Close();
            return XLSM_VirusNotFound;
        }

        if (outFileName.empty()) {
            zip.Close();
            return XLSM_VirusFound;
        }

        // Do change
        {
            // Remove VBA binary
            zip.DeleteEntry(partPathStr);

            // Change workbook type
            if (workbookNode) {
                workbookNode.attribute("ContentType")
                    .set_value("application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml");
            }

            // Remove VBA xml node
            contentTypesXml.document_element().remove_child(vbaNode);

            // Save Content
            std::ostringstream os;
            StringXmlWrite writer(&os);
            contentTypesXml.save(writer, "");
            zip.AddEntry(contentTypesPath, os.str());
        }

        // Edit "xl/_rels/workbook.xml.rels"
        do {
            static const char workspaceRelPath[] = "xl/_rels/workbook.xml.rels";
            if (!zip.HasEntry(workspaceRelPath)) {
                break;
            }

            pugi::xml_document workspaceRelXml;
            if (workspaceRelXml
                    .load_string(zip.GetEntry(workspaceRelPath).GetDataAsString().data(),
                                 pugi::parse_default | pugi::parse_ws_pcdata)
                    .status != pugi::status_ok) {
                break;
            }

            pugi::xml_node vbaNode2;
            for (auto &child : workspaceRelXml.document_element().children()) {
                if (strcmp(child.attribute("Target").value(), "vbaProject.bin") == 0) {
                    vbaNode2 = child;
                    break;
                }
            }

            if (!vbaNode2) {
                break;
            }

            // Remove VBA xml node
            workspaceRelXml.document_element().remove_child(vbaNode2);

            // Save Content
            std::ostringstream os;
            StringXmlWrite writer(&os);
            workspaceRelXml.save(writer, "");
            zip.AddEntry(workspaceRelPath, os.str());
        } while (0);

        // Edit "workbook.xml"
        do {
            static const char workbookPath[] = "xl/workbook.xml";
            if (!zip.HasEntry(workbookPath)) {
                break;
            }

            pugi::xml_document workbookXml;
            if (workbookXml
                    .load_string(zip.GetEntry(workbookPath).GetDataAsString().data(),
                                 pugi::parse_default | pugi::parse_ws_pcdata)
                    .status != pugi::status_ok) {
                break;
            }

            pugi::xml_node sheetsNode = workbookXml.document_element().child("sheets");
            if (!sheetsNode) {
                break;
            }

            // Set sheets visible
            for (auto &child : sheetsNode.children()) {
                auto attr = child.attribute("state");
                if (strcmp(attr.value(), "hidden") == 0) {
                    child.remove_attribute(attr);
                }
            }

            // Save Content
            std::ostringstream os;
            StringXmlWrite writer(&os);
            workbookXml.save(writer, "");
            zip.AddEntry(workbookPath, os.str());
        } while (0);

        // Save
        try {
            zip.Save(WinUtils::strWide2Multi(outFileName));
        } catch (const std::exception &e) {
            *errorString = WinUtils::strMulti2Wide(e.what());
            zip.Close();
            return XLSM_Failed;
        }

        zip.Close();
        return XLSM_VirusFound;
    }

} // namespace Synare
