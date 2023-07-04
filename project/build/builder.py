from enum import Enum
from stats.lines import SourceFile, SourceLanguage
import consts
import os
import json


class BuilderMode(Enum):
    INVALID = -1
    KERNEL = 0
    USERSPACE = 1
    LIBRARIES = 2


class BuilderResult(Enum):
    FAIL = 0
    SUCCESS = 1
    WARNINGS = 2


class ProjectBuilder(object):

    builderMode: BuilderMode
    srcPath: list[str]
    constants: consts.Consts
    # userspaceBinariesOutPath: str

    def __init__(self, mode: BuilderMode, constants: consts.Consts) -> None:
        self.builderMode = mode
        self.constants = constants
        if mode == BuilderMode.KERNEL:
            self.srcPath = ["src/aniva", "src/drivers"]
        elif mode == BuilderMode.USERSPACE:
            self.srcPath = ["src/user", "src/libs"]
            # self.userspaceBinariesOutPath = constants.OUT_DIR + "/user/binaries"
            # self.constants.ensure_path(self.userspaceBinariesOutPath)
        elif mode == BuilderMode.LIBRARIES:
            self.srcPath = ["src/libs"]
        else:
            self.builderMode = BuilderMode.INVALID
        pass

    # Either build or link, based on the buildermode
    def do(self) -> BuilderResult:
        print("Building...")
        if self.build() == BuilderResult.FAIL:
            return BuilderResult.FAIL

        print("Linking...")
        return self.link()

    def build(self) -> BuilderResult:
        if self.builderMode == BuilderMode.KERNEL:
            for srcFile in self.constants.SRC_FILES:
                srcFile: SourceFile = srcFile
                if self.shouldBuild(srcFile):
                    if srcFile.language == SourceLanguage.C:
                        srcFile.setBuildFlags(self.constants.KERNEL_C_FLAGS)
                    elif srcFile.language == SourceLanguage.ASM:
                        srcFile.setBuildFlags(self.constants.KERNEL_ASM_FLAGS)
                    else:
                        return BuilderResult.FAIL

                    print(f"Building {srcFile.path}...")
                    os.system(f"mkdir -p {srcFile.getOutputDir()}")

                    if os.system(srcFile.getCompileCmd()) != 0:
                        return BuilderResult.FAIL
        else:
            return self.buildUserspace()
        return BuilderResult.SUCCESS

    # Build a userspace binary
    def buildUserspace(self) -> BuilderResult:
        for srcFile in self.constants.SRC_FILES:
            srcFile: SourceFile = srcFile
            if self.shouldBuild(srcFile):
                if srcFile.language == SourceLanguage.C:
                    srcFile.setBuildFlags(self.constants.USERSPACE_C_FLAGS)
                elif srcFile.language == SourceLanguage.ASM:
                    srcFile.setBuildFlags(self.constants.USERSPACE_ASM_FLAGS)

                print(f"Building {srcFile.path}...")
                os.system(f"mkdir -p {srcFile.getOutputDir()}")

                if os.system(srcFile.getCompileCmd()) != 0:
                    return BuilderResult.FAIL
        for crtFile in self.constants.CRT_FILES:
            crtFile: SourceFile = crtFile

            print(f"(crt) Building {crtFile.path}...")
            os.system(f"mkdir -p {crtFile.getOutputDir()}")

            if os.system(f"{crtFile.compilerDir} -c {crtFile.path} -o {crtFile.outputPath}") != 0:
                return BuilderResult.FAIL
        return BuilderResult.SUCCESS

    # Determine if we want to (re)build a sourcefile
    # based of the size of the file
    # TODO: make a system to cache filesizes
    def shouldBuildFile(self, file: SourceFile, cachedSize: int) -> bool:
        newSize: int = os.path.getsize(file.path)
        if (newSize != cachedSize):
            return True
        return False

    def link(self) -> BuilderResult:
        self.constants.reinit()

        SDN = self.constants.SRC_DIR_NAME
        ODN = self.constants.OUT_DIR_NAME

        KEP = self.constants.KERNEL_ELF_PATH
        KLF = self.constants.KERNEL_LD_FLAGS

        if self.builderMode == BuilderMode.KERNEL:

            objFiles: str = " "

            for objFile in self.constants.OBJ_FILES:
                objFile: str = objFile

                for builderPath in self.srcPath:
                    builderPath = builderPath.replace(SDN, ODN)

                    if (objFile.find(builderPath) != -1):
                        # Pad the end with a space
                        objFiles += f"{objFile} "
                        break

            ld = self.constants.CROSS_LD_DIR

            if os.system(f"{ld} -o {KEP} {objFiles} {KLF}") == 0:
                return BuilderResult.SUCCESS

            return BuilderResult.FAIL
        elif self.builderMode == BuilderMode.USERSPACE:
            # For each directory in the src/user directory
            # we should link a binary
            userDir: str = self.constants.SRC_DIR + "/user"

            for entry in os.listdir(userDir):

                entryName: str = entry
                entry: str = f"{userDir}/{entry}"

                # Skip anything that is not a process direcotry
                if not os.path.isdir(entry):
                    continue

                for procEntry in os.listdir(entry):
                    if procEntry == "manifest.json":
                        procEntry: str = f"{entry}/{procEntry}"

                        with open(procEntry, "r") as manifestFile:
                            manifest = json.load(manifestFile)

                            # TODO: check for dependencies
                            if manifest["name"] == entryName:
                                objFiles: str = " "
                                print(f"Building process: {entryName}")
                                print(f"Binary out path: {self.constants.OUT_DIR}/user/{entryName}")

                                BIN_OUT_PATH = f"{self.constants.OUT_DIR}/user/{entryName}"
                                BIN_OUT = BIN_OUT_PATH + "/" + entryName

                                ULF = self.constants.USERSPACE_LD_FLAGS

                                for objFile in self.constants.OBJ_FILES:
                                    objFile: str = objFile

                                    # Add objectfiles from the processes directory
                                    if objFile.find(BIN_OUT_PATH) != -1:
                                        objFiles += f"{objFile} "
                                    # Add objectfiles from libraries (TODO: Limit to libraries in the manifest)
                                    elif objFile.find(self.constants.LIBS_OUT_DIR) != -1:
                                        objFiles += f"{objFile} "

                                ld = self.constants.CROSS_LD_DIR

                                if os.system(f"{ld} -o {BIN_OUT} {objFiles} {ULF}") != 0:
                                    return BuilderResult.FAIL

                                # TODO: we should check the manifest.json for
                                # the libraries we need to staticaly link
                                # with. After that we dump the binary to
                                # the output directory
                                # (currently out/user/binaries)
            return BuilderResult.SUCCESS

    def clean(self) -> None:
        pass

    def shouldBuild(self, file: SourceFile) -> bool:
        for path in self.srcPath:
            # The path of this file is one that we care about
            if file.get_path().find(path) != -1 and file.is_header is False:
                # Without a compiler or output path, we are
                # not really able to do much, are we...
                if file.compilerDir == "" or file.outputPath == "":
                    return False
                return True
        return False
