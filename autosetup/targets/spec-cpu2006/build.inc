for benchmark in $BENCHMARKS_SPEC; do
    echo "building spec-cpu2006-$instancename $benchmark"
    logsuffix="spec-cpu2006-$instancename-$benchmark" run time "$PATHAUTOSCRIPTSBUILD/build-spec-cpu2006-$instancename.sh" "$benchmark"
done

echo "updating spec-cpu2006-$instancename MD5s"
configbuild="$PATHSPEC/config/MetAlloc-$instancename-build.cfg"
configrun="$PATHSPEC/config/MetAlloc-$instancename-run.cfg"
lineno="`grep -n __MD5__ "$configbuild" | cut -d: -f1`"
tail -n "+$lineno" "$configbuild" >> "$configrun"
