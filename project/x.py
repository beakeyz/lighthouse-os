#!/bin/python3

from sys import argv
from consts import Consts
from deps.dependencies import DependencyManager
from deps.ramdisk import RamdiskManager
from build.builder import ProjectBuilder, BuilderMode, BuilderResult


def project_main() -> None:
    print("Starting project script")

    commands: list[str] = argv

    if len(commands) == 1:
        print("Please supply at least one valid command!")
        return

    c = Consts()

    if commands[1] == "lines":
        c.log_source()
        c.draw_source_bar()
    elif commands[1] == "deps":
        dpm = DependencyManager()
        dpm.pacman_install()
    elif commands[1] == "ramdisk":
        if len(commands) != 3:
            print("Please supply a valid subcommand for ramdisk!")
            return

        rdm = RamdiskManager()

        print(commands[2])

        if commands[2] == "create":
            rdm.create_ramdisk()
        else:
            print("Please supply a valid subcommand for ramdisk!")
    elif commands[1] == "build":
        print("Trying to build project")
        builder = ProjectBuilder(BuilderMode.KERNEL)
        if builder.do() != BuilderResult.SUCCESS:
            print("Failed!")
    else:
        print("Please supply valid commands")


if __name__ == "__main__":
    project_main()
