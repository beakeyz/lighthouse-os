import os
from build.sourcefiles import SourceFile, SourceLanguage
from consts import Consts

class KSymbol(object):

    name: str = ""
    address = 0
    ktype = ''

    def __init__(self, name: str, address, ktype):
        self.name = name
        self.address = address
        self.ktype = ktype


def read_map(map_path: str) -> list[KSymbol]:
    symbol_list = []

    if not map_path.endswith(".map"):
        return None

    with open(map_path, "r") as map_file:
        # Read the line
        values = map_file.readline()

        while values:

            values = values.split(" ")
            
            # Grab the underlying data
            address = values[0]
            ktype = values[1]
            name = values[2].replace("\n", "")

            symbol_list.append(KSymbol(name, address, ktype))

            values = map_file.readline()

    return symbol_list

# NOTE
#   We output x86 NASM assembly
#

def __put_global_label(file, label: str):
    file.write(f"[global {label}]\n")
    file.write("align 8\n")
    file.write(f"{label}:\n")

def __put_long(file, num):
    file.write(f"\tdw {num}\n")

def __put_quad(file, num):
    file.write(f"\tdq {num}\n")

def __put_string(file, string: str):
    file.write(f"\tdb \"{string}\",0x00\n")

def __put_address(file, address):
    file.write(f"\tdq 0x{address}\n")

def write_dummy_source(out_path: str, c: Consts) -> SourceFile:
    '''
    Write a dummy variant of the ksyms source file, to make the linker happy
    '''

    if not out_path.endswith(".asm"):
        return None 

    try:
        os.remove(out_path)
    except:
        pass

    with open(out_path, "x") as source_file:
        source_file.write("[section .ksyms]\n")

        source_file.write("\n")

        __put_global_label(source_file, "__ksyms_count")
        __put_quad(source_file, 0)

        source_file.write("\n")

        __put_global_label(source_file, "__ksyms_table")
        
        # Mark the end of the symbol table with a 0x00
        __put_quad(source_file, 0)

    # Write a dummy sourcefile
    file = SourceFile(False, out_path, "ksyms.asm", SourceLanguage.ASM)
    file.set_compiler(c.CROSS_ASM_DIR)
    file.setOutputPath(file.get_path().replace(c.SRC_DIR, c.OUT_DIR))

    return file

def write_map_to_source(map: list[KSymbol], out_path: str, c: Consts) -> SourceFile:

    if not out_path.endswith(".asm"):
        return None 

    try:
        os.remove(out_path)
    except:
        pass

    with open(out_path, "x") as source_file:
        source_file.write("[section .ksyms]\n")

        source_file.write("\n")

        __put_global_label(source_file, "__ksyms_count")
        __put_quad(source_file, len(map))

        source_file.write("\n")

        __put_global_label(source_file, "__ksyms_table")

        for symbol in map:
            symbol: KSymbol = symbol

            # Total length of this symbol object: length of the name, 
            # plus the null-byte plus the quad for the address
            __put_quad(source_file, len(symbol.name) + 1 + 8 + 8)
            __put_address(source_file, symbol.address)
            __put_string(source_file, symbol.name)

        
        # Mark the end of the symbol table with a 0x00
        __put_quad(source_file, 0)


    file = SourceFile(False, out_path, "ksyms.asm", SourceLanguage.ASM)
    file.set_compiler(c.CROSS_ASM_DIR)
    file.setOutputPath(file.get_path().replace(c.SRC_DIR, c.OUT_DIR))

    return file
