from enum import Enum

class InstallerType(Enum):
    IMAGE = 0,
    DEVICE = 1,

class Installer(object):
    def __init__(self, type: InstallerType) -> None:
        # TODO
        pass
    