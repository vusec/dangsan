BENCHMARKS_SPEC_C="
	400.perlbench
	401.bzip2
	403.gcc
	429.mcf
	433.milc
	445.gobmk
	456.hmmer
	458.sjeng
	462.libquantum
	464.h264ref
	470.lbm
	482.sphinx3
	"
BENCHMARKS_SPEC_CPP="
	444.namd
	447.dealII
	450.soplex
	453.povray
	471.omnetpp
	473.astar
	483.xalancbmk
	"

: ${BENCHMARKS_SPEC:="$BENCHMARKS_SPEC_C $BENCHMARKS_SPEC_CPP"}
