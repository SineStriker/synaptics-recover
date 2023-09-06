# Synaptics Recover

Recover files infected by **Synaptics Pointing Device Driver** virus.

## Usage

```sh
Command line tool to remove Synaptics Virus.

Usage: synaptics-recover [-k] [-h] [-v] [<dir>] [<input> [output]]

Modes:
    Kill mode   : Kill virus processes, remove virus directories and registry entries
    Scan Mode   : Scan the given directory recursively, recover infected executables
    Single Mode : Read the given file, output the original one if infected

Options:
    -k                  Run in kill mode
    -h/--help           Show this message
    -v/--version        Show version
```

## Vulnerabilities

+ Cannot handle long file names
+ XLSM not implemented

## Examples

```sh
# Recover a file
synaptics-recover infected.exe infected.exe

# Recover a file but reserve the infected one
synaptics-recover infected.exe recovered.exe

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

+ OpenXLSX
    + https://github.com/troldal/OpenXLSX