#!/bin/bash

# TODO: bruh kinda empty lol

handle_err () {
    panic "Error occurred, exititing..."
}

# hihi wonky color codes
log () {
    echo -e "\e[36m[\e[0m \e[1;36mLog\e[0m \e[36m]\e[0m: $1"
}

warn () {
    echo -e "\e[31m[\e[0m \e[1;31mWarn\e[0m \e[31m]\e[0m: $1"
}

panic () {
    echo -e "[Panic] \e[31m $1 \e[0m"
    exit 0
}

remove_sysroot () {
  log "removing sysroot!"
}

make_sysroot () {
  log "making sysroot!"

}

rebuild_sysroot () {
  log "rebuilding sysroot!"
}

# TODO: figure out how to propperly do this -_-
log "Preparing sysroot"
# mkdir -p ./../system/Lib

# TODO: do the thing
LIBC_HEADER_DIR="./../src/libs/LibC"
SYSTEM_ROOT_LIBS_DIR="./../system/libs"

# Oehhh this is dangerous lol
rm -rf $SYSTEM_ROOT_LIBS_DIR

cp -r "$LIBC_HEADER_DIR" "./../system"
mv "./../system/LibC" "./../system/libs"

find "$SYSTEM_ROOT_LIBS_DIR" -type f -not -name '*.h' -delete

log "Done!"
