cflags="$cflags -flto"
ldflagsalways="$ldflagsalways -flto -Wl,-plugin-opt=-load=$PATHLLVMPLUGINS/libplugins.so"

# helper for pass config files
add_lto_args() {
    for arg in "$@"; do
        ldflagsalways="$ldflagsalways -Wl,-plugin-opt=$arg"
    done
}
