echo "building m4"
cd "$PATHAUTOPACKSRC"
[ -f "$VERSIONM4.tar.gz" ] || run wget "http://ftp.gnu.org/gnu/m4/$VERSIONM4.tar.gz"
[ -d "$VERSIONM4" ] || run tar xf "$VERSIONM4.tar.gz"
run mkdir -p "$PATHAUTOPACKOBJ/$VERSIONM4"
cd "$PATHAUTOPACKOBJ/$VERSIONM4"
[ -f Makefile ] || run "$PATHAUTOPACKSRC/$VERSIONM4/configure" --prefix="$PATHAUTOPREFIX"
run make -j"$JOBS"
run make install
