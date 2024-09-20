#include "winutils.h"

#include <Windows.h>

#include <Shlwapi.h>

#include <TlHelp32.h>

#include <psapi.h>

namespace WinUtils {

    static const DWORD g_EnglishLangId = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);

    static inline int longPathBufferSize() {
        return 65536;
    }

    static wchar_t *longPathBuffer() {
        static auto buffer = new wchar_t[longPathBufferSize()];
        return buffer;
    }

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
        ::SetLastError(err);
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
                    if (!WinUtils::removeFile(filePath)) {
                        return false;
                    }
                }
            }
        } while (FindNextFileW(hFind, &findFileData));

        FindClose(hFind);

        if (!removeFile(directoryPath)) {
            return false;
        }
        return true;
    }

    bool removeDirectoryRecursively(const std::wstring &dir) {
        return removeDirectoryImpl(fixDirectoryPath(dir));
    }

    bool removeFile(const std::wstring &fileName) {
        auto attr = GetFileAttributesW(fileName.data());
        if (attr == INVALID_FILE_ATTRIBUTES) {
            return true;
        }

        // Force delete readonly files
        if (attr & FILE_ATTRIBUTE_READONLY) {
            if (!SetFileAttributesW(fileName.data(), attr & (~FILE_ATTRIBUTE_READONLY))) {
                return false;
            }
        }
        if (attr & FILE_ATTRIBUTE_DIRECTORY) {
            return RemoveDirectoryW(fileName.data());
        }
        return DeleteFileW(fileName.data());
    }

    static bool walkThroughDirectoryImpl(const std::wstring &dir, const std::function<bool(const std::wstring &)> &func,
                                         bool strict) {
        WIN32_FIND_DATA findFileData;
        std::wstring searchPath = dir + L"\\*.*";
        HANDLE hFind = FindFirstFileW(searchPath.data(), &findFileData);
        if (hFind == INVALID_HANDLE_VALUE)
            return true;

        do {
            if (wcscmp(findFileData.cFileName, L".") != 0 && wcscmp(findFileData.cFileName, L"..") != 0) {
                std::wstring filePath = dir + L"\\" + findFileData.cFileName;
                if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    if (!walkThroughDirectoryImpl(filePath, func, strict) && strict) {
                        FindClose(hFind);
                        return false;
                    }
                } else {
                    if (!func(filePath) && strict) {
                        FindClose(hFind);
                        return false;
                    }
                }
            }
        } while (FindNextFileW(hFind, &findFileData) != 0);
        FindClose(hFind);
        return true;
    }

    bool walkThroughDirectory(const std::wstring &dir, const std::function<bool(const std::wstring &)> &func,
                              bool strict) {
        return walkThroughDirectoryImpl(fixDirectoryPath(dir), func, strict);
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

    bool pathIsFile(const std::wstring &path) {
        DWORD attributes = GetFileAttributesW(path.data());
        if (attributes != INVALID_FILE_ATTRIBUTES) {
            if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
                return true;
            }
        }
        return false; // Directory or invalid
    }

    std::wstring appFilePath() {
        auto buf = longPathBuffer();
        if (!::GetModuleFileNameW(nullptr, buf, longPathBufferSize())) {
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
        auto buf = longPathBuffer();
        if (!::GetCurrentDirectoryW(longPathBufferSize(), buf)) {
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

    bool walkThroughProcesses(const std::function<bool(const ProcessInfo &, void *)> &func) {
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
                auto szProcessPath = longPathBuffer();
                if (GetModuleFileNameExW(hProcess, NULL, szProcessPath, longPathBufferSize())) {
                    if (func({szProcessPath, pe32.th32ProcessID}, hProcess)) {
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
        auto buf = longPathBuffer();
        auto result = ::GetEnvironmentVariableW(key, buf, longPathBufferSize());
        if (!result)
            return {};
        return buf;
    }

    std::wstring getAbsolutePath(const std::wstring &basePath, const std::wstring &relativePath) {
        return getCanonicalPath(basePath + L"\\" + relativePath);
    }

    std::wstring getCanonicalPath(const std::wstring &path) {
        std::wstring fullPath =
            LR"(\\?\)" + (::PathIsRelativeW(path.data()) ? (currentDirectory() + L"\\" + path) : path);
        auto buf = longPathBuffer();
        DWORD result = GetFullPathNameW(fullPath.c_str(), longPathBufferSize(), longPathBuffer(), nullptr);
        if (result == 0) {
            return {};
        }
        return std::wstring(buf + 4, result - 4);
    }

    std::wstring strMulti2Wide(const std::string &bytes) {
        int len = MultiByteToWideChar(CP_UTF8, 0, bytes.data(), (int) bytes.size(), nullptr, 0);
        auto buf = new wchar_t[len + 1];
        MultiByteToWideChar(CP_UTF8, 0, bytes.data(), (int) bytes.size(), buf, len);
        buf[len] = '\0';

        std::wstring res(buf);
        delete[] buf;
        return res;
    }

    std::string strWide2Multi(const std::wstring &str) {
        int len = WideCharToMultiByte(CP_UTF8, 0, str.data(), (int) str.size(), nullptr, 0, nullptr, nullptr);
        auto buf = new char[len + 1];
        WideCharToMultiByte(CP_UTF8, 0, str.data(), (int) str.size(), buf, len, nullptr, nullptr);
        buf[len] = '\0';

        std::string res(buf);
        delete[] buf;
        return res;
    }

}