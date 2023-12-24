from enum import Enum, IntFlag


class SourceLanguage(IntFlag, Enum):
    NONE = 0,
    C = 1,
    CPP = 2,
    ASM = 3,
    PYTHON = 4,
    SHELL_SCRIPT = 5,
    MARKDOWN = 6,

    def __int__(self) -> int:
        return self._value_

    def __str__(self) -> str:
        if self == SourceLanguage.C:
            return "C"
        elif self == SourceLanguage.CPP:
            return "C++"
        elif self == SourceLanguage.PYTHON:
            return "Python"
        elif self == SourceLanguage.SHELL_SCRIPT:
            return "Shell script"
        elif self == SourceLanguage.MARKDOWN:
            return "Markdown"
        elif self == SourceLanguage.ASM:
            return "Assembly"
        else:
            return "None"


class SourceFile:

    is_header: bool = False
    path: str
    fileName: str
    language: SourceLanguage = SourceLanguage.NONE
    compilerDir: str
    outputPath: str
    buildFlags: str

    def __init__(self, is_header: bool, path: str, fileName: str, language: SourceLanguage) -> None:
        self.is_header = is_header
        self.path = path
        self.language = language
        self.fileName = fileName
        self.compilerDir = ""
        self.buildFlags = ""
        self.outputPath = ""

    def get_path(self) -> str:
        return self.path

    def set_compiler(self, path: str) -> None:
        self.compilerDir = path

    def setBuildFlags(self, flags: str) -> None:
        self.buildFlags = flags

    def setOutputPath(self, path: str) -> None:
        path = self.addObjectSuffix(path)
        self.outputPath = path

    def addObjectSuffix(self, path: str) -> str:
        # C suffixes
        path = path.replace(".c", ".o")

        # ASM suffixes
        path = path.replace(".asm", ".o")
        path = path.replace(".S", ".o")
        path = path.replace(".s", ".o")

        # CPP suffixes
        path = path.replace(".cpp", ".o")
        return path

    def getOutputDir(self) -> str:
        path = self.outputPath
        filename = self.fileName
        suffix = self.addObjectSuffix(filename)
        path = path.removesuffix(suffix)
        return path

    def getCompileCmd(self) -> str:
        if self.is_header:
            return "echo Tried to build header! Ignoring..."

        if self.language == SourceLanguage.C:
            return f"{self.compilerDir} -c {self.buildFlags} {self.path} -o {self.outputPath}"

        if self.language == SourceLanguage.ASM:
            return f"{self.compilerDir} {self.path} -o {self.outputPath} {self.buildFlags}"
