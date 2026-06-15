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

<!-- ## Vulnerabilities

+ Cannot handle long file names -->

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

## Cross-Platform Compilation (Cross-Compilation under Linux)

Although this program is primarily designed for virus repair in Windows environments, you can cross-compile it into a standalone Windows portable executable file using `mingw-w64` on Linux (such as Ubuntu) to repair infected files on mounted devices like USB drives.

### 1. Install the Compiler Toolchain

On Debian/Ubuntu systems, install the MinGW cross-compiler:

```sh
sudo apt update
sudo apt install mingw-w64
```

### 2. Build a Static Portable Version using CMake

Create a build directory and force-specify the MinGW toolchain and static linking parameters (ensuring the generated `.exe` is free from dependencies on temporary `.dll` files and can run independently):

```bash
mkdir build && cd build

cmake .. -DCMAKE_SYSTEM_NAME=Windows \
-DCMAKE_C_COMPILER=i686-w64-mingw32-gcc \
-DCMAKE_CXX_COMPILER=i686-w64-mingw32-g++ \
-DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++ -static"

make
```
After compilation, a standalone `synaptics-recover.exe` will be generated in the `build/bin/` directory. You can run it via Wine on Linux, or copy it directly to your Windows computer.

## ⚠️ Wine Environment Operation Instructions

If you run this program directly in the Linux terminal using `wine`, due to Wine's compatibility limitations with Windows console-specific encoding pages (such as `SetConsoleOutputCP`) and wide character localization output, the help menu layout may show incomplete characters (partially "eaten up"):

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

Note: This is only a display compatibility issue in the Wine terminal environment. The program's underlying core functions (such as process cleanup, file scanning, and virus removal and repair logic) are completely normal and unaffected. The full menu displays correctly when running in the native Windows environment.

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
