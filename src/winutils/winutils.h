#ifndef WINUTILS_H
#define WINUTILS_H

#include <functional>
#include <string>

namespace WinUtils {

    std::vector<std::wstring> commandLineArguments();

    std::wstring winErrorMessage(uint32_t error, bool nativeLanguage = false);

    std::wstring winLastErrorMessage(bool nativeLanguage = false, uint32_t *code = nullptr);

    bool writeFile(const std::wstring &fileName, const std::string &data);

    std::wstring fixDirectoryPath(const std::wstring &path);

    bool removeDirectoryRecursively(const std::wstring &dir);

    bool removeFile(const std::wstring &fileName);

    bool walkThroughDirectory(const std::wstring &dir, const std::function<bool(const std::wstring &)> &func,
                              bool strict = false);

    std::wstring pathFindFileName(const std::wstring &path);

    std::wstring pathFindBaseName(const std::wstring &path);

    std::wstring pathFindDirectory(const std::wstring &path);

    std::wstring pathFindExtension(const std::wstring &path);

    bool pathIsFile(const std::wstring &path);

    std::wstring appFilePath();

    std::wstring appDirectory();

    std::wstring appName();

    std::wstring currentDirectory();

    enum ConsoleColor {
        Red = 1,
        Blue = 2,
        Green = 4,
        Yellow = Red | Green,
        White = Yellow | Blue,
        Highlight = 8,
    };
    void winConsoleColorScope(const std::function<void()> &func, int color);

    void winClearConsoleLine();

    struct ProcessInfo {
        std::wstring path;
        uint32_t pid;
    };
    bool walkThroughProcesses(const std::function<bool(const ProcessInfo &, void *)> &func);

    bool killProcess(uint32_t pid);

    std::wstring getPathEnv(const wchar_t *key);

    std::wstring getAbsolutePath(const std::wstring &basePath, const std::wstring &relativePath);

    std::wstring getCanonicalPath(const std::wstring &path);

    std::wstring strMulti2Wide(const std::string &bytes);

    std::string strWide2Multi(const std::wstring &str);

}

#endif // WINUTILS_H
