CURRDIR=$(pwd)
GCCLOC=$(which x86_64-elf-gcc)
GCCDIR=$(dirname $GCCLOC)

cd $GCCDIR

ln x86_64-elf-ar x86_64-myos-ar
ln x86_64-elf-as x86_64-myos-as
ln x86_64-elf-gcc x86_64-myos-gcc
ln x86_64-elf-gcc x86_64-myos-cc
ln x86_64-elf-ranlib x86_64-myos-ranlib

# return
cd $CURRDIR