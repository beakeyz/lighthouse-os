import os
from os.path import isdir, isfile
from stats.lines import SourceFile, SourceLanguage


class Consts:
    PROJECT_NAME: str = "lighthouse-os"
    KERNEL_NAME: str = "Aniva"

    PROJECT_DIR = str(os.path.abspath(""))

    SRC_DIR_NAME = "src"
    OUT_DIR_NAME = "out"

    SRC_DIR = PROJECT_DIR + "/" + SRC_DIR_NAME
    OUT_DIR = PROJECT_DIR + "/" + OUT_DIR_NAME
    PROJECT_MANAGEMENT_DIR = PROJECT_DIR + "/project"
    COMPILER_DIR = PROJECT_DIR + "/cross_compiler/bin"

    KERNEL_MAP_PATH = OUT_DIR + "/lightos.map"
    KERNEL_ELF_PATH = OUT_DIR + "/lightos.elf"
    KERNEL_LINKERSCRIPT_PATH = PROJECT_DIR + "/src/aniva/entry/linker.ld"

    CROSS_GCC_DIR = COMPILER_DIR + "/x86_64-pc-lightos-gcc"
    CROSS_LD_DIR = COMPILER_DIR + "/x86_64-pc-lightos-ld"
    CROSS_ASM_DIR = "/usr/bin/nasm"

    # TODO:
    DEFAULT_C_FLAGS = ""

    # Default kernel flags
    KERNEL_C_FLAGS = "-std=gnu11 -Wall -nostdlib -O2 -mno-sse -mno-sse2"
    KERNEL_C_FLAGS += " -mno-mmx -mno-80387 -mno-red-zone -m64 -march=x86-64 -mcmodel=large"
    KERNEL_C_FLAGS += " -ffreestanding -fno-stack-protector -fno-stack-check -fshort-wchar"
    KERNEL_C_FLAGS += " -fno-lto -fpie -fno-exceptions -MMD -I./src -I./src/aniva/"
    KERNEL_C_FLAGS += " -D\'KERNEL\'"

    # Default libenv flags (aka the aniva equivilant of libc)
    LIBENV_C_FLAGS = "-std=gnu11 -Wall -O2 -ffreestanding -fPIC"
    LIBENV_C_FLAGS += " -D\'LIBENV\'"

    # TODO: expand
    # Default userspace flags (just anything that isn't the kernel basically)
    USERSPACE_C_FLAGS = "-std=gnu11 -Wall -O2 -ffreestanding -I./src/libs"
    USERSPACE_C_FLAGS += " -D\'USER\'"

    KERNEL_ASM_FLAGS = " -f elf64"

    KERNEL_LD_FLAGS = f" -T {KERNEL_LINKERSCRIPT_PATH} -Map {KERNEL_MAP_PATH} -z max-page-size=0x1000"

    CRT_FILES: list[str] = []
    SRC_FILES: list[SourceFile] = []
    OBJ_FILES = []
    SRC_LINES: list[int] = []

    TOTAL_LINES = 0

    def __init__(self) -> None:
        self.SRC_FILES = []
        self.OBJ_FILES = []
        self.SRC_LINES = []
        self.CRT_FILES = []

        self.TOTAL_LINES = 0

        if not self.PROJECT_DIR.endswith(self.PROJECT_NAME):
            print("Please run this script from the project root!")
            exit(-1)

        self.ensure_path(self.OUT_DIR)
        self.ensure_path(self.SRC_DIR)
        self.ensure_path(self.PROJECT_MANAGEMENT_DIR)

        self.scan_dirs(self.SRC_DIR)
        self.scan_dirs(self.OUT_DIR)
        self.scan_dirs(self.PROJECT_MANAGEMENT_DIR)

        for _ in SourceLanguage:
            self.SRC_LINES.append(0)

        for file in self.SRC_FILES:
            with open(file.path) as f:
                lines: int = len(f.readlines())
                self.TOTAL_LINES += lines
                self.SRC_LINES[int(file.language)] += lines

    def reinit(self) -> None:
        self.__init__()

    def scan_dirs(self, path) -> None:
        dirs: list[str] = os.listdir(path)
        for entry in dirs:
            if entry.startswith("."):
                continue

            abs_entry: str = path + "/" + entry

            if isdir(abs_entry):
                self.scan_dirs(abs_entry)
            elif isfile(abs_entry):
                # CRTs are special, so we handle them first
                if entry.startswith("crt"):
                    self.CRT_FILES.append(abs_entry)
                elif entry.endswith(".c"):
                    file = SourceFile(False, abs_entry, entry, SourceLanguage.C)
                    file.set_compiler(self.CROSS_GCC_DIR)
                    file.setOutputPath(file.get_path().replace(self.SRC_DIR, self.OUT_DIR))
                    self.SRC_FILES.append(file)
                if entry.endswith(".py"):
                    file = SourceFile(False, abs_entry, entry, SourceLanguage.PYTHON)
                    self.SRC_FILES.append(file)
                elif entry.endswith(".h"):
                    file = SourceFile(True, abs_entry, entry, SourceLanguage.C)
                    self.SRC_FILES.append(file)
                elif entry.endswith(".asm"):
                    file = SourceFile(False, abs_entry, entry, SourceLanguage.ASM)
                    file.set_compiler(self.CROSS_ASM_DIR)
                    file.setOutputPath(file.get_path().replace(self.SRC_DIR, self.OUT_DIR))
                    self.SRC_FILES.append(file)
                elif entry.endswith(".md"):
                    file = SourceFile(False, abs_entry, entry, SourceLanguage.MARKDOWN)
                    self.SRC_FILES.append(file)
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

    def ensure_path(self, path: str) -> int:
        return os.system(f"mkdir -p {path}")
