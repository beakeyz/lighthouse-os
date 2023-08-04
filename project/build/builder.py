from enum import Enum
from stats.lines import SourceFile, SourceLanguage
import build.manifest
import build.symbols
import consts
import os
import json


class BuilderMode(Enum):
    INVALID = -1
    KERNEL = 0
    USERSPACE = 1
    LIBRARIES = 2
    DRIVERS = 3


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
            self.srcPath = ["src/aniva"]
        elif mode == BuilderMode.USERSPACE:
            self.srcPath = ["src/user"]
            # self.userspaceBinariesOutPath = constants.OUT_DIR + "/user/binaries"
            # self.constants.ensure_path(self.userspaceBinariesOutPath)
        elif mode == BuilderMode.LIBRARIES:
            self.srcPath = ["src/libs"]
        elif mode == BuilderMode.DRIVERS:
            self.srcPath = ["src/drivers"]
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

    def buildKernelSourceFile(self, file: SourceFile) -> int:
        if file.language == SourceLanguage.C:
            file.setBuildFlags(self.constants.KERNEL_C_FLAGS)
        elif file.language == SourceLanguage.ASM:
            file.setBuildFlags(self.constants.KERNEL_ASM_FLAGS)
        else:
            return -1 
        return os.system(file.getCompileCmd())

    def build(self) -> BuilderResult:
        if self.builderMode == BuilderMode.KERNEL or self.builderMode == BuilderMode.DRIVERS:
            for srcFile in self.constants.SRC_FILES:
                srcFile: SourceFile = srcFile
                if self.shouldBuild(srcFile):

                    print(f"Building {srcFile.path}...")
                    os.system(f"mkdir -p {srcFile.getOutputDir()}")

                    if self.buildKernelSourceFile(srcFile) != 0:
                        return BuilderResult.FAIL
            return BuilderResult.SUCCESS
        elif self.builderMode == BuilderMode.USERSPACE:
            return self.buildUserspace()
        elif self.builderMode == BuilderMode.LIBRARIES:
            return self.buildLibraries()

        return BuilderResult.FAIL

    def buildLibraries(self) -> BuilderResult:
        if (self.builderMode != BuilderMode.LIBRARIES):
            return BuilderResult.FAIL

        # Just treat every library project path entry as a seperate library
        for libraryPath in self.constants.LIBS_PROJECT_PATHS:
            self.buildLibrary(libraryPath)

        for crtFile in self.constants.CRT_FILES:
            crtFile: SourceFile = crtFile

            print(f"(crt) Building {crtFile.path}...")
            os.system(f"mkdir -p {crtFile.getOutputDir()}")

            if os.system(f"{crtFile.compilerDir} -c {crtFile.path} -o {crtFile.outputPath}") != 0:
                return BuilderResult.FAIL

        return BuilderResult.SUCCESS

    def buildLibrary(self, path: str) -> BuilderResult:

        libsSrcDir: str = self.constants.SRC_DIR + "/libs"

        if path.find(libsSrcDir) == -1:
            return BuilderResult.FAIL
        
        for libSrcFile in self.constants.SRC_FILES:
            libSrcFile: SourceFile = libSrcFile
            if self.shouldBuild(libSrcFile) and libSrcFile.get_path().find(path) != -1:
                if libSrcFile.language == SourceLanguage.C:
                    libSrcFile.setBuildFlags(self.constants.USERSPACE_C_FLAGS)
                elif libSrcFile.language == SourceLanguage.ASM:
                    libSrcFile.setBuildFlags(self.constants.USERSPACE_ASM_FLAGS)

                print(f"Building {libSrcFile.path}...")
                os.system(f"mkdir -p {libSrcFile.getOutputDir()}")

                if os.system(libSrcFile.getCompileCmd()) != 0:
                    return BuilderResult.FAIL

        return BuilderResult.SUCCESS

    # Build the entire userspace
    def buildUserspace(self) -> BuilderResult:
        for userDirectory in self.constants.USER_PROJECT_PATHS:
            self.buildUserspaceBinary(userDirectory)

        return BuilderResult.SUCCESS

    def buildUserspaceBinary(self, path: str) -> BuilderResult:
        '''
        Build a binary out of a single userspace project
        * path: a absolute path to the userspace project folder
        '''

        userDir: str = self.constants.SRC_DIR + "/user"

        if path.find(userDir) == -1:
            return BuilderResult.FAIL

        manifestPath: str = f"{path}/manifest.json"
        ourSourceFiles: list[SourceFile] = [];

        # Filter the sourcefiles that are in our directory
        for srcFile in self.constants.SRC_FILES:
            srcFile: SourceFile = srcFile
            if srcFile.path.find(path) != -1:
                ourSourceFiles.append(srcFile)

        userCFlags: str = self.constants.USERSPACE_C_FLAGS

        try:
            with open(manifestPath, "r") as manifestFile:
                manifest = json.load(manifestFile)

                programName: str = manifest["name"]

                print(f"Building program: {programName}")

                # Add kernel headers if we are a driver
                if manifest["type"] == "driver":
                    userCFlags += self.constants.USERSPACE_C_FLAGS_KRNL_INCLUDE_EXT
                
                for srcFile in ourSourceFiles:
                    srcFile: SourceFile = srcFile
                    if self.shouldBuild(srcFile):
                        if srcFile.language == SourceLanguage.C:
                            srcFile.setBuildFlags(userCFlags)
                        elif srcFile.language == SourceLanguage.ASM:
                            srcFile.setBuildFlags(self.constants.USERSPACE_ASM_FLAGS)

                        print(f"Building {srcFile.path}...")
                        os.system(f"mkdir -p {srcFile.getOutputDir()}")

                        if os.system(srcFile.getCompileCmd()) != 0:
                            return BuilderResult.FAIL

        except:
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
            nm = self.constants.CROSS_NM_DIR

            file = build.symbols.write_dummy_source(self.constants.KERNEL_KSYMS_SRC_PATH, self.constants)

            if file == None:
                return BuilderResult.FAIL

            # Build the dummy sourcefile
            if self.buildKernelSourceFile(file) != 0:
                return BuilderResult.FAIL

            ksyms_file_obj_path = f"{file.getOutputDir()}ksyms.o"

            # Add it if it didn't exist before
            if objFiles.find(ksyms_file_obj_path) == -1:
                objFiles += ksyms_file_obj_path

            # Try to link
            if os.system(f"{ld} -o {KEP} {objFiles} {KLF}") != 0:
                return BuilderResult.FAIL

            # We have a valid ELF now, lets dump the symbols
            if os.system(f"{nm} {self.constants.KERNEL_NM_FLAGS} > {self.constants.KERNEL_MAP_PATH}") != 0:
                return BuilderResult.FAIL

            # Run the symbol generator to inject the kernels symbols directly into the build
            symbols: list[build.symbols.KSymbol] = build.symbols.read_map(self.constants.KERNEL_MAP_PATH)

            # We can no generate the corrent symbol sourcefile
            file = build.symbols.write_map_to_source(symbols, self.constants.KERNEL_KSYMS_SRC_PATH, self.constants)

            if file == None:
                return BuilderResult.FAIL

            # Build the sourcefile
            if self.buildKernelSourceFile(file) != 0:
                return BuilderResult.FAIL

            # Link again to include the symbols in the build =D
            if os.system(f"{ld} -o {KEP} {objFiles} {KLF}") != 0:
                return BuilderResult.FAIL

            return BuilderResult.SUCCESS
        elif self.builderMode == BuilderMode.USERSPACE:
            # For each directory in the src/user directory
            # we should link a binary
            userDir: str = self.constants.SRC_DIR + "/user"

            for entryPath in os.listdir(userDir):

                entryName: str = entryPath
                entryPath: str = f"{userDir}/{entryPath}"

                # Skip anything that is not a process direcotry
                if not os.path.isdir(entryPath):
                    continue

                manifestPath: str = f"{entryPath}/manifest.json"

                try:
                    with open(manifestPath, "r") as manifestFile:
                        manifest = json.load(manifestFile)

                        # FIXME: should we do this? (we should allow different program names from directory names lol)
                        if manifest["name"] == entryName:

                            programName: str = manifest["name"]
                            programType: str = manifest["type"]
                            linkType: str = manifest["linking"]

                            objFiles: str = " "
                            print(f"Building process: {entryName}")
                            print(f"Binary out path: {self.constants.OUT_DIR}/user/{entryName}")

                            BIN_OUT_PATH = f"{self.constants.OUT_DIR}/user/{entryName}"
                            BIN_OUT = BIN_OUT_PATH + "/" + programName 

                            ULF = self.constants.USERSPACE_LD_FLAGS

                            for objFile in self.constants.OBJ_FILES:
                                objFile: str = objFile

                                # Add objectfiles from the processes directory
                                if objFile.find(BIN_OUT_PATH) != -1:
                                    objFiles += f"{objFile} "
                                # Add objectfiles from libraries (TODO: Limit to libraries in the manifest)
                                elif objFile.find(self.constants.LIBS_OUT_DIR) != -1 and programType != "driver":
                                    objFiles += f"{objFile} "

                            ld = self.constants.CROSS_LD_DIR

                            if os.system(f"{ld} -o {BIN_OUT} {objFiles} {ULF}") != 0:
                                return BuilderResult.FAIL

                            # TODO: we should check the manifest.json for
                            # the libraries we need to staticaly link
                            # with. After that we dump the binary to
                            # the output directory
                            # (currently out/user/binaries)
                except:
                    continue

            return BuilderResult.SUCCESS
        elif self.builderMode == BuilderMode.LIBRARIES:
            # TODO: make work
            out: str = self.constants.LIBS_OUT_DIR
            entries: list[str] = os.listdir(out)

            # We dump all the shared libraries in /out/libs
            BIN_OUT_PATH: str = out

            for e in entries:

                absLibOutPath: str = f"{out}/{e}"

                if not os.path.isdir(absLibOutPath):
                    continue

                should_terminate: bool = False

                for static_libs in self.constants.LIBENV_ALWAYS_STATIC_LIBS:
                    if absLibOutPath.find(static_libs) != -1:
                        should_terminate = True
                        break

                # If this library is one we should skip, just return a mock success
                if should_terminate:
                    return BuilderResult.SUCCESS

                objFiles: str = " "

                for objFile in self.constants.OBJ_FILES:
                    objFile: str = objFile

                    # Add objectfiles from the librarys directory
                    if objFile.find(absLibOutPath) != -1:
                        objFiles += f"{objFile} "

                BIN_OUT: str = BIN_OUT_PATH + "/" + e + self.constants.SHARED_LIB_EXTENTION
                ULF = self.constants.LIB_LD_FLAGS

                ld = self.constants.CROSS_LD_DIR

                if os.system(f"{ld} -o {BIN_OUT} {objFiles} {ULF}") != 0:
                    return BuilderResult.FAIL
            return BuilderResult.SUCCESS
        elif self.builderMode == BuilderMode.DRIVERS:

            for driver_manifest in self.constants.DRIVER_MANIFESTS:

                driver_manifest: build.manifest.BuildManifest = driver_manifest
                manifest_out_path: str = driver_manifest.path.strip("manifest.json").replace(self.constants.SRC_DIR, self.constants.OUT_DIR)

                print("Manifest out path")
                print(manifest_out_path)

                objFiles: str = " "

                for objFile in self.constants.OBJ_FILES:
                    objFile: str = objFile

                    if objFile.find(manifest_out_path) != -1:
                        objFiles += f"{objFile} "

                ld = self.constants.CROSS_LD_DIR
                out_file = manifest_out_path + driver_manifest.manifested_name

                out_flags = f"{self.constants.DRIVER_LD_FLAGS_EXT}"
 
                if os.system(f"{ld} -o {out_file} {objFiles} {out_flags} ") != 0:
                    return BuilderResult.FAIL

        return BuilderResult.SUCCESS
        

    def clean(self) -> None:
        pass

    def shouldBuild(self, file: SourceFile) -> bool:
        for path in self.srcPath:
            # The path of this file is one that we care about
            if file.get_path().find(path) != -1 and not file.is_header:
                # Without a compiler or output path, we are
                # not really able to do much, are we?
                if file.compilerDir == "" or file.outputPath == "":
                    return False
                return True
        return False
