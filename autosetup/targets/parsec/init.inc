cd "$PATHAUTOTARGETSRC"
[ -f "$VERSIONPARSEC.tar.gz" ] || run wget "http://parsec.cs.princeton.edu/download/$VERSIONPARSECURL/$VERSIONPARSEC.tar.gz"
[ -d "$VERSIONPARSEC" ] || run tar xf "$VERSIONPARSEC.tar.gz"
