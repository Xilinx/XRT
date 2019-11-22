#!/bin/bash

set -e

OSDIST=`lsb_release -i |awk -F: '{print tolower($2)}' | tr -d ' \t'`
BUILDDIR=$(readlink -f $(dirname ${BASH_SOURCE[0]}))
CORE=`grep -c ^processor /proc/cpuinfo`
CMAKE=cmake
CPU=`uname -m`

if [[ $OSDIST == "centos" ]] || [[ $OSDIST == "amazon" ]]; then
    CMAKE=cmake3
    if [[ ! -x "$(command -v $CMAKE)" ]]; then
        echo "$CMAKE is not installed, please run xrtdeps.sh"
        exit 1
    fi
fi

if [[ $CPU == "aarch64" ]] && [[ $OSDIST == "ubuntu" ]]; then
    # On ARM64 Ubuntu use GCC version 8 if available since default
    # (GCC version 7) has random Internal Compiler Issues compiling XRT
    # C++14 code
    gcc-8 --version > /dev/null 2>&1
    status1=$?
    g++-8 --version > /dev/null 2>&1
    status2=$?
    if [[ $status1 == 0 ]] && [[ $status2 == 0 ]]; then
	export CC=gcc-8
	export CXX=g++-8
    fi
fi

usage()
{
    echo "Usage: build.sh [options]"
    echo
    echo "[-help]                    List this help"
    echo "[clean|-clean]             Remove build directories"
    echo "[-dbg]                     Build debug library only"
    echo "[-opt]                     Build optimized library only"
    echo "[-nocmake]                 Skip CMake call"
    echo "[-j <n>]                   Compile parallel (default: system cores)"
    echo "[-ccache]                  Build using RDI's compile cache"
    echo "[-driver]                  Include building driver code"
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
driver=0
clangtidy=0
checkpatch=0
jcore=$CORE
opt=1
dbg=1
nocmake=0
while [ $# -gt 0 ]; do
    case "$1" in
        -help)
            usage
            ;;
        clean|-clean)
            clean=1
            shift
            ;;
        -dbg)
            dbg=1
            opt=0
            shift
            ;;
        -opt)
            dbg=0
            opt=1
            shift
            ;;
        -nocmake)
            nocmake=1
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

if [[ $dbg == 1 ]]; then
  mkdir -p Debug
  cd Debug
  if [[ $nocmake == 0 ]]; then
    echo "$CMAKE -DRDI_CCACHE=$ccache -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ../../src"
    time $CMAKE -DRDI_CCACHE=$ccache -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ../../src
  fi
  echo "make -j $jcore $verbose DESTDIR=$PWD install"
  time make -j $jcore $verbose DESTDIR=$PWD install
  time ctest --output-on-failure
  cd $BUILDDIR
fi

if [[ $opt == 1 ]]; then
  mkdir -p Release
  cd Release
  if [[ $nocmake == 0 ]]; then
    echo "$CMAKE -DRDI_CCACHE=$ccache -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ../../src"
    time $CMAKE -DRDI_CCACHE=$ccache -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ../../src
  fi
  echo "make -j $jcore $verbose DESTDIR=$PWD install"
  time make -j $jcore $verbose DESTDIR=$PWD install
  time ctest --output-on-failure
  time make package
fi

if [[ $driver == 1 ]]; then
    unset CC
    unset CXX
    echo "make -C usr/src/xrt-2.4.0/driver/xocl"
    make -C usr/src/xrt-2.4.0/driver/xocl
    if [[ $CPU == "aarch64" ]]; then
	# I know this is dirty as it messes up the source directory with build artifacts but this is the
	# quickest way to enable native zocl build in Travis CI environment for aarch64
	ZOCL_SRC=`readlink -f ../../src/runtime_src/core/edge/drm/zocl`
	make -C $ZOCL_SRC
    fi
fi

if [[ $docs == 1 ]]; then
    echo "make xrt_docs"
    make xrt_docs
fi

if [[ $clangtidy == 1 ]]; then
    echo "make clang-tidy"
    make clang-tidy
fi

if [[ $checkpatch == 1 ]]; then
    # check only driver released files
    DRIVERROOT=`readlink -f $BUILDDIR/Release/usr/src/xrt-2.4.0/driver`

    # find corresponding source under src tree so errors can be fixed in place
    XOCLROOT=`readlink -f $BUILDDIR/../src/runtime_src/core/pcie/driver`
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
