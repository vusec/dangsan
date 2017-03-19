echo "building binutils"
cd "$PATHAUTOPACKSRC"
[ -f "$VERSIONBINUTILS.tar.bz2" ] || run wget "http://ftp.gnu.org/gnu/binutils/$VERSIONBINUTILS.tar.bz2"
[ -d "$VERSIONBINUTILS" ] || run tar xf "$VERSIONBINUTILS.tar.bz2"
run mkdir -p "$PATHAUTOPACKOBJ/$VERSIONBINUTILS"
cd "$PATHAUTOPACKOBJ/$VERSIONBINUTILS"
confopts="--enable-gold --enable-plugins --disable-werror"
[ -n "`gcc -print-sysroot`" ] && confopts="$confopts --with-sysroot" # match system setting to avoid 'this linker was not configured to use sysroots' error or failure to find libpthread.so
[ -f Makefile ] || run "$PATHAUTOPACKSRC/$VERSIONBINUTILS/configure" --prefix="$PATHAUTOPREFIX" $confopts
run make -j"$JOBS"
run make -j"$JOBS" all-gold
run make install
run rm "$PATHAUTOPREFIX/bin/ld"
run cp "$PATHAUTOPREFIX/bin/ld.gold" "$PATHAUTOPREFIX/bin/ld" # replace ld with gold
