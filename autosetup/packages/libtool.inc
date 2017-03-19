echo "building libtool"
cd "$PATHAUTOPACKSRC"
[ -f "$VERSIONLIBTOOL.tar.gz" ] || run wget "http://ftp.gnu.org/gnu/libtool/$VERSIONLIBTOOL.tar.gz"
[ -d "$VERSIONLIBTOOL" ] || run tar xf "$VERSIONLIBTOOL.tar.gz"
run mkdir -p "$PATHAUTOPACKOBJ/$VERSIONLIBTOOL"
cd "$PATHAUTOPACKOBJ/$VERSIONLIBTOOL"
[ -f Makefile ] || run "$PATHAUTOPACKSRC/$VERSIONLIBTOOL/configure" --prefix="$PATHAUTOPREFIX"
run make -j"$JOBS"
run make install
