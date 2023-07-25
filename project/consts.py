import os
from os.path import isdir, isfile
from stats.lines import SourceFile, SourceLanguage

def take_input(string: str) -> str:
    '''
    Generic routine we can use to generate a consistent input prompt
    '''

    if string is None:
        return None

    return input(f"({string}) > ")


class Consts:
    PROJECT_NAME: str = "lighthouse-os"
    KERNEL_NAME: str = "aniva"

    PROJECT_DIR = str(os.path.abspath(""))

    # Path where we can store our configuration
    PROJECT_DATA_DIR = PROJECT_DIR + "/.data"

    SRC_DIR_NAME = "src"
    OUT_DIR_NAME = "out"

    SRC_DIR = PROJECT_DIR + "/" + SRC_DIR_NAME
    OUT_DIR = PROJECT_DIR + "/" + OUT_DIR_NAME
    LIBS_OUT_DIR = OUT_DIR + "/libs"
    PROJECT_MANAGEMENT_DIR = PROJECT_DIR + "/project"
    COMPILER_DIR = PROJECT_DIR + "/cross_compiler/bin"

    KERNEL_MAP_PATH = OUT_DIR + "/lightos.map"
    KERNEL_ELF_PATH = OUT_DIR + "/lightos.elf"
    KERNEL_LINKERSCRIPT_PATH = PROJECT_DIR + "/src/aniva/entry/linker.ld"
    USERSPACE_DEFAULT_LDSCRPT_PATH = PROJECT_DIR + "/src/user/linker.ld"

    CROSS_GCC_DIR = COMPILER_DIR + "/x86_64-pc-lightos-gcc"
    CROSS_LD_DIR = COMPILER_DIR + "/x86_64-pc-lightos-ld"
    CROSS_ASM_DIR = "/usr/bin/nasm"

    # Libraries that should always be linked staticly
    LIBENV_ALWAYS_STATIC_LIBS = ["libs/LibC", "libs/LibEnv"]

    # TODO:
    DEFAULT_C_FLAGS = ""

    # Default kernel flags
    # TODO: implement -Wpedantic 
    KERNEL_C_FLAGS = "-std=gnu11 -Werror -Wall -nostdlib -O2 -mno-sse -mno-sse2"
    KERNEL_C_FLAGS += " -mno-mmx -mno-80387 -mno-red-zone -m64 -march=x86-64 -mcmodel=large"
    KERNEL_C_FLAGS += " -ffreestanding -fno-stack-protector -fno-stack-check -fshort-wchar"
    KERNEL_C_FLAGS += " -fno-lto -fpie -fno-exceptions -MMD -I./src -I./src/aniva/ -I./src/libs"
    KERNEL_C_FLAGS += " -D\'KERNEL\'"

    # Default libenv flags (aka the aniva equivilant of libc)
    LIBENV_C_FLAGS = "-std=gnu11 -Wall -O2 -ffreestanding -fPIC -shared"
    LIBENV_C_FLAGS += " -D\'LIBENV\'"

    # TODO: implement compat with external kernel drivers as modules (which are treated as userspace until they are discovered to be drivers)
    # Default userspace flags (just anything that isn't the kernel basically)
    USERSPACE_C_FLAGS = "-std=gnu11 -Wall -fPIC -O2 -ffreestanding -I./src/libs -I./src/libs/LibC"
    USERSPACE_C_FLAGS += " -D\'USER\'"

    # Extention for the userspace CFlags to include kernel headers
    USERSPACE_C_FLAGS_KRNL_INCLUDE_EXT = " -I./src/aniva"

    KERNEL_ASM_FLAGS = " -f elf64"
    USERSPACE_ASM_FLAGS = " -f elf64"

    KERNEL_LD_FLAGS = f" -T {KERNEL_LINKERSCRIPT_PATH} -Map {KERNEL_MAP_PATH} -z max-page-size=0x1000"

    USERSPACE_LD_FLAGS = f" -T {USERSPACE_DEFAULT_LDSCRPT_PATH}"

    LIB_LD_FLAGS = " -shared -fPIC"

    ELF_EXTENTION = ".elf"
    SHARED_LIB_EXTENTION = ".slb" # Shared Library Binary

    # NOTE: crt files have to be asm files!
    CRT_FILES: list[SourceFile] = []
    SRC_FILES: list[SourceFile] = []
    OBJ_FILES = []
    KERNEL_OBJ_FILES = []
    SRC_LINES: list[int] = []

    USER_PROJECT_PATHS: list[str] = []
    LIBS_PROJECT_PATHS: list[str] = []

    TOTAL_LINES = 0

    def __init__(self) -> None:
        self.SRC_FILES = []
        self.OBJ_FILES = []
        self.KERNEL_OBJ_FILES = []
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

        userSrcDir = f"{self.SRC_DIR}/user" 

        # Scan the entire user source dir
        completeContent = os.listdir(userSrcDir)

        # Add every directory, since that is the only place where userprograms should be
        # TODO: recursive and check for manifest.json?
        for path in completeContent:
            absPath: str = f"{userSrcDir}/{path}"
            if isdir(absPath):
                self.USER_PROJECT_PATHS.append(absPath)

        libSrcDir = f"{self.SRC_DIR}/libs" 

        # Scan the entire user source dir
        completeContent = os.listdir(libSrcDir)

        # Add every directory, since that is the only place where libraries should be
        for path in completeContent:
            absPath: str = f"{libSrcDir}/{path}"
            if isdir(absPath):
                self.LIBS_PROJECT_PATHS.append(absPath)

        for _ in range(7):
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
                if entry.startswith("crt") and (entry.endswith(".S") or entry.endswith(".s")):
                    file = SourceFile(False, abs_entry, entry, SourceLanguage.ASM)
                    file.set_compiler(self.CROSS_GCC_DIR)
                    file.setOutputPath(file.get_path().replace(self.SRC_DIR, self.OUT_DIR))
                    self.CRT_FILES.append(file)
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
                    if abs_entry.find(f"{self.OUT_DIR}/{self.KERNEL_NAME}") != -1:
                        self.KERNEL_OBJ_FILES.append(abs_entry)

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
