#include <iomanip>
#include <iostream>
#include <vector>

#include <shlwapi.h>

#include <psapi.h>

#include <ShlObj.h>

#include <TlHelp32.h>

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

static std::wstring winErrorMessage(DWORD error) {
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

static std::wstring getBaseName(const std::wstring &path) {
    auto name = path;
    auto slashIdx = path.find_last_of(L"/\\");
    if (slashIdx != std::wstring::npos) {
        name = path.substr(slashIdx + 1);
    }
    auto dotIndex = name.find_last_of(L".");
    if (dotIndex != std::wstring::npos) {
        name = name.substr(0, dotIndex);
    }
    return name;
}

enum Result {
    NoResource,
    EXEVSNX_NotFound,
    EXERESX_NotFound,
    Failed,
    Success,
    MySelf,
};

/*!
    \param fileName Windows Executable file
    \param version Virus version
    \param data Original executable data
    \param errorString Error string if fail
*/
static Result parseExecutable(const std::wstring &fileName, int *version, std::string *data,
                              std::wstring *errorString) {
    // Open PE file
    auto hModule = LoadLibraryExW(fileName.data(), NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (!hModule) {
        *errorString = winErrorMessage(::GetLastError());
        return Failed;
    }

    // Check EXEVSNX
    int version1 = 0;
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

        version1 = std::atoi(std::string(static_cast<const char *>(pData), SizeofResource(hModule, hResource)).data());
        if (version)
            *version = version1;
        FreeResource(hResourceData);
    }

    // 114514 (It's me)
    {
        HRSRC hResource = FindResourceW(hModule, L"_114514", RT_RCDATA);
        if (hResource != NULL && version1 == 0) {
            FreeLibrary(hModule);
            return MySelf;
        }
    }

    // Get XLSM
    {
        HRSRC hResource = FindResourceW(hModule, L"XLSM", RT_RCDATA);
        if (hResource == NULL) {
            FreeLibrary(hModule);
            return EXEVSNX_NotFound;
        }
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

        if (data) {
            *data = std::string(static_cast<char *>(pData), SizeofResource(hModule, hResource));
        }
        FreeResource(hResourceData);
    }

    // Close PE file
    FreeLibrary(hModule);
    return Success;
}

static bool writeFile(const std::wstring &fileName, const std::string &data, std::wstring *errorString) {
    HANDLE hFile = CreateFileW(fileName.data(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        if (errorString) {
            *errorString = winErrorMessage(::GetLastError());
        }
        return false;
    }
    DWORD bytesWritten;
    if (!WriteFile(hFile, data.data(), data.size(), &bytesWritten, NULL)) {
        if (errorString) {
            *errorString = winErrorMessage(::GetLastError());
        }
        CloseHandle(hFile);
        return false;
    }
    CloseHandle(hFile);
    return true;
}

static bool removeDirectoryRecursively(const std::wstring &directoryPath) {
    WIN32_FIND_DATA findFileData;
    std::wstring searchPath = directoryPath + L"\\*.*";
    HANDLE hFind = FindFirstFileW(searchPath.data(), &findFileData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return false;
    }

    do {
        if (wcscmp(findFileData.cFileName, L".") != 0 && wcscmp(findFileData.cFileName, L"..") != 0) {
            std::wstring filePath = directoryPath + L"\\" + findFileData.cFileName;
            if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (!removeDirectoryRecursively(filePath)) {
                    return false;
                }
            } else {
                if (!DeleteFileW(filePath.c_str())) {
                    return false;
                }
            }
        }
    } while (FindNextFileW(hFind, &findFileData));

    FindClose(hFind);

    if (!RemoveDirectoryW(directoryPath.data())) {
        return false;
    }
    return true;
}

static void ClearCurrentConsoleLine() {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    GetConsoleScreenBufferInfo(hConsole, &csbi);

    COORD cursorPosition;
    cursorPosition.X = 0;
    cursorPosition.Y = csbi.dwCursorPosition.Y;
    SetConsoleCursorPosition(hConsole, cursorPosition);

    DWORD numCharsWritten;
    DWORD lineLength = csbi.dwSize.X;
    std::wstring spaces(lineLength, L' ');

    WriteConsoleOutputCharacterW(hConsole, spaces.data(), lineLength, cursorPosition, &numCharsWritten);
    SetConsoleCursorPosition(hConsole, cursorPosition);
}

static void displayVersion() {
    wprintf(L"%s\n", TEXT(APP_VERSION));
}

static void displayHelpText(const std::wstring &wAppName) {
    wprintf(L"Command line tool to kill Synaptics Virus.\n");
    wprintf(L"Usage: %s [options] [<input> [output]]\n", wAppName.data());
    wprintf(L"\n");
    wprintf(L"Options:\n");
    wprintf(L"    %-16s    Scan processes, program data and registry to kill the virus\n", L"-k");
    wprintf(L"    %-16s    Scan a directory recursively, recover infected executables\n", L"-s <dir>");
    wprintf(L"    %-16s    Show this message\n", L"-h/--help");
    wprintf(L"    %-16s    Show version\n", L"-v/--version");
    wprintf(L"\n");
    wprintf(L"If no option is specified, this program will try to recover the input executable file.\n");
}

static int doKill() {
    if (!IsUserAnAdmin()) {
        wprintf(L"Error: %s\n", L"This mode requires the Administrator privilege.");
        return -1;
    }

    // Scan processes
    wprintf(L"[Step 1] Terminate virus process\n");

    HANDLE hProcessSnap;
    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        wprintf(L"Error: %s\n", winErrorMessage(::GetLastError()).data());
        return -1;
    }

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);
    if (!Process32FirstW(hProcessSnap, &pe32)) {
        wprintf(L"Error: %s\n", winErrorMessage(::GetLastError()).data());
        CloseHandle(hProcessSnap);
        return -1;
    }

    struct TargetProcess {
        std::wstring path;
        DWORD pid;
    };
    std::vector<TargetProcess> processes;

    do {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID);
        if (hProcess) {
            wchar_t szProcessPath[MAX_PATH];
            if (GetModuleFileNameExW(hProcess, NULL, szProcessPath, MAX_PATH)) {
                // Check
                std::wstring errorString;
                auto ret = parseExecutable(szProcessPath, nullptr, nullptr, &errorString);
                if (ret == Success || ret == EXERESX_NotFound) {
                    processes.push_back({szProcessPath, pe32.th32ProcessID});
                }
            }
            CloseHandle(hProcess);
        }
    } while (Process32NextW(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);

    if (processes.empty()) {
        wprintf(L"No infected process running.\n");
    } else {
        for (const auto &p : std::as_const(processes)) {
            wprintf(L"Killing process %-10ld %s\n", p.pid, p.path.data());

            HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, p.pid);
            if (hProcess == NULL) {
                wprintf(L"Error: %s\n", winErrorMessage(::GetLastError()).data());
                return -1;
            }

            if (!TerminateProcess(hProcess, 0)) {
                CloseHandle(hProcess);
                wprintf(L"Error: %s\n", winErrorMessage(::GetLastError()).data());
                return -1;
            }

            CloseHandle(hProcess);
        }
        wprintf(L"OK\n");

        ::Sleep(10);
    }
    wprintf(L"\n");

    // Remove virus's home: C:\ProgramData\Synaptics
    wprintf(L"[Step 2] Prevent virus persistence\n");

    auto home = L"C:\\ProgramData\\Synaptics";
    if (::PathIsDirectoryW(home)) {
        wprintf(L"Remove virus home \"%s\"\n", home);

        int tryCount = 0;
        while (true) {
            wprintf(L"Remove attempt %d\n", tryCount + 1);
            if (removeDirectoryRecursively(home)) {
                break;
            }
            wprintf(L"Error: %s\n", winErrorMessage(::GetLastError()).data());
            tryCount++;
            if (tryCount > 10) {
                return -1;
            }
            ::Sleep(100);
        }
        wprintf(L"OK\n");
    } else {
        wprintf(L"The virus home \"%s\" doesn't exist.\n", home);
    }
    wprintf(L"\n");

    // Remove registry
    wprintf(L"[Step 3] Clean Registry\n");

    struct RegValue {
        const wchar_t *field;
        const wchar_t *key;
    };
    static const RegValue regs[] = {
        {LR"(Software\Microsoft\Windows\CurrentVersion\Run)",             //
         L"Synaptics Pointing Device Driver"                        },
        {LR"(Software\Microsoft\Windows\CurrentVersion\RunNotification)", //
         L"StartupTNotiSynaptics Pointing Device Driver"},
    };

    HKEY hKey;
    for (const auto &reg : regs) {
        LSTATUS result = RegOpenKeyExW(HKEY_CURRENT_USER, reg.field, 0, KEY_WRITE, &hKey);
        if (result == ERROR_SUCCESS) {
            if (RegDeleteValueW(hKey, reg.key) == ERROR_SUCCESS) {
                wprintf(L"Remove entry \"%s\\%s\"\n", reg.field, reg.key);
            }
            RegCloseKey(hKey);
        } else {
            wprintf(L"Error: %s\n", winErrorMessage(::GetLastError()).data());
            return -1;
        }
    }
    wprintf(L"OK\n");
    return 0;
}

static bool g_needEndline = false;

static std::wstring getShortPath(const std::wstring &longFilePath) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(hConsole, &csbi);

#ifdef min
#    undef min
#endif
    int len = std::min(80, (csbi.dwSize.X / 8) * 8);

    std::wstring shortPath;
    if (longFilePath.size() > len) {
        shortPath = longFilePath.substr(0, len * 3 / 8) + L"..." +
                    longFilePath.substr(longFilePath.size() - len * 5 / 8, len * 5 / 8);
    } else {
        shortPath = longFilePath;
    }
    return shortPath;
}

static bool doScanImpl(const std::wstring &dir) {
    WIN32_FIND_DATA findFileData;
    std::wstring searchPath = dir + L"\\*.*";
    HANDLE hFind = FindFirstFileW(searchPath.data(), &findFileData);
    if (hFind == INVALID_HANDLE_VALUE)
        return false;

    do {
        if (wcscmp(findFileData.cFileName, L".") != 0 && wcscmp(findFileData.cFileName, L"..") != 0) {
            std::wstring filePath = dir + L"\\" + findFileData.cFileName;
            if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (!doScanImpl(filePath)) {
                    // FindClose(hFind);
                    // return false;

                    // Ignore
                }
            } else if (_wcsicmp(wcsrchr(findFileData.cFileName, L'.'), L".exe") == 0) {
                // Exe file
                std::wstring errorString;
                std::string data;
                auto ret = parseExecutable(filePath, nullptr, &data, &errorString);
                if (ret == EXERESX_NotFound || ret == Success) {
                    // Print in red
                    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
                    CONSOLE_SCREEN_BUFFER_INFO csbi;
                    GetConsoleScreenBufferInfo(hConsole, &csbi);
                    SetConsoleTextAttribute(hConsole, FOREGROUND_RED);

                    ClearCurrentConsoleLine();
                    std::wcout << getShortPath(filePath) << std::flush << std::endl;
                    g_needEndline = false;

                    // Restore color
                    SetConsoleTextAttribute(hConsole, csbi.wAttributes);

                    // Remove
                    if (!DeleteFileW(filePath.data())) {
                        FindClose(hFind);
                        return false;
                    }

                    // Rewrite
                    if (!data.empty()) {
                        if (!writeFile(filePath, data, nullptr)) {
                            FindClose(hFind);
                            return false;
                        }
                    }
                } else {
                    // Print normal
                    ClearCurrentConsoleLine();

                    std::wcout << getShortPath(filePath) << std::flush;
                    g_needEndline = true;
                }
            }
        }
    } while (FindNextFileW(hFind, &findFileData) != 0);
    FindClose(hFind);
    return true;
}

static int doScan(const std::wstring &path) {
    std::wstring fixedPath;
    for (const auto &ch : path) {
        if (ch == '/') {
            fixedPath += '\\';
            continue;
        }
        fixedPath += ch;
    }
    if (fixedPath.size() > 0 && fixedPath.back() == '\\') {
        fixedPath.erase(fixedPath.end() - 1, fixedPath.end());
    }
    if (!doScanImpl(path)) {
        if (g_needEndline) {
            g_needEndline = false;
            wprintf(L"\n");
        }
        wprintf(L"Error: %s\n", winErrorMessage(::GetLastError()).data());
        return -1;
    }
    if (g_needEndline) {
        g_needEndline = false;
        ClearCurrentConsoleLine();
        wprintf(L"OK\n");
    }
    return 0;
}

static int doRecover(const std::wstring &fileName, const std::wstring &outFileName) {
    int version = 0;
    std::wstring errorString;
    std::string data;
    switch (parseExecutable(fileName, &version, &data, &errorString)) {
        case NoResource: {
            wprintf(L"%s: File is not infected, the RC data not found.\n", fileName.data());
            return 1;
        }
        case EXEVSNX_NotFound: {
            wprintf(L"%s: File is not infected, the virus version not found.\n", fileName.data());
            return 2;
        }
        case EXERESX_NotFound: {
            wprintf(L"%s: File is infected, but the binary resource not found.\n", fileName.data());
            return 3;
        }
        case Failed: {
            wprintf(L"Error: %s: %s\n", fileName.data(), errorString.data());
            return -1;
        }
        case MySelf: {
            wprintf(L"%s: This tool is disguised as being infected.\n", fileName.data(), errorString.data());
            return -1;
        }
        default: {
            break;
        }
    }

    errorString.clear();
    if (!writeFile(outFileName, data, &errorString)) {
        wprintf(L"Error: %s: %s\n", fileName.data(), errorString.data());
        return -1;
    }

    // Print in green
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN);

    wprintf(L"%s: Successfully recover, virus version %d.\n", fileName.data(), version);
    g_needEndline = false;

    // Restore color
    SetConsoleTextAttribute(hConsole, csbi.wAttributes);
    return 0;
}

int main(int argc, char *argv[]) {
    LocaleGuard lg;

    (void) argv; // unused

    auto argvW = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
    if (!argvW) {
        return -1;
    }

    // Parse arguments
    std::wstring wAppPath = argvW[0];
    std::vector<std::wstring> fileNames;
    std::wstring scanDir;
    bool version = false;
    bool kill = false;
    bool help = false;
    bool scan = false;
    for (int i = 1; i < argc; ++i) {
        if (!wcscmp(argvW[i], L"--version") || !wcscmp(argvW[i], L"-v")) {
            version = true;
            break;
        }
        if (!wcscmp(argvW[i], L"--help") || !wcscmp(argvW[i], L"-h")) {
            help = true;
            break;
        }
        if (!wcscmp(argvW[i], L"-k")) {
            kill = true;
            break;
        }
        if (!wcscmp(argvW[i], L"-s")) {
            scan = true;
            if (i + 1 < argc) {
                scanDir = argvW[i + 1];
            }
            break;
        }
        fileNames.push_back(argvW[i]);
    }
    LocalFree(argvW);

    if (version) {
        displayVersion();
        return 0;
    }

    if (argc == 1 || help) {
        displayHelpText(getBaseName(wAppPath));
        return 0;
    }

    if (kill) {
        return doKill();
    }

    if (scan) {
        if (scanDir.empty()) {
            wprintf(L"Directory to scan not specified.\n");
            return 1;
        }
        return doScan(scanDir);
    }

    if (fileNames.empty()) {
        displayHelpText(getBaseName(wAppPath));
        return 0;
    }

    return doRecover(fileNames.front(), fileNames.size() > 1 ? fileNames.at(1) : (L"recover_" + fileNames.front()));
}