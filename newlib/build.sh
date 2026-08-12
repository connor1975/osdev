SYSROOT=$(realpath ../sysroot)

rm -r build-newlib
rm -r $SYSROOT/usr/lib
rm -r $SYSROOT/usr/include
git clone https://github.com/connor1975/newlib-cygwin.git
mkdir build-newlib
cd build-newlib
../newlib-cygwin/configure --prefix=/usr --target=x86_64-myos --disable-multilib --disable-shared --disable-werror --disable-newlib-supplied-syscalls --disable-nls --disable-werror
make all
make DESTDIR="$SYSROOT" install
mkdir -p "$SYSROOT"/usr/lib
mv "$SYSROOT"/usr/x86_64-myos/lib/libc.a "$SYSROOT"/usr/lib/libc.a
mv "$SYSROOT"/usr/x86_64-myos/lib/libm.a "$SYSROOT"/usr/lib/libm.a
mv "$SYSROOT"/usr/x86_64-myos/lib/crt0.o "$SYSROOT"/usr/lib/crt0.o
mv "$SYSROOT"/usr/x86_64-myos/include "$SYSROOT"/usr/include
rm -rf "$SYSROOT"/usr/x86_64-myos
cd ..
#rm -rf newlib-cygwin