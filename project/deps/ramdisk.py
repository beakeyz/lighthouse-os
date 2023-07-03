import tarfile
from tarfile import TarInfo


class RamdiskManager(object):

    OUT_PATH: str = "out/anivaRamdisk.igz"

    def __init__(self) -> None:
        pass

    def __tar_filter(self, tarinfo: TarInfo) -> TarInfo:
        tarinfo.uid = 0
        tarinfo.gid = 0

        # TODO

        return tarinfo

    def __copy_headers(self) -> bool:
        '''
        Tries to find which Libraries must be included in the 
        fsroot and copies over the needed headers to the sysroot (system/) directory
        so they can be placed into the ramdisk
        '''
        return False

    def create_ramdisk(self) -> None:

        with tarfile.open(self.OUT_PATH, "w:gz") as anivaRamdisk:
            anivaRamdisk.add("system", arcname="/", filter=self.__tar_filter)
            anivaRamdisk.add("out/user/init/init", arcname="/init", filter=self.__tar_filter)

    def remove_ramdisk(self) -> bool:

        return False
