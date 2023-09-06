#include <iomanip>
#include <iostream>
#include <vector>

#include <shlwapi.h>

#include <ShlObj.h>

#include <Windows.h>

#include <fcntl.h>
#include <io.h>

#include "synare.h"
#include "winutils.h"

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

static int doScan(const std::wstring &path) {
    WinUtils::winConsoleColorScope(
        [&]() {
            wprintf(L"[Scan Mode]\n");
            wprintf(L"This program is searching \"%s\" for infected files and recover them.\n", path.data());
        },
        WinUtils::Yellow | WinUtils::Highlight);
    ;
    if (!IsUserAnAdmin()) {
        WinUtils::winConsoleColorScope(
            [&]() {
                wprintf(L"Warning: %s\n",
                        L"This program isn't running as Administrator, the operation may be incomplete."); //
            },
            WinUtils::Yellow);
    }
    wprintf(L"\n");

    bool needBreak = false;
    bool ret = WinUtils::walkThroughDirectory(path, [&](const std::wstring &filePath) -> bool {
        if (_wcsicmp(WinUtils::pathFindExtension(filePath).data(), L"exe") != 0) {
            return true;
        }

        std::string data;
        if (Synare::parseWinExecutable(filePath, nullptr, &data) & Synare::Infected) {
            WinUtils::winClearConsoleLine();
            WinUtils::winConsoleColorScope(
                [&]() {
                    std::wcout << filePath << std::flush << std::endl; //
                },
                WinUtils::Red | WinUtils::Highlight);
            needBreak = false;

            // Remove infected file
            if (!DeleteFileW(filePath.data())) {
                auto code = ::GetLastError();

                // Try terminate process
                uint32_t pid = 0;
                if (!WinUtils::walkThroughProcesses([&](const WinUtils::ProcessInfo &info) -> bool {
                        WCHAR canonicalPath1[MAX_PATH];
                        WCHAR canonicalPath2[MAX_PATH];
                        PathCanonicalizeW(canonicalPath1, filePath.c_str());
                        PathCanonicalizeW(canonicalPath2, info.path.c_str());
                        if (_wcsicmp(canonicalPath1, canonicalPath2) == 0) {
                            pid = info.pid;
                            return true;
                        }
                        return false;
                    })) {
                    wprintf(L"Error: %s\n", WinUtils::winLastErrorMessage().data());
                    return false;
                }

                if (pid == 0) {
                    wprintf(L"Error: %s\n", WinUtils::winErrorMessage(code).data());
                    return false;
                }
                if (!WinUtils::killProcess(pid)) {
                    wprintf(L"Error: %s\n", WinUtils::winLastErrorMessage().data());
                    return false;
                }
                ::Sleep(50);
                if (!DeleteFileW(filePath.data())) {
                    wprintf(L"Error: %s\n", WinUtils::winLastErrorMessage().data());
                    return false;
                }
            }

            // Remove cached executable if exists
            std::wstring cacheFile =
                WinUtils::pathFindDirectory(filePath) + L"\\._cache_" + WinUtils::pathFindFileName(filePath);
            DeleteFileW(cacheFile.data());

            // Rewrite
            if (!data.empty() && !WinUtils::writeFile(filePath, data)) {
                return false;
            }
        } else {
            WinUtils::winClearConsoleLine();
            std::wcout << getShortPath(filePath) << std::flush;
            needBreak = true;
        }
        return true;
    });

    if (!ret) {
        if (needBreak) {
            std::wcout << std::endl;
        }
        wprintf(L"Error: %s\n", WinUtils::winLastErrorMessage().data());
        return false;
    }
    if (needBreak) {
        WinUtils::winClearConsoleLine();
    }
    wprintf(L"OK\n");
    return 0;
}

static int doRecover(const std::wstring &fileName, const std::wstring &outFileName) {
    if (_wcsicmp(WinUtils::pathFindExtension(fileName).data(), L".xlsm") == 0) {
        std::string data;
        switch (Synare::parseXlsmFile(fileName, &data)) {
            default:
                break;
        }
    } else {
        // Executable
        std::string version;
        std::string data;
        switch (Synare::parseWinExecutable(fileName, &version, &data)) {
            case Synare::EXEVSNX_NotFound: {
                wprintf(L"%s: File is not infected, the virus version not found.\n", fileName.data());
                return 2;
            }
            case Synare::EXERESX_NotFound: {
                WinUtils::winConsoleColorScope(
                    [&]() {
                        wprintf(L"%s: File is infected, but the binary resource not found.\n", fileName.data()); //
                    },
                    WinUtils::Red);
                return 0;
            }
            case Synare::EXE_Failed: {
                wprintf(L"Error: %s: %s\n", fileName.data(), WinUtils::winLastErrorMessage().data());
                return -1;
            }
            case Synare::EXE_Disguised: {
                WinUtils::winConsoleColorScope(
                    [&]() {
                        wprintf(L"%s: This tool is disguised as being infected.\n", fileName.data()); //
                    },
                    WinUtils::Yellow | WinUtils::Highlight);
                return -1;
            }
            default: {
                break;
            }
        }

        // Write output
        if (!WinUtils::writeFile(outFileName, data)) {
            wprintf(L"Error: %s: %s\n", outFileName.data(), WinUtils::winLastErrorMessage().data());
            return -1;
        }

        WinUtils::winConsoleColorScope(
            [&]() {
                wprintf(L"%s: Successfully recover, virus version %d.\n", fileName.data(),
                        std::atoi(version.data())); //
            },
            WinUtils::Green | WinUtils::Highlight);
    }
    return 0;
}

static bool removeFileSystemVirus() {
    std::wstring dirs[] = {
        WinUtils::getPathEnv(L"ALLUSERSPROFILE") + L"\\Synaptics",
        WinUtils::getPathEnv(L"WINDIR") + L"\\System32\\Synaptics",
    };

    for (const auto &dir : std::as_const(dirs)) {
        if (::PathIsDirectoryW(dir.data())) {
            WinUtils::winConsoleColorScope(
                [&]() {
                    wprintf(L"Remove \"%s\"\n", dir.data()); //
                },
                WinUtils::Red | WinUtils::Highlight);

            if (!WinUtils::removeDirectoryRecursively(dir))
                return false;
        }
    }

    return true;
}

static bool removeRegistryVirus() {
    struct RegEntry {
        const wchar_t *field;
        const wchar_t *key;
    };

#define REG_PREFIX      L"Software\\Microsoft\\Windows\\CurrentVersion\\"
#define VIRUS_FULL_NAME L"Synaptics Pointing Device Driver"

    static const RegEntry regs[] = {
        {REG_PREFIX LR"(Run)",             VIRUS_FULL_NAME                },
        {REG_PREFIX LR"(RunNotification)", L"StartupTNoti" VIRUS_FULL_NAME},
    };

    HKEY hKey;
    for (const auto &reg : regs) {
        LSTATUS result = RegOpenKeyExW(HKEY_CURRENT_USER, reg.field, 0, KEY_WRITE, &hKey);
        if (result == ERROR_SUCCESS) {
            switch (RegDeleteValueW(hKey, reg.key)) {
                case ERROR_SUCCESS: {
                    WinUtils::winConsoleColorScope(
                        [&]() {
                            wprintf(L"Remove entry \"%s\\%s\"\n", reg.field, reg.key); //
                        },
                        WinUtils::Red | WinUtils::Highlight);
                    break;
                }
                case ERROR_FILE_NOT_FOUND: {
                    break;
                }
                default: {
                    RegCloseKey(hKey);
                    return false;
                    break;
                }
            }
            RegCloseKey(hKey);
        } else if (result != ERROR_FILE_NOT_FOUND) {
            return false;
        }
    }

    return true;
}

static int doKill() {
    WinUtils::winConsoleColorScope(
        [&]() {
            wprintf(L"[Kill Mode]\n");
            wprintf(L"This program will kill the virus process and clean your filesystem and registry.\n");
        },
        WinUtils::Yellow | WinUtils::Highlight);
    ;
    if (!IsUserAnAdmin()) {
        WinUtils::winConsoleColorScope(
            [&]() {
                wprintf(L"Warning: %s\n",
                        L"This program isn't running as Administrator, the virus process may be invisible."); //
            },
            WinUtils::Yellow);
    }
    wprintf(L"\n");

    wprintf(L"[Step 1] Terminate virus process\n");
    {
        std::vector<WinUtils::ProcessInfo> processes;
        if (!WinUtils::walkThroughProcesses([&](const WinUtils::ProcessInfo &info) -> bool {
                if (Synare::parseWinExecutable(info.path, nullptr, nullptr) & Synare::Infected) {
                    processes.push_back(info);
                }
                return false;
            })) {
            wprintf(L"Error: %s\n", WinUtils::winLastErrorMessage().data());
            return -1;
        }

        if (!processes.empty()) {
            for (const auto &p : std::as_const(processes)) {
                wprintf(L"Killing process %-10ld %s\n", p.pid, p.path.data());

                if (!WinUtils::killProcess(p.pid)) {
                    wprintf(L"Error: %s\n", WinUtils::winLastErrorMessage().data());
                    return -1;
                }
            }

            // Wait process terminated
            ::Sleep(50);
        }
        wprintf(L"OK\n");
    }

    wprintf(L"\n");
    wprintf(L"[Step 2] Remove virus directories\n");
    {
        if (!removeFileSystemVirus()) {
            wprintf(L"Error: %s\n", WinUtils::winLastErrorMessage().data());
            return -1;
        }
        wprintf(L"OK\n");
    }

    wprintf(L"\n");
    wprintf(L"[Step 3] Clean Registry\n");
    {
        if (!removeRegistryVirus()) {
            wprintf(L"Error: %s\n", WinUtils::winLastErrorMessage().data());
            return -1;
        }
        wprintf(L"OK\n");
    }

    return 0;
}

static void displayVersion() {
    wprintf(L"%s\n", TEXT(APP_VERSION));
}

static void displayHelpText() {
    wprintf(L"Command line tool to remove Synaptics Virus.\n");
    wprintf(L"\n");
    wprintf(L"Usage: %s [-k] [-h] [-v] [<dir>] [<input> [output]]\n", WinUtils::appName().data());
    wprintf(L"\n");
    wprintf(L"Modes:\n");
    wprintf(L"    %-12s: Kill virus processes, remove virus directories and registry entries\n", L"Kill mode");
    wprintf(L"    %-12s: Scan the given directory recursively, recover infected executables\n", L"Scan Mode");
    wprintf(L"    %-12s: Read the given file, output the original one if infected\n", L"Single Mode");
    wprintf(L"\n");
    wprintf(L"Options:\n");
    wprintf(L"    %-16s    Run in kill mode\n", L"-k");
    wprintf(L"    %-16s    Show this message\n", L"-h/--help");
    wprintf(L"    %-16s    Show version\n", L"-v/--version");
}

int main(int argc, char *argv[]) {
    (void) argc; // unused
    (void) argv; // unused

    struct LocaleGuard {
        LocaleGuard() {
            mode = _setmode(_fileno(stdout), _O_U16TEXT);
        }
        ~LocaleGuard() {
            _setmode(_fileno(stdout), mode);
        }
        int mode;
    };
    LocaleGuard lg;

    std::vector<std::wstring> fileNames;
    bool version = false;
    bool kill = false;
    bool help = false;

    // Parse arguments
    {
        std::vector<std::wstring> arguments = WinUtils::commandLineArguments();
        for (int i = 1; i < arguments.size(); ++i) {
            const auto &arg = arguments[i];
            if (arg == L"--version" || arg == L"-v") {
                version = true;
                break;
            }
            if (arg == L"--help" || arg == L"-h") {
                help = true;
                break;
            }
            if (arg == L"-k") {
                kill = true;
                break;
            }
            fileNames.push_back(arg);
        }
    }

    if (version) {
        displayVersion();
        return 0;
    }

    if (argc == 1 || help) {
        displayHelpText();
        return 0;
    }

    if (kill) {
        return doKill();
    }

    if (fileNames.empty()) {
        displayHelpText();
        return 0;
    }

    std::wstring fileName = fileNames.front();
    if (::PathIsDirectoryW(fileName.data())) {
        auto path = WinUtils::fixDirectoryPath(fileName);
        path = ::PathIsRelativeW(path.data()) ? WinUtils::getAbsolutePath(WinUtils::currentDirectory(), path)
                                              : WinUtils::getAbsolutePath(path, L".");
        if (path.empty()) {
            wprintf(L"Error: %s\n", L"Invalid path.");
            return -1;
        }
        return doScan(path);
    }

    std::wstring outFileName = fileNames.size() > 1 ? fileNames.at(1) : [&]() {
        auto dir = WinUtils::pathFindDirectory(fileName);
        if (!dir.empty()) {
            dir += L"\\";
        }
        return dir + L"recover_" + WinUtils::pathFindFileName(fileName);
    }();
    return doRecover(fileName, outFileName);
}