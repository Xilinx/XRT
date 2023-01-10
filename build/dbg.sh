#!/bin/bash

set -e

OSDIST=`awk -F= '$1=="ID" {print $2}' /etc/os-release | tr -d '"' | awk '{print tolower($1)}'`
BUILDDIR=$(readlink -f $(dirname ${BASH_SOURCE[0]}))
CORE=`grep -c ^processor /proc/cpuinfo`
CMAKE=cmake

if [[ $OSDIST == "centos" ]]; then
    CMAKE=cmake3
    if [[ ! -x "$(command -v $CMAKE)" ]]; then
        echo "$CMAKE is not installed, please run xrtdeps.sh"
        exit 1
    fi
fi

usage()
{
    echo "Usage: build.sh [options]"
    echo
    echo "[-help]                    List this help"
    echo "[-j <n>]                   Compile parallel (default: system cores)"
    echo "[-nocmake]                 Skip CMake itself, go straight to make"
    echo "[-pskernel]                Enable building of POC ps kernel"
    echo "[clean|-clean]             Remove build directories"
    echo "[-noccache]                Disable ccache"

    exit 1
}

clean=0
ccache=1
jcore=$CORE
nocmake=0
cmake_flags="-DXOCL_VERBOSE=1 -DXRT_VERBOSE=1 -DCMAKE_BUILD_TYPE=Debug"
while [ $# -gt 0 ]; do
    case "$1" in
        -help)
            usage
            ;;
        -j)
            shift
            jcore=$1
            shift
            ;;
        clean|-clean)
            clean=1
            shift
            ;;
        -noccache)
            ccache=0
            shift
            ;;
        -nocmake)
            nocmake=1
            shift
            ;;
        -pskernel)
            cmake_flags+=" -DXRT_PSKERNEL_BUILD=ON"
            shift
            ;;
        *)
            echo "unknown option"
            usage
            ;;
    esac
done

if [[ $ccache == 1 ]]; then
    SRCROOT=`readlink -f $BUILDDIR/../src`
    export RDI_ROOT=$SRCROOT
    export RDI_BUILDROOT=$SRCROOT
    export RDI_CCACHEROOT=/scratch/ccache/$USER
    mkdir -p $RDI_CCACHEROOT
    # Run cleanup script once a day
    # Clean cache dir for stale files older than 30 days
    if [[ -e /proj/rdi/env/HEAD/hierdesign/ccache/cleanup.pl ]]; then
        /proj/rdi/env/HEAD/hierdesign/ccache/cleanup.pl 1 30 $RDI_CCACHEROOT
    fi
    cmake_flags+=" -DRDI_CCACHE=1"
fi

here=$PWD
cd $BUILDDIR

if [ $clean == 1 ]; then
    echo $PWD
    echo "/bin/rm -rf XDebug"
    /bin/rm -rf XDebug
    exit 0
fi

mkdir -p XDebug
cd XDebug
if [ $nocmake == 0 ]; then
    $CMAKE $cmake_flags ../../src
fi
make -j $jcore VERBOSE=1 DESTDIR=$PWD install
make VERBOSE=1 DESTDIR=$PWD package
cd $here
