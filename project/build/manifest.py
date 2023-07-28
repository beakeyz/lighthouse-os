import enum
import os
import json
import consts

class BuildManifestType(enum.Enum):
    U_PROCESS = 0,
    U_LIBRARY = 1,
    U_DRIVER = 2,
    K_DRIVER = 3,
    UNKNOWN = 4,
    INVALID = 5,

class BuildManifest(object):

    type: BuildManifestType = UNKNOWN
    path: str
    
    def __init__(self, type: BuildManifestType, path: str):
        pass


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
            with open(full_path, 'r') as manifest_file:
                manifest_json = json.loads(manifest_file)

                if manifest_json["type"] == None:
                    return BuildManifest(INVALID, full_path)
                
                if manifest_json["type"] == "process":
                    return BuildManifest(U_PROCESS, full_path)

                if manifest_json["type"] == "driver":
                    return BuildManifest(U_DRIVER, full_path)

                if manifest_json["type"] == "kdriver":
                    return BuildManifest(K_DRIVER, full_path)
                
                if manifest_json["type"] == "library":
                    return BuildManifest(U_LIBRARY, full_path)

    return BuildManifest(INVALID, None)
