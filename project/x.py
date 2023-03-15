#!/bin/python3
from sys import argv
from consts import Consts

def project_main() -> None:
    print("Starting project script")
    args: list[str] = argv

    if args.pop() == "lines":
        c = Consts()
        c.log_source()
        c.draw_source_bar()


if __name__ == "__main__":
    project_main()
