import tarfile
from tarfile import TarInfo, TarFile


class RamdiskManager(object):

    OUT_PATH: str = "out/anivaRamdisk.igz"

    def __init__(self) -> None:
        pass

    def __tar_filter(self, tarinfo: TarInfo) -> TarInfo:

        # Just clear ownership flags for now
        tarinfo.uid = 0
        tarinfo.gid = 0

        return tarinfo

    def __copy_headers(self) -> bool:
        '''
        Tries to find which Libraries must be included in the
        fsroot and copies over the needed headers to the sysroot (system/)
        directory so they can be placed into the ramdisk
        '''
        return False

    def __copy_drivers(self) -> bool:
        '''
        Try to copy over all the external driver that the kerne might
        need to the ./system directory before they get compacted to
        the archive
        '''
        return False

    def __copy_assets(self) -> bool:
        '''
        Copies over any assets that the system might need to function
        to the ./system directory. Think of configs,
        images, database files, ect.
        '''
        return False

    def __copy_utills(self) -> bool:
        '''
        Copies any utills to the ./system directory. These programs might be
        handy for system stuff
        '''
        return False

    def add_rd_entry(self, ramdisk: TarFile, path: str, name: str) -> None:
        ramdisk.add(
                name=path,
                arcname=name,
                filter=self.__tar_filter
                )
        pass

    def create_ramdisk(self) -> None:

        # Add driver binaries to the system directory
        self.__copy_drivers()
        # Add assets tot the system directory
        self.__copy_assets()

        with tarfile.open(self.OUT_PATH, "w:gz") as anivaRamdisk:
            # Add the ./system directory as the root for this ramdisk

            self.add_rd_entry(anivaRamdisk, "system", "/")
            self.add_rd_entry(anivaRamdisk, "out/user/init/init", "/init")
            self.add_rd_entry(
                    anivaRamdisk,
                    "out/drivers/test/test.drv",
                    "test.drv")

    def remove_ramdisk(self) -> bool:

        return False
