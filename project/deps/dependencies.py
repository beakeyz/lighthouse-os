import os

class DependencyManager(object):

    DEFAULT_DEPENDENCIES = ["binutils", "nasm", "gcc", "grub", "qemu", "mtools", "xorriso", "patch", "wget", "git"]

    def __init__(self) -> None:
        pass

    def pacman_install(self) :
        command:str = "sudo pacman -S "

        for dependency in DependencyManager.DEFAULT_DEPENDENCIES:
            command += dependency + " "

        os.system(command)
