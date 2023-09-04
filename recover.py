from __future__ import annotations

import os
import sys
import subprocess
import datetime
from colorama import Fore, Style

current_time = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
log_file_name = f"log_{current_time}.txt"


def execute_command(exe_path):
    process = subprocess.run(
        ["synaptics-recover.exe", exe_path, exe_path],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        text=True)

    if process.returncode == 0:
        print(f"{Fore.RED}{exe_path}{Style.RESET_ALL}")
    else:
        print(f"{exe_path}")

    return process.returncode


def scan_and_execute(folder_path):
    with open(log_file_name, "w") as log:
        for root, _, files in os.walk(folder_path):
            for file in files:
                if file.lower().endswith(".exe"):
                    exe_path = os.path.join(root, file)
                    if execute_command(exe_path) == 0:
                        log.write(f"{exe_path}\n")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: python {os.path.basename(__file__)} <dir>")
        sys.exit(1)

    folder_path = sys.argv[1]

    if not os.path.exists(folder_path):
        print(f"The folder '{folder_path}' does not exist.")
        sys.exit(1)

    scan_and_execute(folder_path)
