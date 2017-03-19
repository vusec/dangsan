echo "configuring perl"
cd "$PATHAUTOPACKSRC"
[ -f "$VERSIONPERL.tar.gz" ] || run wget "http://www.cpan.org/src/$VERSIONPERLURL/$VERSIONPERL.tar.gz"
[ -d "$VERSIONPERL" ] || run tar xf "$VERSIONPERL.tar.gz"
cd "$VERSIONPERL"
PATHPERLPATCHES="$PATHROOT/patches"
if [ ! -f .autosetup.patched-makedepend ]; then
	run patch -p1 < "$PATHPERLPATCHES/perl-makedepend.patch"
	touch .autosetup.patched-makedepend
fi
if [ ! -f .autosetup.patched-pagesize ]; then
	run patch -p1 < "$PATHPERLPATCHES/perl-pagesize.patch"
	touch .autosetup.patched-pagesize
fi
if [ ! -f .autosetup.patched-Configure ]; then
	libfile="`gcc -print-file-name=libm.so`"
	libpath="`dirname "$libfile"`"
	[ "$libpath" == . ] || sed -i "s|^xlibpth='|xlibpth='$libpath |" Configure
	touch .autosetup.patched-Configure
fi

if [ "$NO_PERL" -eq 0 ]; then
	echo "building perl"
	[ -f Makefile ] || run "$PATHAUTOPREFIX/bin/bash" ./Configure -des -Dprefix="$PATHAUTOPREFIX"
	for m in makefile x2p/makefile; do
		grep -v "<command-line>" "$m" > "$m.tmp"
		mv "$m.tmp" "$m"
	done
	sed -i 's,# *include *<asm/page.h>,#define PAGE_SIZE 4096,' ext/IPC/SysV/SysV.xs
	run make -j"$JOBS"
	run make install
fi
