echo "building libunwind"
cd "$PATHAUTOPACKSRC"
[ -f "$VERSIONLIBUNWIND.tar.gz" ] || run wget "http://download.savannah.gnu.org/releases/libunwind/$VERSIONLIBUNWIND.tar.gz"
[ -d "$VERSIONLIBUNWIND" ] || run tar xf "$VERSIONLIBUNWIND.tar.gz"
run mkdir -p "$PATHAUTOPACKOBJ/$VERSIONLIBUNWIND"
cd "$PATHAUTOPACKOBJ/$VERSIONLIBUNWIND"
[ -f Makefile ] || run "$PATHAUTOPACKSRC/$VERSIONLIBUNWIND/configure" --prefix="$PATHAUTOPREFIX"
run make -j"$JOBS"
run make install
