source "$PATHROOT/autosetup/passes/helper/reset.inc"

# tcmalloc settings
CONFIG_MALLOC=tcmalloc

# compiler settings
cflags="$cflags -flto"
ldflagsalways="$ldflagsalways -flto"

source "$PATHROOT/autosetup/passes/helper/tcmalloc.inc"
