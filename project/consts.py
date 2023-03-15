import os
from os.path import isdir, isfile
from stats.lines import *

class Consts:
    PROJECT_NAME: str = "lighthouse-os"
    KERNEL_NAME: str = "Aniva"

    PROJECT_DIR = str(os.path.abspath(""))

    SRC_DIR = PROJECT_DIR + "/src"
    PROJECT_MANAGEMENT_DIR = PROJECT_DIR + "/project"

    SRC_FILES: list[SourceFile] = []
    OBJ_FILES = []
    SRC_LINES: list[int] = []

    TOTAL_LINES = 0

    def __init__(self) -> None:

        if not self.PROJECT_DIR.endswith(self.PROJECT_NAME):
            print("Please run this script from the project root!")
            exit(-1)

        self.scan_dirs(self.SRC_DIR)
        self.scan_dirs(self.PROJECT_MANAGEMENT_DIR)

        for _ in SourceLanguage:
            self.SRC_LINES.append(0)

        for file in self.SRC_FILES:
            with open(file.path) as f:
                lines: int = len(f.readlines())
                self.TOTAL_LINES += lines 
                self.SRC_LINES[int(file.language)] += lines

    
    def scan_dirs(self, path) -> None:
        dirs: list[str] = os.listdir(path)
        for entry in dirs:
            if entry.startswith("."):
                continue

            abs_entry:str = path + "/" + entry

            if isdir(abs_entry):
                self.scan_dirs(abs_entry)
            elif isfile(abs_entry):
                if entry.endswith(".c"):
                    self.SRC_FILES.append(SourceFile(False, abs_entry, SourceLanguage.C))
                if entry.endswith(".py"):
                    self.SRC_FILES.append(SourceFile(False, abs_entry, SourceLanguage.PYTHON))
                elif entry.endswith(".h"):
                    self.SRC_FILES.append(SourceFile(True, abs_entry, SourceLanguage.C))
                elif entry.endswith(".md"):
                    self.SRC_FILES.append(SourceFile(False, abs_entry, SourceLanguage.MARKDOWN))
                elif entry.endswith(".o"):
                    self.OBJ_FILES.append(abs_entry)

    def log_source(self) -> None:
        print("Logging source files: ")

        index: int = 0
        for line_count in self.SRC_LINES:
            if SourceLanguage(index) == SourceLanguage.NONE:
                index += 1
                continue

            percentage: float = round(line_count / self.TOTAL_LINES * 100, 1)

            print(f" - Found {line_count} lines of " + str(SourceLanguage(index)) + f" which is {percentage}% of the project")
            index += 1

    def draw_source_bar(self) -> None:
        BAR_CHARS: int = 125

        index: int = 0
        print("")
        print("Bar representing the usage of languages in the project: ")
        print("[", end="")
        for line_count in self.SRC_LINES:
            if SourceLanguage(index) == SourceLanguage.NONE:
                index += 1
                continue

            percentage: float = line_count / self.TOTAL_LINES

            char_count: int = int(round(BAR_CHARS * percentage, 0))

            for i in range(char_count):
                print(str(SourceLanguage(index))[0], end="")

            index += 1

        print("]", end="")
        print("")
