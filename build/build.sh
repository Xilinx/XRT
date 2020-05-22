#!/bin/bash

set -e

OSDIST=`grep '^ID=' /etc/os-release | awk -F= '{print $2}' | tr -d '"'`
BUILDDIR=$(readlink -f $(dirname ${BASH_SOURCE[0]}))
CORE=`grep -c ^processor /proc/cpuinfo`
CMAKE=cmake
CPU=`uname -m`

if [[ $OSDIST == "centos" ]] || [[ $OSDIST == "amazon" ]] || [[ $OSDIST == "rhel" ]]; then
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
    echo "[-dbg]                     Build debug library only (default)"
    echo "[-opt]                     Build optimized library only (default)"
    echo "[-edge]                    Build edge of x64.  Turns off opt and dbg"
    echo "[-nocmake]                 Skip CMake call"
    echo "[-noctest]                 Skip unit tests"
    echo "[-j <n>]                   Compile parallel (default: system cores)"
    echo "[-ccache]                  Build using RDI's compile cache"
    echo "[-toolchain <file>]        Extra toolchain file to configure CMake"
    echo "[-driver]                  Include building driver code"
    echo "[-checkpatch]              Run checkpatch.pl on driver code"
    echo "[-verbose]                 Turn on verbosity when compiling"
    echo "[-ertfw <dir>]             Path to directory with pre-built ert firmware (default: build the firmware)"
    echo ""
    echo "ERT firmware is built if and only if MicroBlaze gcc compiler can be located."
    echo "When compiler is not accesible, use -ertfw to specify path to directory with"
    echo "pre-built ert fw to include in XRT packages"
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
edge=0
nocmake=0
noctest=0
ertfw=""
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
        -ertfw)
            shift
            ertfw=$1
            shift
            ;;
        -edge)
            shift
            edge=1
            opt=0
            dbg=0
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
        -noctest)
            noctest=1
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
        -toolchain)
            shift
            toolchain=$1
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

debug_dir=${DEBUG_DIR:-Debug}
release_dir=${REL_DIR:-Release}
edge_dir=${EDGE_DIR:-Edge}

here=$PWD
cd $BUILDDIR

if [[ $clean == 1 ]]; then
    echo $PWD
    echo "/bin/rm -rf $debug_dir $release_dir"
    /bin/rm -rf $debug_dir $release_dir $edge_dir
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

if [[ ! -z $ertfw ]]; then
    echo "export XRT_FIRMWARE_DIR=$ertfw"
    export XRT_FIRMWARE_DIR=$ertfw
fi

# we pick microblaze toolchain from Vitis install
if [[ -z ${XILINX_VITIS:+x} ]]; then
    export XILINX_VITIS=/proj/xbuilds/2019.2_released/installs/lin64/Vitis/2019.2
fi

if [[ $dbg == 1 ]]; then
  mkdir -p $debug_dir
  cd $debug_dir
  if [[ $nocmake == 0 ]]; then
	echo "$CMAKE -DRDI_CCACHE=$ccache -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_TOOLCHAIN_FILE=$toolchain ../../src"
	time $CMAKE -DRDI_CCACHE=$ccache -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_TOOLCHAIN_FILE=$toolchain ../../src
  fi
  echo "make -j $jcore $verbose DESTDIR=$PWD install"
  time make -j $jcore $verbose DESTDIR=$PWD install
  if [[ $noctest == 0 ]]; then
      time ctest --output-on-failure
  fi
  cd $BUILDDIR
fi

if [[ $opt == 1 ]]; then
  mkdir -p $release_dir
  cd $release_dir
  if [[ $nocmake == 0 ]]; then
	echo "$CMAKE -DRDI_CCACHE=$ccache -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_TOOLCHAIN_FILE=$toolchain ../../src"
	time $CMAKE -DRDI_CCACHE=$ccache -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_TOOLCHAIN_FILE=$toolchain ../../src
  fi

  if [[ $docs == 1 ]]; then
    echo "make xrt_docs"
    make xrt_docs
  else
    echo "make -j $jcore $verbose DESTDIR=$PWD install"
    time make -j $jcore $verbose DESTDIR=$PWD install
    if [[ $noctest == 0 ]]; then
        time ctest --output-on-failure
    fi
    time make package
  fi

  if [[ $driver == 1 ]]; then
    unset CC
    unset CXX
    echo "make -C usr/src/xrt-2.7.0/driver/xocl"
    make -C usr/src/xrt-2.7.0/driver/xocl
    if [[ $CPU == "aarch64" ]]; then
	# I know this is dirty as it messes up the source directory with build artifacts but this is the
	# quickest way to enable native zocl build in Travis CI environment for aarch64
	ZOCL_SRC=`readlink -f ../../src/runtime_src/core/edge/drm/zocl`
	make -C $ZOCL_SRC
    fi
  fi
  cd $BUILDDIR
fi

# Verify compilation on edge
if [[ $CPU != "aarch64" ]] && [[ $edge == 1 ]]; then
  mkdir -p $edge_dir
  cd $edge_dir
  if [[ $nocmake == 0 ]]; then
    echo "$CMAKE -DRDI_CCACHE=$ccache -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ../../src"
    time env XRT_NATIVE_BUILD=no $CMAKE -DRDI_CCACHE=$ccache -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ../../src
  fi
  echo "make -j $jcore $verbose DESTDIR=$PWD"
  time make -j $jcore $verbose DESTDIR=$PWD
  cd $BUILDDIR
fi
    
    
if [[ $clangtidy == 1 ]]; then
    echo "make clang-tidy"
    make clang-tidy
fi

if [[ $checkpatch == 1 ]]; then
    # check only driver released files
    DRIVERROOT=`readlink -f $BUILDDIR/$release_dir/usr/src/xrt-2.7.0/driver`

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
