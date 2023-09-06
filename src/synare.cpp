#include "synare.h"

#include <Windows.h>

#include <Shlwapi.h>

namespace Synare {

    static const char strDisguise[] = "Synaptics Fuck Your Asshole!!!";

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

    class LibraryScopeGuard {
    public:
        LibraryScopeGuard(const std::wstring &fileName) {
            module = LoadLibraryExW(fileName.data(), NULL, LOAD_LIBRARY_AS_DATAFILE);
        }

        LibraryScopeGuard() {
            if (module)
                FreeLibrary(module);
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
                return EXE_Failed;
            case ResourceNotFound:
                return EXERESX_NotFound;
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
        if (findRes == ResourceFound && // Only when all resources match, it will be recognized as disguised
            !strcmp(strDisguise, str114514.data()) && !strcmp(strDisguise, strXLSM.data()) &&
            !strcmp(strDisguise, strEXERESX.data())) {
            return EXE_Disguised;
        }

        return EXERESX_Found;
    }

    VirusExeResult parseXlsmFile(const std::wstring &fileName, std::string *data) {
        // TODO
        return {};
    }

}