from enum import Enum
from consts import take_input 


class InstallerType(Enum):
    IMAGE = 0,
    DEVICE = 1,


class Installer(object):

    i_path: str
    i_type: InstallerType

    def __init__(self, type: InstallerType) -> None:

        self.i_type = type
        self.i_path = take_input("Enter install location")

        pass
    
    def install(self) -> bool:
        '''
        Install the kernel and the userspace to the selected medium
        '''
        return False
