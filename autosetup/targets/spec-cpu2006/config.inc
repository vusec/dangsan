for conftype in build run; do
	configname="MetAlloc-$instancename-$conftype"
	(
		echo "tune        = base"
		echo "ext         = MetAlloc-$instancename"
		echo "reportable  = no"
		echo "teeout      = yes"
		echo "teerunout   = no"
		echo "makeflags   = -j$JOBS"
		echo "strict_rundir_verify = no"
		echo ""
		echo "default=default=default=default:"
		echo "CC          = $cc $cflagsbl"
		echo "CXX         = $cxx $cflagsbl"
		echo "FC          = `which false`"
		echo "COPTIMIZE   = -O2 -fno-strict-aliasing -std=gnu89"
		echo "CXXOPTIMIZE = -O2 -fno-strict-aliasing"
		echo "CLD         = $cc $ldflagsalways $ldflagsnolib"
		echo "CXXLD       = $cxx $ldflagsalways $ldflagsnolib"
		if [ "$conftype" = build -a ${#post_build_hooks[@]} -gt 0 ]; then
			echo ""
			echo -n "build_post_bench ="
			echo -n "binary=\"\`pwd\`/\`echo \$commandexe |"
			echo -n " sed 's/_\\[a-z0-9\\]\\\\+\\\\.MetAlloc-$instancename\\\\\\$//'\`\"; "
			for hook in "${post_build_hooks[@]}"; do
				echo -n " $hook \"\\\$binary\";"
			done
			echo ""
		fi
		if [ "$conftype" = run -a -n "$run_wrapper" ]; then
			echo ""
			echo "monitor_wrapper = $run_wrapper \$command"
		fi
		echo ""
		echo "default=base=default=default:"
		echo "PORTABILITY    = -DSPEC_CPU_LP64"
		echo ""
		echo "400.perlbench=default=default=default:"
		echo "CPORTABILITY   = -DSPEC_CPU_LINUX_X64"
		echo ""
		echo "462.libquantum=default=default=default:"
		echo "CPORTABILITY   =  -DSPEC_CPU_LINUX"
		echo ""
		echo "483.xalancbmk=default=default=default:"
		echo "CXXPORTABILITY = -DSPEC_CPU_LINUX"
		echo ""
		echo "481.wrf=default=default=default:"
		echo "wrf_data_header_size = 8"
		echo "CPORTABILITY   = -DSPEC_CPU_CASE_FLAG -DSPEC_CPU_LINUX"
	) > "$PATHSPEC/config/$configname.cfg"

	if [ "$conftype" = run ]; then
		runtypes="runspec measuremem"
		scriptbasepath="$PATHAUTOSCRIPTSRUN"
	else
		runtypes="runspec"
		scriptbasepath="$PATHAUTOSCRIPTSBUILD"
	fi
	
	for runtype in $runtypes; do
		if [ "$runtype" = runspec ]; then
			scriptpath="$scriptbasepath/$conftype-spec-cpu2006-$instancename.sh"
		else
			scriptpath="$scriptbasepath/$conftype-$runtype-spec-cpu2006-$instancename.sh"
		fi
		(
			runscript_common_start
			if [ "$conftype" = run ]; then
				echo ": \${SPEC_FORCE_CPU=1}"
				echo "if [ \"\$SPEC_FORCE_CPU\" -eq 0 ]; then"
				echo "    SPEC_FORCE_CPU_CMD=\"\""
				echo "else"
				echo "    SPEC_FORCE_CPU_CMD=\"taskset -c 0\""
				echo "fi"
				echo ""
			fi
			echo "export PERLBREW_HOME=\"$PERLBREW_HOME\""
			echo "export PERLBREW_ROOT=\"$PERLBREW_ROOT\""
			echo "cd \"$PATHSPEC\""
			echo "source \"$PERLBREW_ROOT/etc/bashrc\""
			echo "source \"$PATHSPEC/shrc\""
			if [ "$runtype" = measuremem ]; then
				echo "runspec --config=\"$configname\" --action=setup \"\$@\""
			fi
			[ "$conftype" = run ] && echo -n "\"$PATHROOT/nothp/nothp\" "
			[ "$conftype" = run ] && echo -n "\$SPEC_FORCE_CPU_CMD "
			if [ "$runtype" = measuremem ]; then
				pathspecrun="$PATHSPEC"
				echo "$PATHROOT/autosetup/targets/spec-cpu2006/measuremem.py \"$pathspecrun\" \"$instancename\" \"\$@\""
			else
				echo -n "runspec --config=\"$configname\" \"\$@\""
				[ "$conftype" = build ] && echo -n " --action=build"
				[ "$conftype" = run ] && echo -n " --nobuild"
				echo ""
			fi
			runscript_common_end
		) > "$scriptpath"
		run chmod u+x "$scriptpath"
	done
done
