echo "building cmake"
cd "$PATHAUTOPACKSRC"
[ -f "$VERSIONCMAKE.tar.gz" ] || run wget "https://cmake.org/files/$VERSIONCMAKEURL/$VERSIONCMAKE.tar.gz"
[ -d "$VERSIONCMAKE" ] || run tar xf "$VERSIONCMAKE.tar.gz"
mkdir -p "$PATHAUTOPACKOBJ/$VERSIONCMAKE"
cd "$PATHAUTOPACKOBJ/$VERSIONCMAKE"
[ -f Makefile ] || run "$PATHAUTOPACKSRC/$VERSIONCMAKE/configure" --prefix="$PATHAUTOPREFIX"
run make -j"$JOBS"
run make install