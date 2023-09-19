import enum
import os
import json
import stats.lines


class BuildManifestType(enum.Enum):
    U_PROCESS = 0,
    U_LIBRARY = 1,
    U_DRIVER = 2,
    K_DRIVER = 3,
    UNKNOWN = 4,
    INVALID = 5,


class BuildManifest(object):

    manifested_name: str
    type: BuildManifestType = BuildManifestType.UNKNOWN
    path: str
    sourcefiles: list[stats.lines.SourceFile] = []

    def __init__(self, type: BuildManifestType, path: str):
        self.type = type
        self.path = path
        self.sourcefiles = []

        with open(self.path, "r") as manifest_file:
            manifest_data = json.load(manifest_file)
            self.manifested_name = manifest_data["name"]

    def gather_sourcefiles(self, sourcefiles: list[stats.lines.SourceFile]):
        dir_path: str = self.path.strip("manifest.json")

        for sourceFile in sourcefiles:
            sourceFile: stats.lines.SourceFile = sourceFile

            if sourceFile.path.find(dir_path) != -1:
                self.sourcefiles.append(sourceFile)


def get_manifest_type(path: str) -> BuildManifestType:
    if not path.endswith("manifest.json"):
        return BuildManifestType.INVALID

    if path.find("src/drivers") != -1:
        return BuildManifestType.K_DRIVER
    elif path.find("src/user") != -1:
        return BuildManifestType.U_PROCESS
    elif path.find("src/libs") != -1:
        return BuildManifestType.U_LIBRARY

    return BuildManifestType.UNKNOWN


def scan_for_manifest(path: str) -> BuildManifest:
    '''
    Scan a directory for a manifest
    Returns a manifest with a type of INVALID if
    there is no manifest present

    There should be no manifests in any child directory of the directory
    that this manifest is contained in. If any are still present, these are skipped

    path should be absolute
    '''

    for dir in os.listdir(path):
        if dir == "manifest.json":
            full_path = f"{path}/{dir}"
            print(full_path)
            with open(full_path, 'r') as manifest_file:
                manifest_json = json.load(manifest_file)

                if manifest_json["type"] is None:
                    return BuildManifest(BuildManifestType.INVALID, full_path)

                if manifest_json["type"] == "process":
                    return BuildManifest(BuildManifestType.U_PROCESS, full_path)

                if manifest_json["type"] == "driver":
                    return BuildManifest(BuildManifestType.K_DRIVER, full_path)

                if manifest_json["type"] == "user_driver":
                    return BuildManifest(BuildManifestType.U_DRIVER, full_path)

                if manifest_json["type"] == "library":
                    return BuildManifest(BuildManifestType.U_LIBRARY, full_path)

    return None
