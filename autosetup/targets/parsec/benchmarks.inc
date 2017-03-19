BENCHMARKS_PARSEC_PARSEC="
	parsec.blackscholes
	parsec.canneal
	parsec.ferret
	parsec.fluidanimate
	parsec.freqmine
	parsec.streamcluster
	parsec.vips
	parsec.x264
	"
BENCHMARKS_PARSEC_SPLASH2="
	splash2.barnes
	splash2.cholesky
	splash2.fft
	splash2.fmm
	splash2.lu_cb
	splash2.lu_ncb
	splash2.ocean_cp
	splash2.ocean_ncp
	splash2.radiosity
	splash2.radix
	splash2.raytrace
	splash2.volrend
	splash2.water_nsquared
	splash2.water_spatial
	"
BENCHMARKS_PARSEC_SPLASH2X="
	splash2x.barnes
	splash2x.cholesky
	splash2x.fft
	splash2x.fmm
	splash2x.lu_cb
	splash2x.lu_ncb
	splash2x.ocean_cp
	splash2x.ocean_ncp
	splash2x.radiosity
	splash2x.radix
	splash2x.raytrace
	splash2x.water_nsquared
	splash2x.water_spatial
	"

# compile issues:
#	parsec.bodytrack		incorrect template syntax for clang
#	parsec.cmake			expects strlcat; fixed in dangsan but does not work with baseline
#	parsec.dedup			incorrect pod2man syntax
#	parsec.facesim			undeclared template used
#	parsec.mesa			incorrect asm syntax
#	parsec.netdedup			conflicting header files
#	parsec.netferret		conflicting header files
#	parsec.netstreamcluster		conflicting header files
#	parsec.raytrace			incorrect asm syntax
#	parsec.ssl			incorrect pod2man syntax
#	parsec.swaptions		missing -I switch
#	parsec.uptcpip			conflicting header files
#	splash2x.volrend		incorrect main function, missing includes

# not real benchmarks:
#	parsec.glib
#	parsec.gsl
#	parsec.hooks
#	parsec.libjpeg
#	parsec.libtool
#	parsec.libxml2
#	parsec.parmacs
#	parsec.tbblib
#	parsec.yasm
#	parsec.zlib

: ${BENCHMARKS_PARSEC:="$BENCHMARKS_PARSEC_PARSEC $BENCHMARKS_PARSEC_SPLASH2X"}
