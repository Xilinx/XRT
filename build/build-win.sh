#!/bin/bash

# Use this script within a bash shell under WSL
# No need to use Visual Studio separate shell.
# Clone workspace must be on /mnt/c

set -e

BUILDDIR=$(readlink -f $(dirname ${BASH_SOURCE[0]}))
SRCDIR=$(readlink -f $BUILDDIR/../src)
CORE=7

CMAKE="/mnt/c/Program Files/CMake/bin/cmake.exe"
XRT=/mnt/c/Xilinx/xrt
BOOST=$XRT/ext
KHRONOS=$XRT/ext

BOOST=$(sed -e 's|/mnt/\([A-Za-z]\)/\(.*\)|\1:/\2|' -e 's|/|\\|g' <<< $BOOST)
KHRONOS=$(sed -e 's|/mnt/\([A-Za-z]\)/\(.*\)|\1:/\2|' -e 's|/|\\|g' <<< $KHRONOS)

usage()
{
    echo "Usage: build.sh [options]"
    echo
    echo "[-help]                    List this help"
    echo "[clean|-clean]             Remove build directories"
    echo "[-cmake]                   CMAKE executable (default: $CMAKE)"
    echo "[-xrt]                     XRT root directory (default: $XRT)"
    echo "[-boost]                   BOOST libaries root directory (default: $BOOST)"
    echo "[-nocmake]                 Do not rerun cmake generation, just build"
    echo "[-j <n>]                   Compile parallel (default: system cores)"
    echo "[-dbg]                     Build debug library (default: optimized)"
    echo "[-all]                     Build debug and optimized library (default: optimized)"
    echo "[-driver]                  Include building driver code"
    echo "[-verbose]                 Turn on verbosity when compiling"
    echo ""
    echo "Compile caching is enabled with '-ccache' but requires access to internal network."

    exit 1
}

clean=0
ccache=0
docs=0
verbose=""
driver=0
jcore=$CORE
nocmake=0
dbg=0
release=1
while [ $# -gt 0 ]; do
    case "$1" in
        -help)
            usage
            ;;
        clean|-clean)
            clean=1
            shift
            ;;
	-cmake)
	    shift
	    CMAKE="$1"
	    shift
	    ;;
	-xrt)
	    shift
	    XRT="$1"
	    shift
	    ;;
        -dbg)
            dbg=1
            release=0
            shift
            ;;
        -all)
            dbg=1
            release=1
            shift
            ;;
	-boost)
	    shift
	    BOOST="$1"
	    shift
	    ;;
        -j)
            shift
            jcore=$1
            shift
            ;;
        -ccache)
            ccache=1
            shift
            ;;
        -checkpatch)
            checkpatch=1
            shift
            ;;
	docs|-docs)
            docs=1
            shift
            ;;
        -driver)
            driver=1
            shift
            ;;
        -clangtidy)
            clangtidy=1
            shift
            ;;
        -verbose)
            verbose="VERBOSE=1"
            shift
            ;;
        -nocmake)
            nocmake=1
            shift
            ;;
        *)
            echo "unknown option '$1'"
            usage
            ;;
    esac
done

here=$PWD
cd $BUILDDIR

if [[ $clean == 1 ]]; then
    echo $PWD
    echo "/bin/rm -rf WRelease WDebug"
    /bin/rm -rf WRelease WDebug
    exit 0
fi

mkdir -p WDebug WRelease

if [ $dbg == 1 ]; then
cd WDebug
if [ $nocmake == 0 ]; then
  "$CMAKE" -G "Visual Studio 15 2017 Win64" -DMSVC_PARALLEL_JOBS=$jcore -DKHRONOS=$KHRONOS -DBOOST_ROOT=$BOOST -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ../../src
fi
"$CMAKE" --build . --verbose --config Debug
"$CMAKE" --build . --verbose --config Debug --target install
cd $BUILDDIR
fi

if [ $release == 1 ]; then
cd WRelease
if [ $nocmake == 0 ]; then
  "$CMAKE" -G "Visual Studio 15 2017 Win64" -DMSVC_PARALLEL_JOBS=$jcore -DKHRONOS=$KHRONOS -DBOOST_ROOT=$BOOST -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ../../src
fi
"$CMAKE" --build . --verbose --config Release
"$CMAKE" --build . --verbose --config Release --target install
cd $BUILDDIR
fi

cd $here
