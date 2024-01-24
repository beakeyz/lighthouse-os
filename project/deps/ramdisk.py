import tarfile
import os
import consts
import build.manifest as m
from tarfile import TarInfo, TarFile


class Header(object):
    filename: str
    fullpath: str

    def __init__(self, fn, fp):
        self.filename = fn
        self.fullpath = fp


class RamdiskManager(object):

    OUT_PATH: str = "out/anivaRamdisk.igz"
    SYSROOT_PATH: str = "system"

    DRV_BINARIES_PATH: str = "out/drivers"
    USER_BINARIES_PATH: str = "out/user"

    ASSETS_PATH: str = SYSROOT_PATH + "/Assets"
    SYS_PATH: str = SYSROOT_PATH + "/System"
    APPS_PATH: str = SYSROOT_PATH + "/Apps"
    USER_PATH: str = SYSROOT_PATH + "/User"
    GLOB_USR_PATH: str = USER_PATH + "/Global"

    CFG_PATH: str = SYSROOT_PATH + "/kcfg.ini"

    c: consts.Consts = None
    gathered_headers: list[Header]
    current_header_scan_root: str

    def __init__(self, c: consts.Consts) -> None:
        self.c = c
        self.gathered_headers = None
        self.current_header_scan_root = None
        pass

    def __ensure_existance(self, path: str):
        try:
            os.makedirs(path, exist_ok=True)
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

        os.system(f"cp {self.USER_BINARIES_PATH}/doomgeneric/doom {self.APPS_PATH}")
        os.system(f"cp {self.USER_BINARIES_PATH}/init/init {self.APPS_PATH}")
        os.system(f"cp {self.USER_BINARIES_PATH}/gfx_test/gfx_test {self.APPS_PATH}")

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

    def __gather_headers(self, path: str) -> bool:
        '''
        Tries to find which Libraries must be included in the
        fsroot and copies over the needed headers to the sysroot (system/)
        directory so they can be placed into the ramdisk
        '''

        if self.gathered_headers is None:
            self.gathered_headers = []
            self.current_header_scan_root = path

        # Scan the path for header files
        for entry in os.listdir(path):
            abs_entry = f"{path}/{entry}"

            if os.path.isdir(abs_entry):
                self.__gather_headers(abs_entry)

            # Check if this is a header
            # TODO: have unified file operations to check for filetypes
            if entry.endswith(".h"):
                self.gathered_headers.append(Header(entry, abs_entry))

            pass
        return True

    def __copy_headers(self) -> bool:

        # First, collect all the libc headers
        self.__gather_headers(self.c.LIBC_SRC_DIR)

        # Second, copy those headers over to the system include directory
        if self.gathered_headers is None:
            return False

        for hdr in self.gathered_headers:
            hdr: Header = hdr
            # Relative header path
            hdr_rel: str = str(hdr.fullpath).replace(self.c.LIBC_SRC_DIR, "")
            # Previous strip should have made sure hdr_rel starts with a '/'
            hdr_new_path: str = self.c.SYSROOT_HEADERS_DIR + "/" + hdr_rel

            # Make sure this exists
            self.__ensure_existance(hdr_new_path.replace(hdr.filename, ""))

            os.system(f"cp {hdr.fullpath} {hdr_new_path}")

        return True

    def __copy_drivers(self) -> bool:
        '''
        Try to copy over all the external driver that the kernel might
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

        self.__copy_headers()

        with tarfile.open(self.OUT_PATH, "w:gz") as anivaRamdisk:
            # Add the ./system directory as the root for this ramdisk
            self.add_rd_entry(anivaRamdisk, self.SYSROOT_PATH, "/")

    def remove_ramdisk(self) -> bool:
        return False
