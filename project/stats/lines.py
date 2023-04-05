
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
    path: str = ""
    language: SourceLanguage = SourceLanguage.NONE
    compilerDir: str
    buildFlags: str

    def __init__(self, is_header: bool, path: str, language: SourceLanguage) -> None:
        self.is_header = is_header
        self.path = path
        self.language = language
        self.compilerDir = ""
        self.buildFlags = ""

    def get_path(self) -> str:
        return self.path

    def set_compiler(self, path: str) -> None:
        self.compilerDir = path

    def setBuildFlags(self, flags: str) -> None:
        self.buildFlags = flags

    def getCompileCmd(self) -> str:
        if self.is_header:
            return "echo Tried to build header! Ignoring..."
        return f"{self.compilerDir} {self.path} {self.buildFlags}"
