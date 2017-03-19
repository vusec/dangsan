unset CONFIG_MALLOC
unset CONFIG_FIXEDCOMPRESSION
unset CONFIG_METADATABYTES
unset CONFIG_DEEPMETADATA
unset CONFIG_DEEPMETADATABYTES
unset CONFIG_SAFESTACK_OPTIONS

unset CONFIG_STATICLIB_MAKE

ar="$PATHAUTOPREFIX/bin/llvm-ar"
cc="$PATHAUTOPREFIX/bin/clang"
cxx="$PATHAUTOPREFIX/bin/clang++"
cflags="$EXTRA_CFLAGS"
ldflagsalways="$EXTRA_LDFLAGS"
ldflagsnolib=""
ldflagslib=""
nm="$PATHAUTOPREFIX/bin/llvm-nm"
ranlib="$PATHAUTOPREFIX/bin/llvm-ranlib"
blacklist=""
run_wrapper=""
post_build_hooks=()

unset prefix
unset prefixbin
unset prefixlib
