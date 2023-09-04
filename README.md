# Synaptics Recover

Recover a Windows Executable infected by Synaptics Pointing Device Driver virus.

## Usage

Make a directory, place `recover.py` and `synaptics-recover.exe` into the directory.

```sh
python recover.py <dir>
```

The script will search the directory recursively for `.exe` files and call `synaptics-recover` for each, and generate a log file listing all infected ones.