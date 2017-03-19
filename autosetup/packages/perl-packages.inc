echo "installing perl packages"
if [ ! -f "$PATHAUTOSTATE/installed-perlbrew-perl-5.8.8" ]; then
	run perlbrew --notest install 5.8.8
	touch "$PATHAUTOSTATE/installed-perlbrew-perl-5.8.8"
fi
run perlbrew switch 5.8.8

if [ "$NO_PERL" -eq 0 ]; then
	[ -f "$PERLBREW_ROOT/bin/cpanm" ] || run perlbrew install-cpanm
	run cpanm -n IO::Uncompress::Bunzip2
	run cpanm -n LWP::UserAgent
	run cpanm -n XML::SAX
	run cpanm -n IO::Scalar
	run cpanm -n Digest::MD5
fi
