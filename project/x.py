#!/bin/python3

from sys import argv
from consts import Consts
from enum import Enum
from deps.dependencies import DependencyManager
from deps.ramdisk import RamdiskManager
from build.builder import ProjectBuilder, BuilderMode, BuilderResult


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


# Global todos:
#   TODO: implement caching of built files, so that we don't have to
#         rebuild the project every time we change one thing
#   TODO: redo the command handleing in x.py, its hot trash


def project_main() -> Status:
    print("Starting project script")

    commands: list[str] = argv

    if len(commands) == 1:
        return Status(StatusCode.Fail, "Please supply atleast one valid command")

    c = Consts()

    #
    # EWWWW this reaks lmao
    #

    if commands[1] == "lines":
        c.log_source()
        c.draw_source_bar()
        return Status(StatusCode.Success, "finished showing project diagnostics")
    elif commands[1] == "deps":
        dpm = DependencyManager()
        dpm.pacman_install()
        return Status(StatusCode.Success, "finished installing dependencies!")
    elif commands[1] == "ramdisk":
        if len(commands) != 3:
            return Status(StatusCode.Fail, "Ramdisk needs atleast one subcommand")

        rdm = RamdiskManager()

        print(commands[2])

        if commands[2] == "create":
            rdm.create_ramdisk()
            return Status(StatusCode.Success, "Created ramdisk!")

        return Status(0, "Could not handle subcommand")
    elif commands[1] == "build":
        if len(commands) != 3:
            return Status(StatusCode.Fail, "The Buildsys needs atleast one subcommand")

        print("Trying to build project")

        if commands[2] == "kernel":
            builder = ProjectBuilder(BuilderMode.KERNEL, c)
            if builder.do() != BuilderResult.SUCCESS:
                return Status(StatusCode.Fail, "Failed to build!")

            return Status(StatusCode.Success, "Finished building the kernel =D")
        elif commands[2] == "user":
            return Status(StatusCode.Fail, "TODO: implement userspace building")

        return Status(StatusCode.Fail, "Could not handle subcommand")

    return Status(StatusCode.Fail, "Failed to execute project utilities")


if __name__ == "__main__":
    status: Status = project_main()
    messagePrefix: str = "Success"

    if status.code == StatusCode.Fail:
        messagePrefix = "Error"

    print(f"{messagePrefix}: {status.msg}")
