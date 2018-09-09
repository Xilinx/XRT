#!/bin/bash

set -e

BUILDDIR=$(readlink -f $(dirname ${BASH_SOURCE[0]}))
CORE=`grep -c ^processor /proc/cpuinfo`

usage()
{
    echo "Usage: build.sh [options]"
    echo
    echo "[-help]                    List this help"
    echo "[clean|-clean]             Remove build directories"
    echo "[-j <n>]                   Compile parallel (default: system cores)"
    echo "[-ccache]                  Build using RDI's compile cache"
    echo "[-coverity]                Run a Coverity build, requires admin priviledges to Coverity"
    echo "[-verbose]                 Turn on verbosity when compiling"
    echo ""
    echo "Compile caching is enabled with '-ccache' but requires access to internal network."

    exit 1
}

clean=0
covbuild=0
ccache=0
verbose=""
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
        coverity|-coverity)
            covbuild=1
            shift
            ;;
        -covuser)
            shift
            covuser=$1
            shift
            ;;
        -covpw)
            shift
            covpw=$1
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

if [[ $covbuild == 1 ]]; then
    if [[ -z ${covuser+x} ]]; then
        echo -n "Enter coverity user name: "
        read covuser
    fi
    if [[ -z ${covpw+x} ]]; then
    echo -n "Enter coverity password: "
    read covpw
    fi
    mkdir -p Coverity
    cd Coverity
    cmake -DCMAKE_BUILD_TYPE=Release ../../src
    make -j $CORE COVUSER=$covuser COVPW=$covpw DATE="`git rev-parse --short HEAD`" coverity
    cd $here
    exit 0
fi

mkdir -p Debug Release
cd Debug
cmake -DRDI_CCACHE=$ccache -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ../../src
make -j $jcore $verbose DESTDIR=$PWD install
cd $BUILDDIR

cd Release
cmake -DRDI_CCACHE=$ccache -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ../../src
make -j $jcore $verbose DESTDIR=$PWD install
make package
cd $here
