#include <iostream>
#include <vector>

#include <shlwapi.h>

#include <Windows.h>

#include <fcntl.h>
#include <io.h>

struct LocaleGuard {
    LocaleGuard() {
        mode = _setmode(_fileno(stdout), _O_U16TEXT);
    }
    ~LocaleGuard() {
        _setmode(_fileno(stdout), mode);
    }
    int mode;
};

static std::wstring winErrorMessage(unsigned long error) {
    std::wstring rc;
    wchar_t *lpMsgBuf;

    const DWORD langId = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
    const DWORD len =
        ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                         NULL, error, langId, reinterpret_cast<LPWSTR>(&lpMsgBuf), 0, NULL);
    if (len) {
        // Remove tail line breaks
        if (lpMsgBuf[len - 1] == L'\n') {
            lpMsgBuf[len - 1] = L'\0';
            if (len > 2 && lpMsgBuf[len - 2] == L'\r') {
                lpMsgBuf[len - 2] = L'\0';
            }
        }
        rc = std::wstring(lpMsgBuf, int(len));
        ::LocalFree(lpMsgBuf);
    } else {
        rc += L"unknown error";
    }

    return rc;
}

enum Result {
    NoResource,
    EXEVSNX_NotFound,
    EXERESX_NotFound,
    Failed,
    Success,
};

static Result recoverFile(const std::wstring &fileName, const std::wstring &outFileName, int *version,
                          std::wstring *errorString) {
    // Open PE file
    auto hModule = LoadLibraryExW(fileName.data(), NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (!hModule) {
        *errorString = winErrorMessage(::GetLastError());
        return Failed;
    }

    char *buf = nullptr;
    size_t size = 0;

    // Check EXEVSNX
    {
        // Find resource
        HRSRC hResource = FindResourceW(hModule, L"EXEVSNX", RT_RCDATA);
        if (hResource == NULL) {
            FreeLibrary(hModule);
            return EXEVSNX_NotFound;
        }

        // Load resource
        HGLOBAL hResourceData = LoadResource(hModule, hResource);
        if (hResourceData == NULL) {
            *errorString = winErrorMessage(::GetLastError());
            FreeLibrary(hModule);
            return Failed;
        }

        // Lock resource
        LPVOID pData = LockResource(hResourceData);
        if (pData == NULL) {
            *errorString = winErrorMessage(::GetLastError());
            FreeLibrary(hModule);
            return Failed;
        }

        // Determine version resource type
        if (IS_INTRESOURCE(pData)) {
            // Not string
            FreeLibrary(hModule);
            return EXEVSNX_NotFound;
        }
        *version = atoi(std::string(static_cast<const char *>(pData), SizeofResource(hModule, hResource)).data());
        FreeResource(hResourceData);
    }

    // Get EXERESN
    {
        // Find resource
        HRSRC hResource = FindResourceW(hModule, L"EXERESX", RT_RCDATA);
        if (hResource == NULL) {
            FreeLibrary(hModule);
            return EXERESX_NotFound;
        }

        // Load resource
        HGLOBAL hResourceData = LoadResource(hModule, hResource);
        if (hResourceData == NULL) {
            *errorString = winErrorMessage(::GetLastError());
            FreeLibrary(hModule);
            return Failed;
        }

        // Lock resource
        LPVOID pData = LockResource(hResourceData);
        if (pData == NULL) {
            *errorString = winErrorMessage(::GetLastError());
            FreeLibrary(hModule);
            return Failed;
        }

        // Determine version resource type
        if (IS_INTRESOURCE(pData)) {
            // Not string
            FreeLibrary(hModule);
            return EXERESX_NotFound;
        }

        size = SizeofResource(hModule, hResource);
        if (size > 0) {
            buf = new char[size];
            memcpy(buf, pData, size);
        }

        FreeResource(hResourceData);
    }

    // Close PE file
    FreeLibrary(hModule);

    // Write recovered file
    if (size > 0) {
        HANDLE hFile = CreateFileW(outFileName.data(), // 文件名
                                   GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

        if (hFile == INVALID_HANDLE_VALUE) {
            *errorString = winErrorMessage(::GetLastError());
            return Failed;
        }

        DWORD bytesWritten;
        if (!WriteFile(hFile, buf, size, &bytesWritten, NULL)) {
            *errorString = winErrorMessage(::GetLastError());
            CloseHandle(hFile);
            return Failed;
        }

        CloseHandle(hFile);
    }

    delete buf;
    return Success;
}

int main(int argc, char *argv[]) {
    (void) argv; // unused

    auto argvW = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
    if (!argvW) {
        return -1;
    }

    // Parse arguments
    std::wstring wAppPath = argvW[0];
    std::vector<std::wstring> fileNames;
    bool showHelp = false;
    for (int i = 1; i < argc; ++i) {
        if (!wcscmp(argvW[i], L"--help") || !wcscmp(argvW[i], L"-h")) {
            showHelp = true;
            break;
        }
        fileNames.push_back(argvW[i]);
    }
    LocalFree(argvW);

    if (fileNames.empty() || fileNames.size() > 2 || showHelp) {
        auto wAppName = wAppPath;
        auto slashIdx = wAppPath.find_last_of(L"/\\");
        if (slashIdx != std::wstring::npos) {
            wAppName = wAppPath.substr(slashIdx + 1);
        }
        LocaleGuard lg;
        wprintf(L"Recover a Windows Executable infected by Synaptics Pointing Device Driver virus.\n");
        wprintf(L"Usage: %s <input> [output]\n", wAppName.data());
        return 0;
    }

    int version = 0;
    std::wstring errorString;
    const auto &fileName = fileNames.front();
    auto outFileName = fileNames.size() > 1 ? fileNames.at(1) : (L"recover_" + fileName);
    switch (recoverFile(fileName, outFileName, &version, &errorString)) {
        case NoResource: {
            LocaleGuard lg;
            wprintf(L"%s: File is not infected, the RC data not found.\n", fileName.data());
            return 1;
        }
        case EXEVSNX_NotFound: {
            LocaleGuard lg;
            wprintf(L"%s: File is not infected, the virus version not found.\n", fileName.data());
            return 2;
        }
        case EXERESX_NotFound: {
            LocaleGuard lg;
            wprintf(L"%s: File is infected, but the binary resource not found.\n", fileName.data());
            return 3;
        }
        case Failed: {
            LocaleGuard lg;
            wprintf(L"Error: %s\n", errorString.data());
            return -1;
        }
        default: {
            LocaleGuard lg;
            wprintf(L"Successfully recover \"%s\", virus version %d.\n", fileName.data(), version);
            break;
        }
    }

    return 0;
}
