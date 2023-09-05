# Synaptics Recover

Recover Windows Executable files infected by Synaptics Pointing Device Driver virus.

## Usage

```sh
Command line tool to kill Synaptics Virus.
Usage: synaptics-recover [options] [<input> [output]]

Options:
    -k                  Scan processes, program data and registry to kill the virus
    -s <dir>            Scan a directory recursively, recover infected executables
    -h/--help           Show this message
    -v/--version        Show version

If no option is specified, this program will try to recover the input executable file.
```

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
synaptics-recover -s C:
```