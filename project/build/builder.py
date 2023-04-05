from enum import Enum
from stats.lines import SourceFile
import consts
import os


class BuilderMode(Enum):
    INVALID = -1
    KERNEL = 0
    USERSPACE = 1
    LINK = 2


class BuilderResult(Enum):
    FAIL = 0
    SUCCESS = 1
    WARNINGS = 2


class ProjectBuilder(object):

    builderMode: BuilderMode
    srcPath: list[str]
    constants: consts.Consts

    def __init__(self, mode: BuilderMode, constants: consts.Consts) -> None:
        self.builderMode = mode
        self.constants = constants
        if mode == BuilderMode.KERNEL:
            self.srcPath = ["src/aniva", "src/drivers", "src/libs"]
        elif mode == BuilderMode.USERSPACE:
            self.srcPath = ["src/user", "src/libs"]
        else:
            self.builderMode = BuilderMode.INVALID
        pass

    # Either build or link, based on the buildermode
    def do(self) -> BuilderResult:
        if self.builderMode == BuilderMode.LINK:
            return self.link()

        return self.build()

    def build(self) -> BuilderResult:
        for srcFile in self.constants.SRC_FILES:
            if self.shouldBuild(srcFile):
                if self.builderMode == BuilderMode.KERNEL:
                    srcFile.setBuildDir(self.constants.KERNEL_C_FLAGS)
                print(f"Building {srcFile.path}...")
                os.system(srcFile.getCompileCmd())
        return BuilderResult.FAIL

    def link(self) -> BuilderResult:
        self.constants.reinit()
        pass

    def clean(self) -> None:
        pass

    def shouldBuild(self, file: SourceFile) -> bool:
        for path in self.srcPath:
            # The path of this file is one that we care about
            if file.get_path().find(path) != -1 and file.is_header is False:
                if file.compilerDir == "":
                    return False
                return True
        return False
