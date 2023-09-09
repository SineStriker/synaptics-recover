# Synaptics Recover

[**English**](./README.md) | [**中文简体**](./README_zh_CN.md)

Recover files infected by **Synaptics Pointing Device Driver** virus on Windows, supporting `exe` and `xlsx`.

## Usage

This program is a command line tool, checkout the help information.

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

## Vulnerabilities

+ Cannot handle long file names

## Examples

```sh
# Recover a file
synaptics-recover infected.exe recovered.exe
synaptics-recover infected.xlsm recovered.xlsx

# Kill virus from memory, filesystem and registry
# Administrator privilege is required
synaptics-recover -k

# Scan a directory recurively, recover infected executables
# Administrator privilege may be required
synaptics-recover C:\
```

## Dependencies

+ VC-LTL5
    + https://github.com/Chuyu-Team/VC-LTL5

+ pugixml
    + https://github.com/zeux/pugixml

+ Zippy

## Recover Strategy

### Exe

Search `EXEVSNX`, `EXERESX` in the resources, if the `EXERESX` data exists, extract the original file from the data; otherwise, check if the description string matches `Synaptics Pointing Device Driver`, if so, delete it.

### Xlsx

Search `xl/vbaProject.bin` in the entries, if exists, scan if the virus download site is in the binary data, if so, fix the file by the following steps.
+ Remove `xl/vbaProject.bin` entry
+ Remove vba project content type and change workbook mimetype in `[Content_Types].xml`
+ Remove vba related data in `xl/_rels/workbook.xml.rels`
+ Set all sheets visible in `xl/workbook.xml`
+ Save as `xlsx` file

## Notice

### Disguise In This Program

The virus only infects 32-bit Windows Executable. Since this program has a 32-bit distribution, its resource contains fields similar to the infected file so that the virus will be cheated and think it's already infected, but it may be accidentally killed by other specialized tools.

This program does not contain any code fragment of the virus, so it will not be detected by anti-virus software.

### About Repairing XLSX

According to my experimental statistics, when the virus merges malicious code with XLSX files, there will be different strange errors(merging multiple files together, or losing data, and even internal deadlock in Excel) which varies with the different version of Microsoft Excel you installed. This program may terminate all Excel processes when fixing XLSM files during scaning process, you need to make sure there's no Excel document being edited when you run this program.

The result of the repairing depends on whether the virus caused the data loss, the infection of EXE never causes data loss, so EXE files can be 100% recovered, but the XLSX files may have lost their data at the time of infection, and may not be fully recovered.

## License

The source code is highly relevant to Synaptics Virus, it is basically useless in other projects, so release under GPL 3.0.

https://github.com/SineStriker/synaptics-recover