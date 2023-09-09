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

## 不支持的功能

+ 无法处理长文件名

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