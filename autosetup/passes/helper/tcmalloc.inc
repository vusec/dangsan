prefix="$PATHAUTOPREFIXBASE/$instance"
prefixbin="$prefix/bin"
prefixlib="$prefix/lib"

ldflagsalways="$ldflagsalways -ltcmalloc -lpthread -lunwind"
ldflagsalways="$ldflagsalways -L$prefix/lib -L$PATHAUTOPREFIX/lib"
