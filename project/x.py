#!/bin/python3

import os, io, json
from sys import argv
from consts import Consts
from enum import Enum
from installer.install import Installer, InstallerType, take_input
from deps.dependencies import DependencyManager
from deps.ramdisk import RamdiskManager
from build.builder import ProjectBuilder, BuilderMode, BuilderResult
from build.symbols import *
from installer.system import create_systemroot


class StatusCode(Enum):
    Fail = 0
    Success = 1


class Status(object):

    code: StatusCode
    msg: str

    def __init__(self, code: StatusCode, msg: str) -> None:
        self.code = code
        self.msg = msg
        pass


class CommandCallback(object):

    params: ...

    def __init__(self, params=...) -> None:
        self.params = params
        pass

    def call(self) -> Status:
        raise NotImplementedError


class Command(object):

    name: str
    flags: list[str] = None
    callbacks: dict[str, CommandCallback] = None
    callback: CommandCallback = None

    def __init__(self, name: str, args: dict[str, CommandCallback] = None, callback: CommandCallback = None, flags: list[str] = None) -> None:
        self.name = name
        self.callbacks = args
        self.callback = callback
        self.flags = flags
        pass


# Should give the user a step-by-step experiance with the buildsystem
class CommandProcessor(object):

    cmds: dict[str, Command] = {}

    def __init__(self) -> None:
        pass

    def register_cmd(self, cmd: Command) -> bool:
        self.cmds[cmd.name] = cmd
        return cmd.callback is not None

    def execute(self, command: str) -> Status:
        cmd: Command
        try:
            cmd = self.cmds[command]
        except Exception:
            return Status(StatusCode.Fail, "Could not parse command")

        if cmd.callbacks is None and cmd.callback is not None:
            # TODO: check and ask for flags
            return cmd.callback.call()
        elif cmd.callbacks is not None:
            # Ask the user which subcommand to execute
            print("Available options: ")
            for subcmd in cmd.callbacks:
                print(" > " + subcmd)

            subcmd_str: str = input("(Select subcommand) > ")

            return self.execute_subcommand(cmd, subcmd_str)

        return Status(StatusCode.Fail, "Could not parse command")

    def execute_subcommand(self, parent: Command, subcmd: str) -> Status:
        # Try to retrieve the callback from the parent
        subcallback: CommandCallback
        try:
            subcallback = parent.callbacks[subcmd]
        except Exception:
            return Status(StatusCode.Fail, "Could not parse subcommand")

        # Execute the subcommand callback
        if parent.flags is not None:
            # TODO ask for flags
            return Status(StatusCode.Fail, "TODO: implement flags")

        if subcallback is not None:
            return subcallback.call()

        return Status(StatusCode.Fail, "Could not parse subcommand")


class LinesCallback(CommandCallback):
    def call(self) -> Status:
        c = Consts()
        c.log_source()
        c.draw_source_bar()
        return Status(StatusCode.Success, "")


class DepsCallback(CommandCallback):
    def call(self) -> Status:
        mgr = DependencyManager()
        mgr.pacman_install()
        return Status(StatusCode.Success, "")


class RamdiskCreateCallback(CommandCallback):
    def call(self) -> Status:
        c = Consts()

        ramdisk = RamdiskManager(c)

        ramdisk.create_ramdisk()
        return Status(StatusCode.Success, "Created ramdisk!")


class RamdiskRemoveCallback(CommandCallback):
    def call(self) -> Status:
        c = Consts()

        ramdisk = RamdiskManager(c)

        if ramdisk.remove_ramdisk() is True:
            return Status(StatusCode.Success, "Removed ramdisk!")

        return Status(StatusCode.Fail, "Failed to remove ramdisk!")


class BuildsysBuildKernelCallback(CommandCallback):
    def call(self) -> Status:
        c = Consts()
        builder = ProjectBuilder(BuilderMode.KERNEL, c)

        if builder.do() == BuilderResult.SUCCESS:
            return Status(StatusCode.Success, "Built kernel =D")

        return Status(StatusCode.Fail, "Failed to build kernel")


class BuildsysBuildUserspaceCallback(CommandCallback):
    def call(self) -> Status:
        c = Consts()
        builder = ProjectBuilder(BuilderMode.USERSPACE, c)

        if builder.do() == BuilderResult.SUCCESS:
            return Status(StatusCode.Success, "Built userspace =D")

        return Status(StatusCode.Fail, "Failed to build userspace")


class BuildsysBuildLibraryCallback(CommandCallback):
    def call(self) -> Status:
        c = Consts()
        builder = ProjectBuilder(BuilderMode.LIBRARIES, c)

        if builder.do() == BuilderResult.SUCCESS:
            return Status(StatusCode.Success, "Built libraries =D")

        return Status(StatusCode.Fail, "Failed to build libraries")


class BuildsysBuildDriversCallback(CommandCallback):
    def call(self) -> Status:
        c = Consts()
        builder = ProjectBuilder(BuilderMode.DRIVERS, c)

        if builder.do() == BuilderResult.SUCCESS:
            return Status(StatusCode.Success, "Built drivers =D")

        return Status(StatusCode.Fail, "Failed to build drivers")


class BuildsysBuildAllCallback(CommandCallback):
    def call(self) -> Status:
        c = Consts()

        builder = ProjectBuilder(BuilderMode.LIBRARIES, c)

        # Libraries are first
        if builder.do() != BuilderResult.SUCCESS:
            return Status(StatusCode.Fail, "Failed to build libraries")

        builder = ProjectBuilder(BuilderMode.USERSPACE, c)

        # Then userspace
        if builder.do() != BuilderResult.SUCCESS:
            return Status(StatusCode.Fail, "Failed to build userspace")

        builder = ProjectBuilder(BuilderMode.KERNEL, c)

        # Then the kernel
        if builder.do() != BuilderResult.SUCCESS:
            return Status(StatusCode.Fail, "Failed to build kernel")

        builder = ProjectBuilder(BuilderMode.DRIVERS, c)

        # Then the kernel
        if builder.do() != BuilderResult.SUCCESS:
            return Status(StatusCode.Fail, "Failed to build drivers")

        return Status(StatusCode.Success, "Built project =D")


class GenerateKSymsCallback(CommandCallback):
    def call(self) -> Status:
        c = Consts()
        symbols: list[KSymbol] = read_map(c.KERNEL_MAP_PATH)

        if write_map_to_source(symbols, c.KERNEL_KSYMS_SRC_PATH, c):
            return Status(StatusCode.Success, "Wrote symbols to asm/ksyms.asm !")

        return Status(StatusCode.Fail, "Failed to write symbols")


class GenerateUserProcessCallback(CommandCallback):
    def call(self) -> Status:
        c = Consts()
        processName: str = take_input("Enter process name")
        linkingType: str = take_input("Entry linking type { static, dynamic }")
        processType: str = take_input("Enter proc type { process, service, driver }")

        if linkingType != "static" and linkingType != "dynamic":
            print("Invalid linking type")
            return self.call()

        if processType != "process" and processType != "service" and processType != "driver":
            print("Invalid process type")
            return self.call()

        manifest: dict = {
            "name": processName,
            "linking": linkingType,
            "type": processType,
            "libs": []
        }

        js: str = json.dumps(manifest)
        processDir: str = c.SRC_DIR + "/user"
        thisProcDir: str = f"{processDir}/{processName}"

        os.makedirs(thisProcDir)

        with open(f"{thisProcDir}/manifest.json", "w") as file:
            file.write(js)

        initialCFile: str = "int main() {\n  return 0;\n}"

        with open(f"{thisProcDir}/main.c", "w") as file:
            file.write(initialCFile)

        return Status(StatusCode.Success, "Created thing!")


class GenerateDriverStubCallback(CommandCallback):
    def call(self) -> Status:
        c = Consts()
        driverName: str = take_input("Enter driver name")
        _driverDir: str = take_input("Enter driver dir path (relative from src/drivers/)")

        # Make sure it ends with a slash
        if not _driverDir.endswith("/") and len(_driverDir) > 0:
            _driverDir += "/"

        linkingType: str = take_input("Entry linking type { static, dynamic }")

        if linkingType != "static" and linkingType != "dynamic":
            print("Invalid linking type")
            return self.call()

        manifest: dict = {
            "name": driverName + ".drv",
            "linking": linkingType,
            "type": "driver"
        }

        js: str = json.dumps(manifest)
        driverDir: str = c.SRC_DIR + "/drivers" + "/" + _driverDir
        thisProcDir: str = f"{driverDir}{driverName}"

        try:
            os.makedirs(thisProcDir)
        except FileExistsError as e:
            print(e)

        with open(f"{thisProcDir}/manifest.json", "w") as file:
            file.write(js)

        return Status(StatusCode.Success, "Created thing!")


class InstallSysrootCallback(CommandCallback):
    def call(self) -> Status:

        c = Consts()

        if not create_systemroot(c):
            return Status(StatusCode.Fail, "Could not creaste sysroot!")

        return Status(StatusCode.Success, "Installed sysroot!")


# Create a disk image in which we install the bootloader, kernel and ramdisk
class InstallImageCallback(CommandCallback):
    def call(self) -> Status:
        installer = Installer(InstallerType.IMAGE)

        if (installer.install() is True):
            return Status(StatusCode.Success, "Installed!")

        return Status(StatusCode.Fail, "TODO")


# Install bootloader, kernel and ramdisk directly to the device
class InstallDeviceCallback(CommandCallback):
    def call(self) -> Status:
        installer = Installer(InstallerType.DEVICE)

        if (installer.install() is True):
            return Status(StatusCode.Success, "Installed!")

        return Status(StatusCode.Fail, "TODO")


# Clean all build artifacts
class CleanAllCallback(CommandCallback):
    def call(self) -> Status:
        c = Consts(shouldScan=False)

        exitcode = os.system(f"rm -r {c.OUT_DIR}")

        # Dangerous rm =)
        if exitcode == -1:
            return Status(StatusCode.Fail, f"rm command failed ({exitcode})")

        return Status(StatusCode.Success, "Cleaned everything!")


def direct_exec(args: list[str]) -> Status:
    if (args[1] == "link"):
        c = Consts()
        builder = ProjectBuilder(BuilderMode.KERNEL, c)

        if builder.linkKernel() == BuilderResult.SUCCESS:
            return Status(StatusCode.Success, "Linked the kernel =D")
    elif (args[1] == "ramdisk"):
        c = Consts()

        ramdisk = RamdiskManager(c)

        ramdisk.create_ramdisk()
        return Status(StatusCode.Success, "Created ramdisk!")

    return Status(StatusCode.Fail, "Yikes, direct exec went wrong?")

# Global todos:
#   TODO: implement caching of built files, so that we don't have to
#         rebuild the project every time we change one thing
#   TODO: redo the command handleing in x.py, its hot trash
def project_main() -> Status:
    SCRIPT_LOGO =  "   __   _      __   __  __                                \n"
    SCRIPT_LOGO += "  / /  (_)__ _/ /  / /_/ /  ___  __ _____ ___     ___  ___\n"
    SCRIPT_LOGO += " / /__/ / _ `/ _ \\/ __/ _ \\/ _ \\/ // (_-</ -_)___/ _ \\(_-<\n"
    SCRIPT_LOGO += "/____/_/\\_, /_//_/\\__/_//_/\\___/\\_,_/___/\\__/    \\___/___/\n"
    SCRIPT_LOGO += "  / _ )__ __(_)/ /__/ /__ __ _____                        \n"
    SCRIPT_LOGO += " / _  / // / // // _ (_-</ // (_-<                        \n"
    SCRIPT_LOGO += "/____/\\_,_/_//__/_,_/___/\\_, /___/                        \n"
    SCRIPT_LOGO += "                        /___/                             \n"

    if len(argv) > 1:
        return direct_exec(argv)

    cmd_processor = CommandProcessor()

    cmd_processor.register_cmd(Command("lines", callback=LinesCallback()))
    cmd_processor.register_cmd(Command("deps", callback=DepsCallback()))
    # cmd_processor.register_cmd(Command("ramdisk", args={
    #     "create": RamdiskCreateCallback(),
    #     "remove": RamdiskRemoveCallback(),
    # }))
    cmd_processor.register_cmd(Command("ramdisk", callback=RamdiskCreateCallback()))
    cmd_processor.register_cmd(Command("build", args={
        "kernel": BuildsysBuildKernelCallback(),
        "user": BuildsysBuildUserspaceCallback(),
        "lib": BuildsysBuildLibraryCallback(),
        "drivers": BuildsysBuildDriversCallback(),
        "all": BuildsysBuildAllCallback(),
    }))
    cmd_processor.register_cmd(Command("clean", args={
        "all": CleanAllCallback(),
        "ramdisk": None,
        "kernel": None,
        "libraries": None,
        "drivers": None,
        "userspace": None,
    }))
    cmd_processor.register_cmd(Command("gen", args={
        "userprocess": GenerateUserProcessCallback(),
        "driver": GenerateDriverStubCallback(),
        "ksyms": GenerateKSymsCallback(),
    }))
    cmd_processor.register_cmd(Command("install", args={
        "sysroot": InstallSysrootCallback(),
        "image": InstallImageCallback(),
        "device": InstallDeviceCallback(),
    }))

    # TODO: add option to turn off this feature
    print(SCRIPT_LOGO)
    # TODO: print available actions

    print("Available options: ")
    for cmd in cmd_processor.cmds:
        print(" > " + cmd)

    response: str = take_input("Select action")

    # TODO: parse invalid command tokens (spaces, special chars, ect)

    return cmd_processor.execute(response)


if __name__ == "__main__":
    try:
        status: Status = project_main()
        messagePrefix: str = "Success"

        if status.code == StatusCode.Fail:
            messagePrefix = "Error"

        print(f"{messagePrefix}: {status.msg}")
    except KeyboardInterrupt:
        print("\nExitsequence recieved")
