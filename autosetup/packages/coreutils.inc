echo "building coreutils"
cd "$PATHAUTOPACKSRC"
[ -f "$VERSIONCOREUTILS.tar.xz" ] || run wget "http://ftp.gnu.org/gnu/coreutils/$VERSIONCOREUTILS.tar.xz"
[ -d "$VERSIONCOREUTILS" ] || run tar xf "$VERSIONCOREUTILS.tar.xz"
run mkdir -p "$PATHAUTOPACKOBJ/$VERSIONCOREUTILS"
cd "$PATHAUTOPACKOBJ/$VERSIONCOREUTILS"
[ -f Makefile ] || run "$PATHAUTOPACKSRC/$VERSIONCOREUTILS/configure" --prefix="$PATHAUTOPREFIX"
run make -j"$JOBS"
run make install
