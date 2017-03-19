source "$PATHROOT/autosetup/passes/helper/reset.inc"
source "$PATHROOT/autosetup/passes/helper/lto.inc"

# tcmalloc settings
CONFIG_MALLOC=tcmalloc-metalloc
CONFIG_FIXEDCOMPRESSION=false
CONFIG_METADATABYTES=8
CONFIG_DEEPMETADATA=false
CONFIG_DEEPMETADATABYTES=128
CONFIG_SAFESTACK_OPTIONS="largestack=true"
CONFIG_ALLOC_SIZE_HOOK=dang_alloc_size_hook

# safestack
cflags="$cflags -fsanitize=safe-stack"
ldflagsalways="$ldflagsalways -fsanitize=safe-stack"

# passes
add_lto_args -largestack=false
add_lto_args -stats
add_lto_args -mergedstack=false
add_lto_args -byvalhandler
add_lto_args -stacktracker
add_lto_args -globaltracker
add_lto_args -pointertracker
add_lto_args -FreeSentryLoop
add_lto_args -custominline

# The linker does not include these symbols unless we explicitly say so
ldflagsalways="$ldflagsalways -umetaget_$CONFIG_METADATABYTES"
ldflagsalways="$ldflagsalways -umetaset_$CONFIG_METADATABYTES"
ldflagsalways="$ldflagsalways -umetacheck_$CONFIG_METADATABYTES"
ldflagsalways="$ldflagsalways -umetaset_alignment_safe_$CONFIG_METADATABYTES"
ldflagsalways="$ldflagsalways -umetaset_$CONFIG_METADATABYTES"
ldflagsalways="$ldflagsalways -uinitialize_global_metadata"

ldflagsnolib="$ldflagsnolib -L$PATHAUTOFRAMEWORKOBJ/staticlib-$instance"
ldflagsnolib="$ldflagsnolib -Wl,-whole-archive,-l:libmetadata.a,-no-whole-archive"
ldflagslib="$ldflagslib $PATHAUTOFRAMEWORKOBJ/staticlib-$instance/globalinit.o"
ldflagsalways="$ldflagsalways -ldl"
ldflagsalways="$ldflagsalways @$PATHAUTOFRAMEWORKOBJ/metapagetable-$instance/linker-options"

source "$PATHROOT/autosetup/passes/helper/tcmalloc.inc"
