# Synaptics Recover

[**English**](./README.md) | [**中文简体**](./README_zh_CN.md)

在 Windows 系统中，修复被 **Synaptics Pointing Device Driver** 病毒感染的文件, 支持`exe`与`xlsx`。

## 用法

本程序是命令行工具，请查看帮助信息。

```sh
Command line tool to remove Synaptics Virus.

Usage: synaptics-recover [-k] [-h] [-v] [<dir>] [<input> [output]] [-d <N>]

Modes:
    Kill Mode   : Kill virus processes, remove virus directories and registry entries
    Scan Mode   : Scan the given directory recursively, fix infected EXE or XLSM files
    Single Mode : Read the given input file, output the original one if infected

Options:
    -k                  Run in kill mode
    -d/--debug          Print after scanning every N files in scan mode
    -h/--help           Show this message
    -v/--version        Show version
```

<!-- ## 不支持的功能

+ 无法处理长文件名 -->

## 范例

```sh
# 恢复单个文件
synaptics-recover infected.exe recovered.exe
synaptics-recover infected.xlsm recovered.xlsx

# 清除内存，文件系统与注册表中病毒
# 需要管理员权限
synaptics-recover -k

# 递归扫描一个文件夹，修复被感染的文件
# 推荐赋予管理员权限
synaptics-recover C:\
```

## 依赖

+ VC-LTL5
    + https://github.com/Chuyu-Team/VC-LTL5

+ pugixml
    + https://github.com/zeux/pugixml

+ Zippy

## 跨平台编译 (Linux 环境下交叉编译)

虽然本程序主要针对 Windows 环境下的病毒修复，但你完全可以在 Linux（如 Ubuntu）下通过 `mingw-w64` 将其交叉编译为独立的 Windows 绿色版可执行文件，以便修复 U 盘等挂载设备中受感染的文件。

### 1. 安装编译工具链
在 Debian/Ubuntu 系统下，安装 MinGW 交叉编译器：
```sh
sudo apt update
sudo apt install mingw-w64
```

### 2. 使用 CMake 构建静态绿色版
创建一个构建目录，并强制指定 MinGW 工具链与静态链接参数（确保生成的 `.exe` 摆脱对临时 `.dll` 文件的依赖，单枪匹马即可运行）：
```bash
mkdir build && cd build

cmake .. -DCMAKE_SYSTEM_NAME=Windows \
         -DCMAKE_C_COMPILER=i686-w64-mingw32-gcc \
         -DCMAKE_CXX_COMPILER=i686-w64-mingw32-g++ \
         -DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++ -static"

make
```
编译完成后，独立的 `synaptics-recover.exe` 将生成在 `build/bin/` 目录下。你可以在 Linux 下通过 wine 运行，或直接复制到 Windows 电脑上使用。

## ⚠️ Wine 环境运行说明

若直接在 Linux 的终端里通过 `wine` 运行本程序，由于 Wine 对 Windows 控制台特定编码页（如 `SetConsoleOutputCP`）和宽字符本地化输出的兼容性限制，帮助菜单的排版可能会出现字符残缺（被“吃掉”一部分）：
```sh
Command line tool to remove Synaptics Virus.

Usage: s [-k] [-h] [-v] [<dir>] [<input> [output]] [-d <N>]

Modes:
    K           : Kill virus processes, remove virus directories and registry entries
    S           : Scan the given directory recursively, fix infected EXE or XLSM files
    S           : Read the given input file, output the original one if infected

Options:
    -                   Run in kill mode
    -                   Print after scanning every N files in scan mode
    -                   Show this message
    -                   Show version

Copyright SineStriker, checkout https://github.com/SineStriker/synaptics-recover
```
注： 这仅为 Wine 终端环境下的显示兼容性瑕疵，程序的底层核心功能（如进程清理、文件扫描与去毒修复逻辑）完全正常，不受影响。在原生 Windows 环境下运行可正常显示完整菜单。

## 修复策略

### Exe

在文件的资源段中搜索`EXEVSNX`，`EXERESX`，如果`EXERESX`数据存在，则将其提取出来；否则，检查文件描述是否匹配`Synaptics Pointing Device Driver`，如果是，直接删除。

### Xlsx

搜索`xl/vbaProject.bin`入口，如果存在，则检查病毒下载链接是否存在于二进制数据中，如果存在，则按以下步骤修复。
+ 删除`xl/vbaProject.bin`入口
+ 在`[Content_Types].xml`中删除 vba project 内容类型并修改工作表的元类型
+ 在`xl/_rels/workbook.xml.rels`中删除 vba 相关数据
+ 设置`xl/workbook.xml`中所有工作表可见
+ 保存`xlsx`文件

## 注意事项 

### 本程序的伪装资源

该病毒仅感染 32 位 Windows 可执行文件。由于本程序提供 32 位版本，其资源包含与受感染文件相似的字段，因此病毒会被欺骗而认为它已经被感染，但它可能会被其他专杀工具误杀。

本程序不包含任何病毒代码片段，因此不会被杀毒软件检测。


### 关于修复 XLSX

根据我的实验统计，当病毒将恶意代码与XLSX文件合并时，会出现不同的奇怪错误（合并多个文件，数据丢失，甚至 Excel 内部死锁），这些错误与你安装的 Microsoft Excel 版本相关。本程序在扫描过程中可能会终止所有的 Excel 进程，请务必确保在运行本程序时没有正在编辑的 Excel 文档。

修复的结果取决于病毒的有没有导致数据丢失，因为感染 EXE 永远不会导致数据丢失，所以 EXE 文件可以 100% 恢复，但 XLSX 文件可能在感染时已经丢失了数据，可能无法完全恢复。


## 授权

源代码与 Synaptics 病毒高度相关，在其他项目中基本无用，因此以 GPL 3.0 发布。

https://github.com/SineStriker/synaptics-recover
