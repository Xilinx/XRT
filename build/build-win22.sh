#!/bin/bash

# Use this script within a bash shell under WSL
# No need to use Visual Studio separate shell.
# Clone workspace must be on /mnt/c

set -e

BUILDDIR=$(readlink -f $(dirname ${BASH_SOURCE[0]}))
CORE=`grep -c ^processor /proc/cpuinfo`

CMAKE="/mnt/c/Program Files/CMake/bin/cmake.exe"
EXT_DIR=/mnt/c/Xilinx/xrt/ext.new
BOOST=$EXT_DIR
KHRONOS=$EXT_DIR

usage()
{
    echo "Usage: build.sh [options]"
    echo
    echo "[-help]                    List this help"
    echo "[clean|-clean]             Remove build directories"
    echo "[-cmake]                   CMAKE executable (default: $CMAKE)"
    echo "[-ext]                     Location of link dependencies (default: $EXT_DIR)"
    echo "[-boost]                   BOOST libaries root directory (default: $BOOST)"
    echo "[-nocmake]                 Do not rerun cmake generation, just build"
    echo "[-noabi]                   Do compile with ABI version check"
    echo "[-j <n>]                   Compile parallel (default: system cores)"
    echo "[-dbg]                     Build debug library (default: optimized)"
    echo "[-all]                     Build debug and optimized library (default: optimized)"

    exit 1
}

clean=0
jcore=$CORE
nocmake=0
noabi=0
dbg=0
release=1
cmake_flags="-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
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
	-ext)
	    shift
	    EXT_DIR="$1"
            BOOST=$EXT_DIR
            KHRONOS=$EXT_DIR
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
        -hip)
            cmake_flags+= " -DXRT_ENABLE_HIP=ON"
            shift
            ;;
        -j)
            shift
            jcore=$1
            shift
            ;;
        -nocmake)
            nocmake=1
            shift
            ;;
        -noabi)
            cmake_flags+=" -DDISABLE_ABI_CHECK=1"
            shift
            ;;
        *)
            echo "unknown option '$1'"
            usage
            ;;
    esac
done

BOOST=$(sed -e 's|/mnt/\([A-Za-z]\)/\(.*\)|\1:/\2|' -e 's|/|\\|g' <<< $BOOST)
KHRONOS=$(sed -e 's|/mnt/\([A-Za-z]\)/\(.*\)|\1:/\2|' -e 's|/|\\|g' <<< $KHRONOS)

here=$PWD
cd $BUILDDIR

if [[ $clean == 1 ]]; then
    echo $PWD
    echo "/bin/rm -rf WRelease WDebug"
    /bin/rm -rf WRelease WDebug
    exit 0
fi

cmake_flags+=" -DMSVC_PARALLEL_JOBS=$jcore"
cmake_flags+=" -DKHRONOS=$KHRONOS"
cmake_flags+=" -DBOOST_ROOT=$BOOST"

echo "${cmake_flags[@]}"

if [ $dbg == 1 ]; then
    cmake_flags+=" -DCMAKE_BUILD_TYPE=Debug"
    mkdir -p WDebug
    cd WDebug

    if [ $nocmake == 0 ]; then
        "$CMAKE" -G "Visual Studio 17 2022" $cmake_flags ../../src
    fi
    "$CMAKE" --build . --verbose --config Debug
    "$CMAKE" --build . --verbose --config Debug --target install
    cd $BUILDDIR
fi

if [ $release == 1 ]; then
    cmake_flags+=" -DCMAKE_BUILD_TYPE=Release"
    mkdir -p WRelease
    cd WRelease
    if [ $nocmake == 0 ]; then
        "$CMAKE" -G "Visual Studio 17 2022" $cmake_flags ../../src
    fi
    "$CMAKE" --build . --verbose --config Release
    "$CMAKE" --build . --verbose --config Release --target install
    cd $BUILDDIR
fi

cd $here
