# DangSan

DangSan instruments programs written in C or C++ to invalidate pointers
whenever a block of memory is freed, preventing dangling pointers.
Instead, whenever such a pointer is dereferenced, it refers to unmapped memory
and results in a crash. As a consequence, attackers can no longer exploit
dangling pointers.

For more information, see the paper "DangSan: Scalable Use-after-free
Detection" by Erik van der Kouwe, Vinod Nigade, and Cristiano Giuffrida,
presented at the EuroSys 2017 conference.

## Terminology

To explain our system to automatically build DangSan and instrument programs,
we will use the following terms: instance and target.

An instance is a compiler configuration used to instrument a program.
Instances provided by this repository are:

* baseline-lto compiles a program without instrumentation, using LLVM with
  link-time optimizations and using the base version of tcmalloc;
* dangsan instruments the program with our pointer tracker;
* dangsan-stats instruments the program using a static library that tracks
  various statistics about DangSan's work (this instance should not be used
  for performance measurements).

A target is a program to be instrumented by DangSan. We include support for two
targets by default:

* parsec is the PARSEC 3.0 benchmarking suite;
* spec-cpu2006 is the SPEC CPU2006 benchmarking suite.

## Prerequistes

DangSan runs on Linux and was tested on Ubuntu 16.04.2 LTS 64-bit.
It requires a number of packages to be installed, depending on the particular
Linux distribution used. In case of Ubuntu 16.04.2 LTS, the following command
installs the required packages (on a clean server installation):

    sudo apt-get install bison build-essential gettext git pkg-config python ssh subversion

Our prototype includes scripts to instrument the SPEC CPU2006 and PARSEC
benchmarks. While PARSEC is open source and automatically downloaded installed
by our scripts, SPEC CPU2006 is not freely available and must be supplied
by the user.

Our prototype requires about 22GB of disk space, which includes about 2GB
for the SPEC CPU2006 installation and about 11GB for the PARSEC installation.
Both of these are optional.

## Installation

First, obtain the DangSan source code:

    git clone https://github.com/vusec/dangsan.git

The following command automatically installs remaining dependencies locally
(no need for root access), builds DangSan, builds all targets for all instances,
and generate scripts to run the targets conveniently:

    cd dangsan
    PATHSPEC=/path/to/spec/cpu2006 ./autosetup.sh

To control which targets are built, set and export the TARGETS environment
variable to a space-separated (possibly empty) list of desired targets.
Currently supported options are parsec and spec-cpu2006. The default is to build
all targets.

When building the SPEC CPU2006 target, PATHSPEC must point to an existing
SPEC CPU2006 installation. We recommend creating a fresh installation for
DangSan to use because we need to apply some (very minor) patches.

## Running benchmarks

After building DangSan and the desired targets, the targets can be executed
using the run scripts generated in the root directory of the DangSan repository.
The run scripts pass along parameters to the run utility supplied by the
benchmarking suite to allow the user to specify the benchmark and any other
settings. There is a separate run script for each instance. For example,
run-parsec-dangsan.sh runs the parsec target instrumented with DangSan.

For example, to run the bzip2 benchmark from SPEC CPU2006 instrumented by
DangSan, use the following command:

    ./run-spec-cpu2006-dangsan.sh 401.bzip2

To run the blackscholes benchmark from PARSEC in a baseline configuration
using 16 threads, use the following command:

    ./run-parsec-baseline-lto.sh -p parsec.blackscholes -n 16 -i native

Lists of available benchmarks can be found in
autosetup/targets/{parsec,spec-cpu2006}/benchmarks.inc.

## Analyzing results

The run scripts write logs to the standard output. To analyze the results after
running a number of benchmarks, redirect each output to a separate file and pass
the names of output files (or, alternatively, the name of the directory
containing the output files) to scripts/analyze-logs.py. 
