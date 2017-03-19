echo "configuring perlbrew"
export PERLBREW_ROOT="$PATHAUTOSETUP/perl-root"
export PERLBREW_HOME="$PATHAUTOSETUP/perl-home"

if [ "$NO_PERL" -eq 0 ]; then
	echo "building perlbrew"
	run mkdir -p "$PATHAUTOPACKSRC/perlbrew"
	cd "$PATHAUTOPACKSRC/perlbrew"
	[ -f perlbrew-installer ] || run wget -O perlbrew-installer http://install.perlbrew.pl
	[ -f perlbrew-installer-patched ] || sed "s,/usr/bin/perl,$PATHAUTOPREFIX/bin/perl,g" perlbrew-installer > perlbrew-installer-patched
	run chmod u+x perlbrew-installer-patched
	run "$PATHAUTOPREFIX/bin/bash" ./perlbrew-installer-patched
fi

source "$PERLBREW_ROOT/etc/bashrc"
export PATH="$PERLBREW_ROOT/perls/perl-5.8.8/bin:$PATH"
