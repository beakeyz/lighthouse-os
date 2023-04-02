#!/bin/python3
from sys import argv
from consts import Consts
from deps.dependencies import DependencyManager
from deps.ramdisk import RamdiskManager

def project_main() -> None:
    print("Starting project script")

    commands: list[str] = argv

    if len(commands) == 1:
        print("Please supply at least one valid command!")
        return

    if commands[1] == "lines":
        c = Consts()
        c.log_source()
        c.draw_source_bar()
    elif commands[1] == "deps":
        c = DependencyManager()
        c.pacman_install()
    elif commands[1] == "ramdisk":
        if len(commands) != 3:
            print("Please supply a valid subcommand for ramdisk!")
            return

        c = RamdiskManager()

        print(commands[2])

        if commands[2] == "create":
            c.create_ramdisk()
        else:
            print("Please supply a valid subcommand for ramdisk!")
    else:
        print("Please supply valid commands")
        


if __name__ == "__main__":
    project_main()
