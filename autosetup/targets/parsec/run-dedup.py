#!/usr/bin/python

import os
import sys

def deduparg(arg):
	if arg.startswith("@"): return True
	if arg.startswith("-fsanitize="): return True
	if arg.startswith("-Wl,-plugin-opt="): return True
	if arg.startswith("-Wl,-l:") and arg.endswith(".a"): return True
	if arg == "-Wl,-whole-archive,-l:libmetadata.a,-no-whole-archive": return True
	return False

def skiparg(arg):
	if arg == "-fno-rtti": return True
	return False

args = list()
argset = set()
for arg in sys.argv[1:]:
	if skiparg(arg): continue
	if deduparg(arg) and (arg in argset): continue
	args.append(arg)
	argset.add(arg)

os.execvp(args[0], args)

