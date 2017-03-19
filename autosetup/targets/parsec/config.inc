cd "$PATHAUTOTARGETSRC/$VERSIONPARSEC"

ccflto=""
for arg in $cflagsbl; do
	if [ "-flto" = "$arg" ]; then
		ccflto="-flto"
	fi
done

configname="MetAlloc-$instancename"
configpath="$PATHAUTOTARGETSRC/$VERSIONPARSEC/config/$configname.bldconf"

pathwrappers="$PATHAUTOTARGETOBJ/$VERSIONPARSEC/binwrap/$configname"
run mkdir -p "$pathwrappers/bin"
makewrapper()
{
	name="$1"
	shift
	(
		echo "#!$PATHAUTOPREFIX/bin/bash"
		echo "set -e"
		echo "$PATHROOT/autosetup/targets/parsec/run-dedup.py $* \"\$@\""
	) > "$pathwrappers/bin/$name"
	chmod u+x "$pathwrappers/bin/$name"
}
makewrapper cc     "$cc $ccflto"
makewrapper c++    "$cxx $ccflto"
makewrapper cpp    "$cc -E"
makewrapper gcc    "$cc $ccflto"
makewrapper g++    "$cxx $ccflto"
makewrapper ar     "$ar"
makewrapper as     "$PATHAUTOPREFIX/bin/llvm-as"
makewrapper ld     "$PATHAUTOPREFIX/bin/ld.gold -plugin $PATHAUTOPREFIX/lib/LLVMgold.so"
makewrapper make   "$PATHAUTOPREFIX/bin/make"
makewrapper m4     "$PATHAUTOPREFIX/bin/m4"
makewrapper ranlib "$ranlib"

parsecsettingadd()
{
	sed -i "s|^ *export $1=\"\\(.*\\)\"|export $1=\"\\1 $2\"|" "$configpath"
}

parsecsettingset()
{
	sed -i "s|^ *export $1=.*|export $1=\"$2\"|" "$configpath"
}

bin/bldconfadd -n "$configname" -c gcc -f
parsecsettingset CC_HOME       "$pathwrappers"
parsecsettingset BINUTIL_HOME  "$pathwrappers"
parsecsettingset GNUTOOL_HOME  "$pathwrappers"
parsecsettingset BINARY_PREFIX ""
parsecsettingset CC            "$pathwrappers/bin/cc"
parsecsettingset CXX           "$pathwrappers/bin/c++"
parsecsettingset CPP           "$pathwrappers/bin/cpp"
parsecsettingadd CFLAGS        "$cflagsbl"
parsecsettingadd CXXFLAGS      "$cflagsbl"
parsecsettingadd LDFLAGS       "$ldflagsalways $ldflagsnolib"
parsecsettingset AS            "$pathwrappers/bin/as"
parsecsettingset LD            "$pathwrappers/bin/ld"
parsecsettingset AR            "$ar"
parsecsettingset RANLIB        "$ranlib"
echo "export PATH=\"$pathwrappers/bin:\$PATH\"" >> "$configpath"

for d in `find "$PATHAUTOTARGETSRC/$VERSIONPARSEC" -type d -name parsec`; do
	(
		echo "#!/bin/bash"
		echo "source \"$d/gcc.bldconf\""
	) > "$d/$configname.bldconf"
done

scriptpathbuild="$PATHAUTOSCRIPTSBUILD/build-parsec-$instancename.sh"
(
	runscript_common_start
	echo "export V=1"
	echo "export VERBOSE=1"
	echo ""
	echo "cd \"$PATHAUTOTARGETSRC/$VERSIONPARSEC\""
	echo "source env.sh"
	echo "bin/parsecmgmt -c \"$configname\" -a build \"\$@\""
	runscript_common_end
) > "$scriptpathbuild"
run chmod u+x "$scriptpathbuild"

scriptpathrun="$PATHAUTOSCRIPTSRUN/run-parsec-$instancename.sh"
(
	runscript_common_start
	echo "cd \"$PATHAUTOTARGETSRC/$VERSIONPARSEC\""
	echo ": \${PATHTEMP:=\"$PATHROOT/autosetup.tmp/\`uname -n\`.\$\$\"}"
	echo "mkdir -p \"\$PATHTEMP\""
	echo "source env.sh"
	echo -n "\"$PATHROOT/nothp/nothp\" "
	echo "bin/parsecmgmt -c \"$configname\" -a run -d \"\$PATHTEMP\" \"\$@\""
	echo "rm -rf \"\$PATHTEMP\""
	runscript_common_end
) > "$scriptpathrun"
run chmod u+x "$scriptpathrun"
