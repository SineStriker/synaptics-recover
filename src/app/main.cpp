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
    int maxLen = std::min(104, (csbi.dwSize.X / 8) * 8);

    int len = 0;
    for (const auto &ch : longFilePath) {
        len += (ch > 0xFF) ? 2 : 1;
    }

    if (len <= maxLen)
        return longFilePath;

    // Cut string
    const int leftLen = maxLen * 3 / 8;
    const int rightLen = maxLen - leftLen;

    len = 0;
    int leftIndex = 0;
    for (auto it = longFilePath.begin(); it != longFilePath.end(); ++it) {
        const auto &ch = *it;
        len += (ch > 0xFF) ? 2 : 1;
        leftIndex++;
        if (len >= leftLen)
            break;
    }

    len = 0;
    int rightIndex = 0;
    for (auto it = longFilePath.rbegin(); it != longFilePath.rend(); ++it) {
        const auto &ch = *it;
        len += (ch > 0xFF) ? 2 : 1;
        rightIndex++;
        if (len >= rightLen)
            break;
    }

    return longFilePath.substr(0, leftIndex) + L"..." + longFilePath.substr(longFilePath.size() - rightIndex);
}

static bool forceDeleteExe(const std::wstring &filePath) {
    int attempts = 0;
    while (!DeleteFileW(filePath.data()) && ++attempts < 10) {
        // This file may be running
        // Try scan processes and terminate it

        auto code = ::GetLastError();
        uint32_t pid = 0;
        if (!WinUtils::walkThroughProcesses([&](const WinUtils::ProcessInfo &info, void *) -> bool {
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
            return false;
        }

        if (pid == 0) {
            ::SetLastError(code);
            return false;
        }

        WinUtils::winConsoleColorScope(
            [&]() {
                wprintf(L"Killing process %-10ld %s\n", pid, filePath.data()); //
            },
            WinUtils::Yellow);
        if (!WinUtils::killProcess(pid)) {
            return false;
        }
        ::Sleep(50);
    }
    return true;
}

static int doScan(const std::wstring &path) {
    WinUtils::winConsoleColorScope(
        [&]() {
            wprintf(L"[Scan Mode]\n");
            wprintf(L"Searching \"%s\" for infected files and recover them.\n", path.data());
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
        auto printNormal = [&needBreak](const std::wstring &s) {
            WinUtils::winClearConsoleLine();
            std::wcout << getShortPath(s) << std::flush;
            needBreak = true;
        };
        auto printHighlight = [&needBreak](const std::wstring &s, int color = WinUtils::Red | WinUtils::Highlight) {
            WinUtils::winClearConsoleLine();
            WinUtils::winConsoleColorScope(
                [&]() {
                    std::wcout << s << std::flush << std::endl; //
                },
                color);
            needBreak = false;
        };

        if (_wcsicmp(WinUtils::pathFindExtension(filePath).data(), L"xlsm") == 0) {
            // XLSM
            std::string data;
            printNormal(filePath);
            if (Synare::parseXlsmFile(filePath, &data) & Synare::Infected) {
                printHighlight(filePath);

                // TODO
                if (!DeleteFileW(filePath.data())) {
                }
            }
        } else if (_wcsicmp(WinUtils::pathFindExtension(filePath).data(), L"exe") == 0) {
            // EXE
            std::string data;
            printNormal(filePath);
            if (Synare::parseWinExecutable(filePath, nullptr, &data) & Synare::Infected) {
                printHighlight(filePath);

                if (!forceDeleteExe(filePath)) {
                    wprintf(L"Error: %s\n", WinUtils::winLastErrorMessage().data());
                    return false;
                }

                // Remove cached executable if exists
                std::wstring cacheFileName = L"._cache_" + WinUtils::pathFindFileName(filePath);
                std::wstring cacheFilePath = WinUtils::pathFindDirectory(filePath) + L"\\" + cacheFileName;
                if (WinUtils::pathIsFile(cacheFilePath)) {
                    printHighlight(cacheFilePath, WinUtils::Green | WinUtils::Highlight);
                    if (!forceDeleteExe(cacheFilePath)) {
                        wprintf(L"Warning: %s: %s\n", cacheFileName.data(), WinUtils::winLastErrorMessage().data());
                    }
                }

                // Rewrite
                if (!data.empty() && !WinUtils::writeFile(filePath, data)) {
                    return false;
                }
            } else if (wcsncmp(L"._cache_", WinUtils::pathFindFileName(filePath).data(), 8) == 0) {
                auto fileDir = WinUtils::pathFindDirectory(filePath);
                auto originalFile = fileDir + L"\\" + WinUtils::pathFindFileName(filePath).substr(8);
                if (!WinUtils::pathIsFile(originalFile)) {
                    // Original file doesn't exist
                    // Check file attributes
                    auto attributes = GetFileAttributesW(filePath.data());
                    if ((attributes & FILE_ATTRIBUTE_HIDDEN) && (attributes & FILE_ATTRIBUTE_SYSTEM)) {
                        // Hidden and system
                        // It's most likely a cached executable
                        printHighlight(filePath, WinUtils::Green | WinUtils::Highlight);
                        if (CopyFileW(filePath.c_str(), originalFile.c_str(), false)) {
                            // Set normal file attributes
                            SetFileAttributesW(originalFile.c_str(), FILE_ATTRIBUTE_NORMAL);

                            // Remove
                            if (!forceDeleteExe(filePath)) {
                                wprintf(L"Warning: %s: %s\n", WinUtils::pathFindFileName(filePath).data(),
                                        WinUtils::winLastErrorMessage().data());
                            }
                        } else {
                            wprintf(L"Warning: %s: %s\n", WinUtils::pathFindFileName(filePath).data(),
                                    WinUtils::winLastErrorMessage().data());
                        }
                    }
                }
            }
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

static int doRecover(const std::wstring &filePath, const std::wstring &outFileName) {
    if (_wcsicmp(WinUtils::pathFindExtension(filePath).data(), L"xlsm") == 0) {
        // XLSM
        std::string data;
        switch (Synare::parseXlsmFile(filePath, &data)) {
            case Synare::XLSM_Failed: {
                wprintf(L"Error: %s: %s\n", filePath.data(), WinUtils::winLastErrorMessage().data());
                return -1;
            }
            default:
                break;
        }

        // Write output
        if (!WinUtils::writeFile(outFileName, data)) {
            wprintf(L"Error: %s: %s\n", outFileName.data(), WinUtils::winLastErrorMessage().data());
            return -1;
        }

        WinUtils::winConsoleColorScope(
            [&]() {
                wprintf(L"%s: Successfully recover XLSX file.\n", filePath.data()); //
            },
            WinUtils::Green | WinUtils::Highlight);
    } else if (_wcsicmp(WinUtils::pathFindExtension(filePath).data(), L"exe") == 0) {
        // EXE
        std::string version;
        std::string data;
        switch (Synare::parseWinExecutable(filePath, &version, &data)) {
            case Synare::EXEVSNX_NotFound: {
                wprintf(L"%s: File is not infected, the virus version not found.\n", filePath.data());
                return 2;
            }
            case Synare::EXERESX_NotFound: {
                WinUtils::winConsoleColorScope(
                    [&]() {
                        wprintf(L"%s: File is infected, but the binary resource not found.\n", filePath.data()); //
                    },
                    WinUtils::Red);
                return 0;
            }
            case Synare::EXE_Failed: {
                wprintf(L"Error: %s: %s\n", filePath.data(), WinUtils::winLastErrorMessage().data());
                return -1;
            }
            case Synare::EXE_Disguised: {
                WinUtils::winConsoleColorScope(
                    [&]() {
                        wprintf(L"%s: This tool is disguised as being infected.\n", filePath.data()); //
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
                wprintf(L"%s: Successfully recover executable file, virus version %d.\n", filePath.data(),
                        std::atoi(version.data())); //
            },
            WinUtils::Green | WinUtils::Highlight);
    } else {
        // Other types
        wprintf(L"%s: This file type can not be infected.\n", filePath.data()); //
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
        // Open entry
        LSTATUS result = RegOpenKeyExW(HKEY_CURRENT_USER, reg.field, 0, KEY_WRITE, &hKey);
        if (result == ERROR_SUCCESS) {
            // Remove key
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
            wprintf(L"Sanitizing the processes, virus directory and registry entries.\n");
        },
        WinUtils::Yellow | WinUtils::Highlight);
    ;

    // Show warning if not running as Administrator
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
        // Walk through all processes, collect infected ones
        std::vector<WinUtils::ProcessInfo> processes;
        if (!WinUtils::walkThroughProcesses([&](const WinUtils::ProcessInfo &info, void *) -> bool {
                if (Synare::parseWinExecutable(info.path, nullptr, nullptr) & Synare::Infected) {
                    processes.push_back(info);
                }
                return false;
            })) {
            wprintf(L"Error: %s\n", WinUtils::winLastErrorMessage().data());
            return -1;
        }

        if (!processes.empty()) {
            // Terminate all infected
            for (const auto &p : std::as_const(processes)) {
                WinUtils::winConsoleColorScope(
                    [&]() {
                        wprintf(L"Killing process %-10ld %s\n", p.pid, p.path.data()); //
                    },
                    WinUtils::Red | WinUtils::Highlight);

                if (!WinUtils::killProcess(p.pid)) {
                    wprintf(L"Error: %s\n", WinUtils::winLastErrorMessage().data());
                    return -1;
                }

                // Wait for process terminated
                ::Sleep(50);
            }
        }
        wprintf(L"OK\n");
    }

    wprintf(L"\n");
    wprintf(L"[Step 2] Remove virus directories\n");
    {
        // Remove Synaptics directories
        if (!removeFileSystemVirus()) {
            wprintf(L"Error: %s\n", WinUtils::winLastErrorMessage().data());
            return -1;
        }
        wprintf(L"OK\n");
    }

    wprintf(L"\n");
    wprintf(L"[Step 3] Clean Registry\n");
    {
        // The virus will add itself to startup list in registry
        // Remove them all
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
    wprintf(L"    %-12s: Kill virus processes, remove virus directories and registry entries\n", L"Kill Mode");
    wprintf(L"    %-12s: Scan the given directory recursively, recover infected executables\n", L"Scan Mode");
    wprintf(L"    %-12s: Read the given file, output the original one if infected\n", L"Single Mode");
    wprintf(L"\n");
    wprintf(L"Options:\n");
    wprintf(L"    %-16s    Run in kill mode\n", L"-k");
    wprintf(L"    %-16s    Show this message\n", L"-h/--help");
    wprintf(L"    %-16s    Show version\n", L"-v/--version");
    wprintf(L"\n");

    WinUtils::winConsoleColorScope(
        []() {
            wprintf(L"Copyright SineStriker, checkout https://github.com/SineStriker/synaptics-recover\n"); //
        },
        WinUtils::Yellow | WinUtils::Highlight);
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

    const std::wstring &fileName = fileNames.front();

    DWORD attributes = GetFileAttributesW(fileName.data());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        wprintf(L"Error: %s: %s\n", fileName.data(), L"Invalid path.");
        return -1;
    }

    if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
        // Directory
        auto path = WinUtils::fixDirectoryPath(fileName);
        path = ::PathIsRelativeW(path.data()) ? WinUtils::getAbsolutePath(WinUtils::currentDirectory(), path)
                                              : WinUtils::getAbsolutePath(path, L".");

        // If the path is the system root, always run kill mode
        if (path == L"C:" || path == L"C:\\") {
            WinUtils::winConsoleColorScope(
                [&]() {
                    wprintf(L"The path is the system root, automatically run kill mode first.\n"); //
                },
                WinUtils::White | WinUtils::Highlight);
            wprintf(L"\n");

            int ret = doKill();
            if (ret != 0) {
                return ret;
            }
            wprintf(L"\n");
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