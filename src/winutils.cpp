#include "winutils.h"

#include <Windows.h>

#include <Shlwapi.h>

#include <TlHelp32.h>

#include <psapi.h>

namespace WinUtils {

    static const DWORD g_EnglishLangId = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);

    std::vector<std::wstring> commandLineArguments() {
        int argc;
        auto argvW = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
        if (!argvW) {
            return {};
        }

        std::vector<std::wstring> res;
        for (int i = 0; i < argc; ++i) {
            res.push_back(argvW[i]);
        }
        ::LocalFree(argvW);
        return res;
    }

    std::wstring winErrorMessage(uint32_t error, bool nativeLanguage) {
        std::wstring rc;
        wchar_t *lpMsgBuf;

        const DWORD len = ::FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error,
            nativeLanguage ? 0 : g_EnglishLangId, reinterpret_cast<LPWSTR>(&lpMsgBuf), 0, NULL);

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

    std::wstring winLastErrorMessage(bool nativeLanguage, uint32_t *code) {
        auto err = ::GetLastError();
        auto res = winErrorMessage(err, nativeLanguage);
        ::SetLastErrorEx(err, nativeLanguage);
        if (code) {
            *code = err;
        }
        return res;
    }

    bool writeFile(const std::wstring &fileName, const std::string &data) {
        HANDLE hFile = CreateFileW(fileName.data(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            return false;
        }
        DWORD bytesWritten;
        if (!WriteFile(hFile, data.data(), data.size(), &bytesWritten, NULL)) {
            CloseHandle(hFile);
            return false;
        }
        CloseHandle(hFile);
        return true;
    }

    std::wstring fixDirectoryPath(const std::wstring &path) {
        std::wstring fixedPath;
        for (const auto &ch : path) {
            if (ch == '/') {
                fixedPath += '\\';
                continue;
            }
            fixedPath += ch;
        }
        while (fixedPath.size() > 0 && fixedPath.back() == '\\') {
            fixedPath.erase(fixedPath.end() - 1, fixedPath.end());
        }
        return fixedPath;
    }

    static bool removeDirectoryImpl(const std::wstring &directoryPath) {
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
                    if (!removeDirectoryImpl(filePath)) {
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

    bool removeDirectoryRecursively(const std::wstring &dir) {
        return removeDirectoryImpl(fixDirectoryPath(dir));
    }

    static bool walkThroughDirectoryImpl(const std::wstring &dir,
                                         const std::function<bool(const std::wstring &)> &func) {
        WIN32_FIND_DATA findFileData;
        std::wstring searchPath = dir + L"\\*.*";
        HANDLE hFind = FindFirstFileW(searchPath.data(), &findFileData);
        if (hFind == INVALID_HANDLE_VALUE)
            return true;

        do {
            if (wcscmp(findFileData.cFileName, L".") != 0 && wcscmp(findFileData.cFileName, L"..") != 0) {
                std::wstring filePath = dir + L"\\" + findFileData.cFileName;
                if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    if (!walkThroughDirectoryImpl(filePath, func)) {
                        // Ignore
                    }
                } else {
                    // Handle file name
                    func(filePath);
                }
            }
        } while (FindNextFileW(hFind, &findFileData) != 0);
        FindClose(hFind);
        return true;
    }

    bool walkThroughDirectory(const std::wstring &dir, const std::function<bool(const std::wstring &)> &func) {
        return walkThroughDirectoryImpl(fixDirectoryPath(dir), func);
    }

    std::wstring pathFindFileName(const std::wstring &path) {
        auto name = fixDirectoryPath(path);
        auto slashIdx = path.find_last_of(L"/\\");
        if (slashIdx != std::wstring::npos) {
            name = path.substr(slashIdx + 1);
        }
        return name;
    }

    std::wstring pathFindBaseName(const std::wstring &path) {
        auto name = pathFindFileName(path);
        auto dotIndex = name.find_last_of(L".");
        if (dotIndex != std::wstring::npos) {
            name = name.substr(0, dotIndex);
        }
        return name;
    }

    std::wstring pathFindDirectory(const std::wstring &path) {
        auto dir = fixDirectoryPath(path);
        auto slashIdx = path.find_last_of(L"/\\");
        if (slashIdx != std::wstring::npos) {
            dir = path.substr(0, slashIdx);
        } else {
            dir.clear();
        }
        return dir;
    }

    std::wstring pathFindExtension(const std::wstring &path) {
        std::wstring name = pathFindFileName(path);
        auto dotIndex = name.find_last_of(L".");
        if (dotIndex != std::wstring::npos) {
            name = name.substr(dotIndex + 1);
        } else {
            name.clear();
        }
        return name;
    }

    std::wstring appFilePath() {
        wchar_t buf[MAX_PATH];
        if (!::GetModuleFileNameW(nullptr, buf, MAX_PATH)) {
            return {};
        }
        return buf;
    }

    std::wstring appDirectory() {
        return pathFindDirectory(appFilePath());
    }

    std::wstring appName() {
        return pathFindBaseName(appFilePath());
    }

    std::wstring currentDirectory() {
        wchar_t buf[MAX_PATH];
        if (!::GetCurrentDirectoryW(MAX_PATH, buf)) {
            return {};
        }
        return buf;
    }

    void winConsoleColorScope(const std::function<void()> &func, int color) {
        WORD winColor = 0;
        if (color & ConsoleColor::Red) {
            winColor |= FOREGROUND_RED;
        }
        if (color & ConsoleColor::Blue) {
            winColor |= FOREGROUND_BLUE;
        }
        if (color & ConsoleColor::Green) {
            winColor |= FOREGROUND_GREEN;
        }
        if (color & ConsoleColor::Highlight) {
            winColor |= FOREGROUND_INTENSITY;
        }

        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hConsole, &csbi);
        SetConsoleTextAttribute(hConsole, winColor);

        func();

        // Restore
        SetConsoleTextAttribute(hConsole, csbi.wAttributes);
    }

    void winClearConsoleLine() {
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

    bool walkThroughProcesses(const std::function<bool(const ProcessInfo &)> &func) {
        HANDLE hProcessSnap;
        hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hProcessSnap == INVALID_HANDLE_VALUE) {
            return false;
        }

        PROCESSENTRY32W pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32W);
        if (!Process32FirstW(hProcessSnap, &pe32)) {
            CloseHandle(hProcessSnap);
            return false;
        }

        struct TargetProcess {
            std::wstring path;
            DWORD pid;
        };
        std::vector<TargetProcess> processes;

        bool over = false;
        do {
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID);
            if (hProcess) {
                wchar_t szProcessPath[MAX_PATH];
                if (GetModuleFileNameExW(hProcess, NULL, szProcessPath, MAX_PATH)) {
                    if (func({szProcessPath, pe32.th32ProcessID})) {
                        over = true;
                    }
                }
                CloseHandle(hProcess);
            }
        } while (Process32NextW(hProcessSnap, &pe32) && !over);

        CloseHandle(hProcessSnap);
        return true;
    }

    bool killProcess(uint32_t pid) {
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (hProcess == NULL) {
            return false;
        }

        if (!TerminateProcess(hProcess, 0)) {
            CloseHandle(hProcess);
            return false;
        }

        CloseHandle(hProcess);
        return true;
    }

    std::wstring getPathEnv(const wchar_t *key) {
        wchar_t buf[MAX_PATH];
        auto result = ::GetEnvironmentVariableW(key, buf, MAX_PATH);
        if (!result)
            return {};
        return buf;
    }

    std::wstring getAbsolutePath(const std::wstring &basePath, const std::wstring &relativePath) {
        wchar_t absolutePath[MAX_PATH];
        if (PathCanonicalizeW(absolutePath, (basePath + L"\\" + relativePath).data())) {
            return absolutePath;
        }
        return {};
    }

}