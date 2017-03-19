for benchmark in $BENCHMARKS_PARSEC; do
    echo "building parsec-$instancename $benchmark"
    logsuffix="parsec-$instancename-$benchmark" run time "$PATHAUTOSCRIPTSBUILD/build-parsec-$instancename.sh" -p "$benchmark"
done
