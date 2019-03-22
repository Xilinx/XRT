#!/bin/bash

set -e

OSDIST=`lsb_release -i |awk -F: '{print tolower($2)}' | tr -d ' \t'`
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
    echo "[clean|-clean]             Remove build directories"
    echo "[-j <n>]                   Compile parallel (default: system cores)"
    echo "[-ccache]                  Build using RDI's compile cache"
    echo "[-nodriver]                Do not build driver code"
    echo "[-checkpatch]              Run checkpatch.pl on driver code"
    echo "[-verbose]                 Turn on verbosity when compiling"
    echo ""
    echo "Compile caching is enabled with '-ccache' but requires access to internal network."

    exit 1
}

clean=0
ccache=0
docs=0
verbose=""
driver=1
checkpatch=0
jcore=$CORE
while [ $# -gt 0 ]; do
    case "$1" in
        -help)
            usage
            ;;
        clean|-clean)
            clean=1
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
        -nodriver)
            driver=0
            shift
            ;;
        -verbose)
            verbose="VERBOSE=1"
            shift
            ;;
        *)
            echo "unknown option"
            usage
            ;;
    esac
done

here=$PWD
cd $BUILDDIR

if [[ $clean == 1 ]]; then
    echo $PWD
    echo "/bin/rm -rf Release Debug"
    /bin/rm -rf Release Debug
    exit 0
fi

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
fi

mkdir -p Debug Release
cd Debug
echo "$CMAKE -DRDI_CCACHE=$ccache -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ../../src"
time $CMAKE -DRDI_CCACHE=$ccache -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ../../src
time make -j $jcore $verbose DESTDIR=$PWD install
cd $BUILDDIR

cd Release
echo "$CMAKE -DRDI_CCACHE=$ccache -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ../../src"
time $CMAKE -DRDI_CCACHE=$ccache -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ../../src
time make -j $jcore $verbose DESTDIR=$PWD install
time make package

if [[ $driver == 1 ]]; then
    make -C usr/src/xrt-2.2.0/driver/xclng/drm/xocl
fi

if [[ $docs == 1 ]]; then
    make xrt_docs
fi

if [[ $checkpatch == 1 ]]; then
    # check only driver released files
    DRIVERROOT=`readlink -f $BUILDDIR/Release/usr/src/xrt-2.2.0/driver`

    # find corresponding source under src tree so errors can be fixed in place
    XOCLROOT=`readlink -f $BUILDDIR/../src/runtime_src/driver`
    echo $XOCLROOT
    for f in $(find $DRIVERROOT -type f -name *.c -o -name *.h); do
        fsum=$(md5sum $f | cut -d ' ' -f 1)
        for src in $(find $XOCLROOT -type f -name $(basename $f)); do
            ssum=$(md5sum $src | cut -d ' ' -f 1)
            if [[ "$fsum" == "$ssum" ]]; then
                $BUILDDIR/checkpatch.sh $src | grep -v WARNING
            fi
        done
    done
fi

cd $here
