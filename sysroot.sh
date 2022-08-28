# for porting app we need sysroot, ty wingos 
# https://github.com/Supercip971/WingOS/blob/master/make_sysroot.sh

mkdir -p ./sysroot/
mkdir -p ./sysroot/libraries
mkdir -p ./sysroot/libraries/libc
mkdir -p ./sysroot/libraries/utils
cp -r ./libraries/libc/** ./sysroot/libraries/

cp -r ./libraries/utils ./sysroot/libraries/
cp -r ./libraries/mod ./sysroot/mod
