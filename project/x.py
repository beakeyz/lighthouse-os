#!/bin/python3
from sys import argv
from consts import Consts
from deps.dependencies import DependencyManager

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
    else:
        print("Please supply valid commands")
        


if __name__ == "__main__":
    project_main()
