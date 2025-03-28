#!/bin/bash

# Use this script within a bash shell under WSL
# No need to use Visual Studio separate shell.
# Clone workspace must be on /mnt/c

set -e

BUILDDIR=$(readlink -f $(dirname ${BASH_SOURCE[0]}))

unix2dos()
{
    echo $(sed -e 's|/mnt/\([A-Za-z]\)/\(.*\)|\1:/\2|' <<< $1)
}


CORE=`grep -c ^processor /proc/cpuinfo`

CMAKE="/mnt/c/Program Files/CMake/bin/cmake.exe"
CPACK="/mnt/c/Program Files/CMake/bin/cpack.exe"
EXT_DIR=/mnt/c/Xilinx/xrt/ext.new
BOOST=$(unix2dos $EXT_DIR)
KHRONOS=$(unix2dos $EXT_DIR)

usage()
{
    echo "Usage: build.sh [options]"
    echo
    echo "[-help]                    List this help"
    echo "[clean|-clean]             Remove build directories"
    echo "[-prefix]                  CMAKE_INSTALL_PREFIX (default: $BUILDDIR/<WRelease|WDebug>/xilinx)"
    echo "[-cmake]                   CMAKE executable (default: $CMAKE)"
    echo "[-ext]                     Location of link dependencies (default: $EXT_DIR)"
    echo "[-boost]                   BOOST libaries root directory (default: $BOOST)"
    echo "[-nocmake]                 Do not rerun cmake generation, just build"
    echo "[-noabi]                   Do compile with ABI version check"
    echo "[-j <n>]                   Compile parallel (default: system cores)"
    echo "[-dbg]                     Build debug library (default: optimized)"
    echo "[-all]                     Build debug and optimized library (default: optimized)"
    echo "[-sdk]                     Create NSIS XRT SDK NPU Installer (requires NSIS installed)."

    exit 1
}

clean=0
prefix=
jcore=$CORE
nocmake=0
noabi=0
dbg=0
release=1
sdk=0
alveo_build=0
npu_build=0
base_build=0
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
        -prefix)
            shift
            prefix="$1"
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
        -base)
            shift
            base_build=1
            cmake_flags+= " -DXRT_BASE=1"
            ;;
        -alveo)
            shift
            alveo_build=1
            cmake_flags+=" -DXRT_ALVEO=1"
            ;;
	-npu)
            shift
	    npu_build=1
	    cmake_flags+=" -DXRT_NPU=1"
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
        -sdk)
            shift
            npu_build=1
	    cmake_flags+=" -DXRT_NPU=1"
            sdk=1
            ;;
        *)
            echo "unknown option '$1'"
            usage
            ;;
    esac
done

if [[ $((npu_build + alveo_build + base_build)) > 1 ]]; then
    echo "build.sh: -npu, -alveo, -base are mutually exclusive"
    exit 1
fi

BOOST=$(unix2dos $BOOST)
KHRONOS=$(unix2dos $KHRONOS)
prefix=$(unix2dos $prefix)

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

if [ $dbg == 1 ]; then
    cmake_flags+=" -DCMAKE_BUILD_TYPE=Debug"
    mkdir -p WDebug
    cd WDebug

    if [[ $prefix == "" ]]; then
        cmake_flags+=" -DCMAKE_INSTALL_PREFIX=$(unix2dos $BUILDDIR/WDebug/xilinx)"
    else
        cmake_flags+=" -DCMAKE_INSTALL_PREFIX=$prefix"
    fi

    if [ $nocmake == 0 ]; then
        echo "${cmake_flags[@]}"
        "$CMAKE" -G "Visual Studio 17 2022" $cmake_flags ../../src
    fi
    "$CMAKE" --build . --verbose --config Debug --target install
    cd $BUILDDIR
fi

if [ $release == 1 ]; then
    cmake_flags+=" -DCMAKE_BUILD_TYPE=Release"
    mkdir -p WRelease
    cd WRelease

    if [[ $prefix == "" ]]; then
        cmake_flags+=" -DCMAKE_INSTALL_PREFIX=$(unix2dos $BUILDDIR/WRelease/xilinx)"
    else
        cmake_flags+=" -DCMAKE_INSTALL_PREFIX=$prefix"
    fi
    
    if [ $nocmake == 0 ]; then
        echo "${cmake_flags[@]}"
        "$CMAKE" -G "Visual Studio 17 2022" $cmake_flags ../../src
    fi
    "$CMAKE" --build . --verbose --config Release --target install

    if [[ $sdk == 1 && $npu_build == 1 ]]; then
        echo "Creating SDK installer ..."
        "$CPACK" -G NSIS -C Release
    fi
    cd $BUILDDIR
fi

cd $here
