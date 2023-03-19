
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

    def __init__(self, is_header: bool, path: str, language: SourceLanguage) -> None:
        self.is_header = is_header
        self.path = path
        self.language = language

    def get_path(self) -> str:
        return self.path
