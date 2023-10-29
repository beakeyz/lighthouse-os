import tarfile
import os
import consts
import build.manifest as m
from tarfile import TarInfo, TarFile


class RamdiskManager(object):

    OUT_PATH: str = "out/anivaRamdisk.igz"
    SYSROOT_PATH: str = "system"

    DRV_BINARIES_PATH: str = "out/drivers"

    ASSETS_PATH: str = SYSROOT_PATH + "/Assets"
    SYS_PATH: str = SYSROOT_PATH + "/System"
    APPS_PATH: str = SYSROOT_PATH + "/Apps"
    USER_PATH: str = SYSROOT_PATH + "/User"
    GLOB_USR_PATH: str = USER_PATH + "/Global"

    CFG_PATH: str = SYSROOT_PATH + "/kcfg.ini"

    c: consts.Consts = None

    def __init__(self, c: consts.Consts) -> None:
        self.c = c
        pass

    def __ensure_existance(self, path: str):
        try:
            os.mkdir(path)
        except Exception:
            pass

    def __tar_filter(self, tarinfo: TarInfo) -> TarInfo:

        # Just clear ownership flags for now
        tarinfo.uid = 0
        tarinfo.gid = 0

        return tarinfo

    def __copy_apps(self) -> bool:
        '''
        Copy any apps to the sysroot folder that the kernel (or user) might
        need at startup or when the system is installing itself
        '''

        self.__ensure_existance(self.APPS_PATH)

        return True

    def __prepare_user_dir(self) -> bool:
        '''
        Prepare the user directories of the ramdisk
        (These are probably useless but we'll keep them for now
         as a sort of mock-up)
        '''
        self.__ensure_existance(self.USER_PATH)
        self.__ensure_existance(self.GLOB_USR_PATH)
        return True

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

        self.__ensure_existance(self.SYS_PATH)

        # For now, we copy all the drivers unsorted into
        # the default system path for them
        for drv in self.c.DRIVER_MANIFESTS:
            drv: m.BuildManifest = drv

            path: str = drv.path.replace("/src/drivers", "/out/drivers")
            path = path.replace("manifest.json", drv.manifested_name)

            os.system(f"cp {path} {self.SYS_PATH}")

        return True

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

    def reset_ramdisk_root_dir(self) -> bool:
        '''
        Ensure the directory exists within our project and create it if it
        is missing. If it does exist, clear it out
        '''
        project_dirs: list[str] = os.listdir()

        for dir in project_dirs:
            dir: str = dir

            if dir.find(self.SYSROOT_PATH) != -1:
                os.system(f"rm -r {dir}")
                break

        os.mkdir(self.SYSROOT_PATH)

        return True

    def create_ramdisk(self) -> None:

        self.__prepare_user_dir()

        self.__copy_apps()

        # Add driver binaries to the system directory
        self.__copy_drivers()
        # Add assets tot the system directory
        self.__copy_assets()

        with tarfile.open(self.OUT_PATH, "w:gz") as anivaRamdisk:
            # Add the ./system directory as the root for this ramdisk

            self.add_rd_entry(anivaRamdisk, self.SYSROOT_PATH, "/")
            self.add_rd_entry(anivaRamdisk, "out/user/init/init", "/init")
            self.add_rd_entry(anivaRamdisk, "out/user/gfx_test/gfx_test", "/gfx")

    def remove_ramdisk(self) -> bool:
        return False
