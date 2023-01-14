import io
from operator import contains
import os
from os.path import isdir, isfile

class Consts:
    PROJECT_NAME: str = "lighthouse-os"
    KERNEL_NAME: str = "Aniva"

    SRC_DIR = str(os.environ.get("PWD")) + "/../src"

    SRC_FILES = []
    HEADER_FILES = []
    OBJ_FILES = []

    TOTAL_LINES = 0

    def __init__(self) -> None:
        self.scan_dirs(self.SRC_DIR)

        if not self.SRC_DIR.__contains__(self.PROJECT_NAME):
            print("ERROR: project is in an arbitrary directory!")

        for file in self.SRC_FILES:
            with open(file) as f:
                self.TOTAL_LINES += len(f.readlines())

        for file in self.HEADER_FILES:
            with open(file) as f:
                self.TOTAL_LINES += len(f.readlines())
    
    def scan_dirs(self, path):
        dirs: list[str] = os.listdir(path)
        for entry in dirs:
            if entry.startswith("."):
                continue

            abs_entry:str = path + "/" + entry

            if isdir(abs_entry):
                self.scan_dirs(abs_entry)
            elif isfile(abs_entry):
                if entry.endswith(".c"):
                    self.SRC_FILES.append(abs_entry)
                elif entry.endswith(".h"):
                    self.HEADER_FILES.append(abs_entry)
                elif entry.endswith(".o"):
                    self.OBJ_FILES.append(abs_entry)


c = Consts()

print(c.TOTAL_LINES)
print(c.SRC_FILES)
