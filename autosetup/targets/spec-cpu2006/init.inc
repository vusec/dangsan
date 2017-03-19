if [ -f "$PATHSPEC/shrc" ]; then
	echo "using existing SPEC CPU2006 installation"
else
	echo "please set \$PATHSPEC to the root of your SPEC CPU2006 installation" >&2
	exit 1
fi

cd "$PATHSPEC"
if [ ! -f .autosetup.patched-dealII-stddef ]; then
	run patch -p0 < "$PATHROOT/patches/SPEC-CPU2006-dealII-stddef.patch"
	touch .autosetup.patched-dealII-stddef
fi
if [ ! -f .autosetup.patched-soplex-dangsan-mask ]; then
	run patch -p0 < "$PATHROOT/patches/SPEC-CPU2006-soplex-dangsan-mask.patch"
	touch .autosetup.patched-soplex-dangsan-mask
fi

if [ "$VERSIONPERL" != none ]; then
	# build Perl, default perl does not work with perlbrew
	source "$PATHROOT/autosetup/packages/perl.inc"

	# install perlbrew (needed by SPEC CPU2006), fixing its installer in the process
	source "$PATHROOT/autosetup/packages/perlbrew.inc"

	# install perl packages
	source "$PATHROOT/autosetup/packages/perl-packages.inc"
fi
