#!/bin/bash

set -e

: ${PATHROOT:="$PWD"}
if [ ! -f "$PATHROOT/autosetup.sh" ]; then
	echo "Please execute from the root of the repository or set PATHROOT" >&2
	exit 1
fi

source "$PATHROOT/autosetup/config.inc"
source "$PATHROOT/autosetup/paths.inc"

corecount="`grep '^processor' /proc/cpuinfo|wc -l`"
[ "$corecount" -le "$JOBSMAX" ] || corecount="$JOBSMAX"

: ${EXTRA_CFLAGS:=""}
: ${EXTRA_LDFLAGS:=""}
: ${JOBS="$corecount"}
: ${NO_PACKAGES:=0}
: ${NO_PERL:=0}

# framework
: ${VERSIONGPERFTOOLS=c46eb1f3d2f7a2bdc54a52ff7cf5e7392f5aa668}

# packages
: ${VERSIONAUTOCONF=autoconf-2.68}
: ${VERSIONAUTOMAKE=automake-1.15}
: ${VERSIONBASH=bash-4.3}
: ${VERSIONBINUTILS=binutils-2.26.1}
: ${VERSIONCMAKE=cmake-3.4.1}
: ${VERSIONCMAKEURL=v3.4}
: ${VERSIONCOREUTILS=coreutils-8.22}
: ${VERSIONLIBTOOL=libtool-2.4.6}
: ${VERSIONLIBUNWIND=libunwind-1.2-rc1}
: ${VERSIONLLVM:=251286}
: ${VERSIONLLVMPATCH:=3.8}
: ${VERSIONM4=m4-1.4.18}
: ${VERSIONMAKE=make-4.2}
: ${VERSIONPERL=perl-5.8.8} # set to 'none' to avoid Perl install
: ${VERSIONPERLURL=5.0}

# targets
: ${VERSIONPARSEC:=parsec-3.0}
: ${VERSIONPARSECURL:=3.0}

PATHBINUTILS="$PATHAUTOPACKSRC/$VERSIONBINUTILS"
PATHLIBUNWIND="$PATHAUTOPACKSRC/$VERSIONLIBUNWIND"

export PATH="$PATHAUTOPREFIX/bin:$PATH"

logdir="$(dirname "$PATHLOG")"
[ -e "$logdir" ] || mkdir -p "$logdir"
exec 5> "$PATHLOG"

run()
{
	echo -------------------------------------------------------------------------------- >&5
	echo "command:          $*"               >&5
	echo "\$PATH:            $PATH"            >&5
	echo "\$LD_LIBRARY_PATH: $LD_LIBRARY_PATH" >&5
	echo "working dir:      $PWD"             >&5
	echo -------------------------------------------------------------------------------- >&5
	success=0
	if [ "$logsuffix" = "" ]; then
		pathlog="$PATHLOG"
		"$@" >&5 2>&5 && success=1
	else
		pathlog="$PATHLOG.$logsuffix.txt"
		echo "logging to $pathlog" >&5
		"$@" > "$pathlog" 2>&1 && success=1
	fi
	if [ "$success" -ne 0 ]; then
		echo "[done]" >&5
	else
		echo "Command '$*' failed in directory $PWD with exit code $?, please check $pathlog for details" >&2
		exit 1
	fi
}

runscript_common_start()
{
	echo "#!/bin/bash"
	echo "set -e"
	echo "export LD_LIBRARY_PATH=\"$prefixlib:$PATHAUTOPREFIX/lib:\$LD_LIBRARY_PATH\""
	echo "export PATH=\"$PATHAUTOPREFIX/bin:\$PATH\""
	echo "export TCMALLOC_LARGE_ALLOC_REPORT_THRESHOLD=281474976710656"
	[ -z "$CONFIG_SAFESTACK_OPTIONS" ] || echo "export SAFESTACK_OPTIONS=$CONFIG_SAFESTACK_OPTIONS"
	echo "PATHROOT=\"$PATHROOT\""
	echo ": \${RUNSCRIPTVERBOSE=0}"
	echo ""
	echo "echo \"[autosetup-runscript] target=$target\""
	echo "echo \"[autosetup-runscript] instancename=$instancename\""
	echo "echo \"[autosetup-runscript] cmd=\$*\""
	echo "echo \"[autosetup-runscript] cwd=\`pwd\`\""
	echo "echo \"[autosetup-runscript] LD_LIBRARY_PATH=\$LD_LIBRARY_PATH\""
	echo "echo \"[autosetup-runscript] PATH=\$PATH\""
	echo "echo \"[autosetup-runscript] PATHROOT=$PATHROOT\""
	echo "echo \"[autosetup-runscript] commit=`git log -n1 --oneline`\""
	echo "echo \"[autosetup-runscript] kernel=\`uname -s\`\""
	echo "echo \"[autosetup-runscript] kernel-release=\`uname -r\`\""
	echo "echo \"[autosetup-runscript] kernel-version=\`uname -v\`\""
	echo "echo \"[autosetup-runscript] machine=\`uname -m\`\""
	echo "echo \"[autosetup-runscript] node=\`uname -n\`\""
	echo "if [ \"\$RUNSCRIPTVERBOSE\" -ne 0 ]; then"
	echo "echo \"[autosetup-runscript] meminfo-start\""
	echo "cat /proc/meminfo"
	echo "echo \"[autosetup-runscript] meminfo-end\""
	echo "echo \"[autosetup-runscript] cpuinfo-start\""
	echo "cat /proc/cpuinfo"
	echo "echo \"[autosetup-runscript] cpuinfo-end\""
	echo "fi"
	echo "echo \"[autosetup-runscript] date-start=\`date +%Y-%m-%dT%H:%M:%S\`\""
	echo ""
}

runscript_common_end()
{
	echo ""
	echo "echo \"[autosetup-runscript] date-end=\`date +%Y-%m-%dT%H:%M:%S\`\""
}

echo "Creating directories"
run mkdir -p "$PATHAUTOFRAMEWORKSRC"
run mkdir -p "$PATHAUTOPACKSRC"
run mkdir -p "$PATHAUTOSCRIPTSBUILD"
run mkdir -p "$PATHAUTOSCRIPTSRUN"
run mkdir -p "$PATHAUTOSTATE"
run mkdir -p "$PATHAUTOTARGETSRC"

export CFLAGS="-I$PATHAUTOPREFIX/include"
export CPPFLAGS="-I$PATHAUTOPREFIX/include"
export LDFLAGS="-L$PATHAUTOPREFIX/lib"

if [ "$NO_PACKAGES" -eq 0 ]; then
	# build bash to override the system's default shell
	source "$PATHROOT/autosetup/packages/bash.inc"

	# build a sane version of coreutils
	source "$PATHROOT/autosetup/packages/coreutils.inc"

	# build binutils to ensure we have gold
	source "$PATHROOT/autosetup/packages/binutils-gold.inc"

	# build make
	source "$PATHROOT/autosetup/packages/make.inc"

	# build m4
	source "$PATHROOT/autosetup/packages/m4.inc"

	# build autoconf
	source "$PATHROOT/autosetup/packages/autoconf.inc"

	# build automake
	source "$PATHROOT/autosetup/packages/automake.inc"

	# build libtool
	source "$PATHROOT/autosetup/packages/libtool.inc"

	# build cmake, needed to build LLVM
	source "$PATHROOT/autosetup/packages/cmake.inc"

	# gperftools requires libunwind
	source "$PATHROOT/autosetup/packages/libunwind.inc"

	# we need a patched LLVM
	source "$PATHROOT/autosetup/packages/llvm.inc"
fi

# Build baseline version of gperftools
echo "downloading gperftools"
cd "$PATHAUTOFRAMEWORKSRC"
if [ ! -d gperftools/.git ]; then
	run git clone https://github.com/gperftools/gperftools.git
	cd gperftools
	run git checkout "$VERSIONGPERFTOOLS"
fi
cd "$PATHAUTOFRAMEWORKSRC/gperftools"
if [ ! -f .autosetup.patched-gperftools-speedup ]; then
	run patch -p1 < "$PATHROOT/patches/GPERFTOOLS_SPEEDUP.patch"
	touch .autosetup.patched-gperftools-speedup
fi
[ -f configure ] || run autoreconf -fi

echo "preparing gperftools-metalloc"
cd "$PATHROOT/gperftools-metalloc"
[ -f configure ] || run autoreconf -fi

for instance in $INSTANCES; do
	if [ ! -f "$PATHROOT/autosetup/passes/$instance.inc" ]; then
		echo "error: unknown pass: $instance" >&2
		exit 1
	fi
	source "$PATHROOT/autosetup/passes/$instance.inc"

	case "$CONFIG_MALLOC" in
	default)
		# no gperftools
		;;
	tcmalloc)
		# standard tcmalloc
		echo "building gperftools-$instance"
		run mkdir -p "$PATHAUTOFRAMEWORKOBJ/gperftools-$instance"
		cd "$PATHAUTOFRAMEWORKOBJ/gperftools-$instance"
		[ -f Makefile ] || run "$PATHAUTOFRAMEWORKSRC/gperftools/configure" --prefix="$PATHAUTOPREFIXBASE/$instance"
		run make -j"$JOBS"
		run make install
		;;
	tcmalloc-metalloc)
		# modified tcmalloc
		echo "building metapagetable-$instance"
		cd "$PATHROOT/metapagetable"
		export METALLOC_OPTIONS="-DFIXEDCOMPRESSION=$CONFIG_FIXEDCOMPRESSION -DMETADATABYTES=$CONFIG_METADATABYTES -DDEEPMETADATA=$CONFIG_DEEPMETADATA"
		[ "true" = "$CONFIG_DEEPMETADATA" ] && METALLOC_OPTIONS="$METALLOC_OPTIONS -DDEEPMETADATABYTES=$CONFIG_DEEPMETADATABYTES"
		[ -n "$CONFIG_ALLOC_SIZE_HOOK" ] && METALLOC_OPTIONS="$METALLOC_OPTIONS -DALLOC_SIZE_HOOK=$CONFIG_ALLOC_SIZE_HOOK"
		metapagetabledir="$PATHAUTOFRAMEWORKOBJ/metapagetable-$instance"
		run make OBJDIR="$metapagetabledir" config
		run make OBJDIR="$metapagetabledir" -j"$JOBS"

		# Build patched gperftools for new allocator
		echo "building gperftools-$instance"
		run mkdir -p "$PATHAUTOFRAMEWORKOBJ/gperftools-$instance"
		cd "$PATHAUTOFRAMEWORKOBJ/gperftools-$instance"
		[ -f Makefile ] || run "$PATHROOT/gperftools-metalloc/configure" --prefix="$PATHAUTOPREFIXBASE/$instance"
		run make METAPAGETABLEDIR="$metapagetabledir" -j"$JOBS"
		run make METAPAGETABLEDIR="$metapagetabledir" install

		echo "building staticlib-$instance"
		cd "$PATHROOT/staticlib"
		run make METAPAGETABLEDIR="$metapagetabledir" OBJDIR="$PATHAUTOFRAMEWORKOBJ/staticlib-$instance" $CONFIG_STATICLIB_MAKE -j"$JOBS"
		;;
	*)
		echo "error: pass $instance does not define CONFIG_MALLOC" >&2
		exit 1
		;;
	esac
done

echo "building llvm-plugins"
cd "$PATHROOT/llvm-plugins"
run make -j"$JOBS" GOLDINSTDIR="$PATHAUTOPREFIX" TARGETDIR="$PATHLLVMPLUGINS"

echo "initializing targets"
for target in $TARGETS; do
	if [ ! -d "$PATHROOT/autosetup/targets/$target" ]; then
		echo "error: unknown target: $target" >&2
		exit 1
	fi
	if [ -f "$PATHROOT/autosetup/targets/$target/init.inc" ]; then
		source "$PATHROOT/autosetup/targets/$target/init.inc"
	fi
done

echo "building nothp"
cd "$PATHROOT/nothp"
run make

# Configure targets
for instance in $INSTANCES; do
	instancename="$instance$INSTANCESUFFIX"
	source "$PATHROOT/autosetup/passes/$instance.inc"

	cflagsbl="$cflags"
	[ "$blacklist" = "" ] || cflagsbl="$cflagsbl -fsanitize-blacklist=$blacklist"

	for target in $TARGETS; do
		echo "configuring $target-$instancename"

		if [ -f "$PATHROOT/autosetup/targets/$target/config.inc" ]; then
			source "$PATHROOT/autosetup/targets/$target/config.inc"
		fi
	done
done

# Build targets
for instance in $INSTANCES; do
	instancename="$instance$INSTANCESUFFIX"

	for target in $TARGETS; do
		echo "building $target-$instancename"

		if [ -f "$PATHROOT/autosetup/targets/$target/build.inc" ]; then
			source "$PATHROOT/autosetup/targets/$target/build.inc"
		fi
	done
done

echo done
