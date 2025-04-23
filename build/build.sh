#!/bin/bash

set -e

OSDIST=`grep '^ID=' /etc/os-release | awk -F= '{print $2}' | tr -d '"'`
VERSION=`grep '^VERSION_ID=' /etc/os-release | awk -F= '{print $2}' | tr -d '"'`
MAJOR=${VERSION%.*}
BUILDDIR=$(readlink -f $(dirname ${BASH_SOURCE[0]}))
CORE=`grep -c ^processor /proc/cpuinfo`
CMAKE=cmake
CMAKE_MAJOR_VERSION=`cmake --version | head -n 1 | awk '{print $3}' |awk -F. '{print $1}'`
CPU=`uname -m`

if [[ $CMAKE_MAJOR_VERSION != 3 ]]; then
    if [[ $OSDIST == "centos" ]] || [[ $OSDIST == "amzn" ]] || [[ $OSDIST == "rhel" ]] || [[ $OSDIST == "fedora" ]] || [[ $OSDIST == "mariner" ]] || [[ $OSDIST == "almalinux" ]]; then
        CMAKE=cmake3
        if [[ ! -x "$(command -v $CMAKE)" ]]; then
            echo "$CMAKE is not installed, please run xrtdeps.sh"
            exit 1
        fi
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

# Use GCC 9 on CentOS 8, RHEL 8, AlmaLinux 8 for std::filesystem
# The dependency is installed by xrtdeps.sh
if [[ $CPU == "x86_64" ]] && [[ $OSDIST == "centos" || $OSDIST == "rhel" || $OSDIST == "almalinux" ]] && [[ $MAJOR == 8 ]]; then
    source /opt/rh/gcc-toolset-9/enable
fi

usage()
{
    echo "Usage: build.sh [options]"
    echo
    echo "[-help]                     List this help"
    echo "[clean|-clean]              Remove build directories"
    echo "[-ci]                       Build is initiated by CI"
    echo "[-dbg]                      Build debug library only (default)"
    echo "[-opt]                      Build optimized library only (default)"
    echo "[-edge]                     Build edge of x64.  Turns off opt and dbg"
    echo "[-hip]                      Enable hip bindings"
    echo "[-disable-werror]           Disable compilation with warnings as error"
    echo "[-nocmake]                  Skip CMake call"
    echo "[-noert]                    Do not treat missing ERT FW as a build error"
    echo "[-noinit]                   Do not initialize Git submodules"
    echo "[-noctest]                  Skip unit tests"
    echo "[-npu]                      Build for NPU only, implies -noert and disables bundling of Alveo Linux drivers"
    echo "[-with-static-boost <boost> Build binaries using static linking of boost from specified boost install"
    echo "[-clangtidy]                Run clang-tidy as part of build"
    echo "[-pskernel]                 Enable building of POC ps kernel"
    echo "[-cppstd]                   Cpp standard (default: 17)"
    echo "[-docs]                     Enable documentation generation with sphinx"
    echo "[-j <n>]                    Compile parallel (default: system cores)"
    echo "[-ccache]                   Build using RDI's compile cache"
    echo "[-toolchain <file>]         Extra toolchain file to configure CMake"
    echo "[-driver]                   Include building driver code"
    echo "[-xclbinutil]               Build xclbinutil only"
    echo "[-checkpatch]               Run checkpatch.pl on driver code"
    echo "[-verbose]                  Turn on verbosity when compiling"
    echo "[-install_prefix <path>]    set CMAKE_INSTALL_PREFIX to path"
    echo "[-ertbsp <dir>]             Path to directory with pre-downloaded BSP files for building ERT (default: download BSP files during build time)"
    echo "[-ertfw <dir>]              Path to directory with pre-built ert firmware (default: build the firmware)"
    echo ""
    echo "ERT firmware is built if and only if MicroBlaze gcc compiler can be located."
    echo "When compiler is not accesible, use -ertfw to specify path to directory with"
    echo "pre-built ert fw to include in XRT packages"
    echo ""
    echo "Compile caching is enabled with '-ccache' but requires access to internal network."
    echo
    echo "Linking of XRT with static boost, resolves all boost references in XRT binaries."
    echo "The specified install of boost must be compiled with -fPIC.  The script "
    echo "src/runtime_src/tools/script/boost.sh can be used to download and build boost"
    echo "for use with the '-with-static-boost' option."

    exit 1
}

clean=0
ci=0
docs=0
verbose=""
driver=0
checkpatch=0
jcore=$CORE
opt=1
dbg=1
edge=0
nocmake=0
init_submodule=1
nobuild=0
noctest=0
noert=0
static_boost=""
ertbsp=""
ertfw=""
werror=1
alveo_build=0
npu_build=0
base_build=0
xclbinutil=0
xrt_install_prefix="/opt/xilinx"
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
        -ci)
            ci=1
            shift
            ;;
        -noert)
            noert=1
            shift
            ;;
        -dbg)
            dbg=1
            opt=0
            shift
            ;;
        -ertbsp)
            shift
            ertbsp=$1
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
        -hip)
            shift
            cmake_flags+=" -DXRT_ENABLE_HIP=ON"
            ;;
        -base)
            shift
            base_build=1
            noert=1
            cmake_flags+=" -DXRT_BASE=1"
            ;;
        -alveo)
            shift
            alveo_build=1
            cmake_flags+=" -DXRT_ALVEO=1"
            ;;
	-npu)
            shift
	    npu_build=1
	    noert=1
	    cmake_flags+=" -DXRT_NPU=1"
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
        -noinit)
            init_submodule=0
            shift
            ;;
        -noctest)
            noctest=1
            shift
            ;;
        -disable-werror|--disable-werror)
            werror=0
            shift
            ;;
        -j)
            shift
            jcore=$1
            shift
            ;;
        -ccache)
            export CCACHE_DIR=${CCACHE_DIR:-/scratch/ccache/$USER}
            mkdir -p $CCACHE_DIR
            cmake_flags+=" -DXRT_CCACHE=1"
            shift
            ;;
        -cppstd)
            shift
            cmake_flags+=" -DCMAKE_CXX_STANDARD=$1"
            shift
            ;;
        -toolchain)
            shift
            cmake_flags+=" -DCMAKE_TOOLCHAIN_FILE=$1"
            shift
            ;;
        -checkpatch)
            checkpatch=1
            shift
            ;;
	docs|-docs)
            docs=1
            dbg=0
            nobuild=1
            shift
            ;;
        -driver)
            driver=1
            shift
            ;;
        -clangtidy)
            cmake_flags+=" -DXRT_CLANG_TIDY=ON"
            shift
            ;;
        -pskernel)
            cmake_flags+=" -DXRT_PSKERNEL_BUILD=ON"
            shift
            ;;
        -xclbinutil)
            xclbinutil=1
            shift
            ;;
        -verbose)
            verbose="VERBOSE=1"
            shift
            ;;
        -install_prefix)
            shift
            xrt_install_prefix=$1
            shift
            ;;
        -with-static-boost)
            shift
            static_boost=$1
            shift
            ;;
        *)
            echo "unknown option"
            usage
            ;;
    esac
done

if [[ $((npu_build + alveo_build + base_build)) > 1 ]]; then
    echo "build.sh: -npu, -alveo, -base are mutually exclusive"
    exit 1
fi

debug_dir=${DEBUG_DIR:-Debug}
release_dir=${REL_DIR:-Release}
edge_dir=${EDGE_DIR:-Edge}

# By default compile with warnings as errors.
# Update every time CMake is generating makefiles.
# Disable with '-disable-werror' option.
cmake_flags+=" -DXRT_ENABLE_WERROR=$werror"

# set CMAKE_INSTALL_PREFIX
cmake_flags+=" -DCMAKE_INSTALL_PREFIX=$xrt_install_prefix -DXRT_INSTALL_PREFIX=$xrt_install_prefix"

here=$PWD
cd $BUILDDIR

if [[ $clean == 1 ]]; then
    echo $PWD
    echo "/bin/rm -rf $debug_dir $release_dir $edge_dir"
    /bin/rm -rf $debug_dir $release_dir $edge_dir
    exit 0
fi

if [[ ! -z $ertbsp ]]; then
    echo "export ERT_BSP_DIR=$ertbsp"
    export ERT_BSP_DIR=$ertbsp
fi

if [[ ! -z $ertfw ]]; then
    echo "export XRT_FIRMWARE_DIR=$ertfw"
    export XRT_FIRMWARE_DIR=$ertfw
fi

if [[ ! -z $static_boost ]]; then
    echo "export XRT_BOOST_INSTALL=$static_boost"
    export XRT_BOOST_INSTALL=$static_boost
fi

if [[ ! -z ${XRT_BOOST_INSTALL:+x} ]]; then
    if [[ ! -d ${XRT_BOOST_INSTALL}/include ]] || [[ ! -d ${XRT_BOOST_INSTALL}/lib ]]; then
        echo "Incomplete boost install specified, please use runtime_src/tools/script/boost.sh"
        exit 1
    fi
    echo "Linking XRT binaries with static boost libraries"
fi

# we pick microblaze toolchain from Vitis install
if [[ -z ${XILINX_VITIS:+x} ]] || [[ ! -d ${XILINX_VITIS} ]]; then
    export XILINX_VITIS=/proj/xbuilds/2023.1_released/installs/lin64/Vitis/2023.1
    if [[ ! -d ${XILINX_VITIS} ]]; then
        echo "****************************************************************"
        echo "* XILINX_VITIS is undefined or not accessible.                 *"
        echo "* MicroBlaze firmware will not be built.                       *"
        echo "* To treat as a warning use -noert option.                     *"
        echo "****************************************************************"
        # The ERT FW is needed when platform packages are installed and
        # creates xsabin files. Without FW the XRT build cannot be used
        # to install platform packages.
        if [[ $noert == 0 ]]; then
            exit 1
        fi
    fi
fi

#If git modules config file exist then try to clone them
GIT_MODULES=$BUILDDIR/../.gitmodules
if [[ -f "$GIT_MODULES" && $init_submodule == 1 ]]; then
    cd $BUILDDIR/../
    echo "Updating Git XRT submodule, use -noinit option to avoid updating"
    git submodule update --init --recursive
    cd $BUILDDIR
fi

if [[ $dbg == 1 ]]; then
  mkdir -p $debug_dir
  cd $debug_dir

  cmake_flags+=" -DCMAKE_BUILD_TYPE=Debug"

  if [[ $nocmake == 0 ]]; then
    if [[ $xclbinutil == 1 ]]; then
      # xclbinutil only build
      cmake_flags+=" -DXRT_SOURCE_DIR=$BUILDDIR/../src"
      echo "$CMAKE $cmake_flags ../../src/runtime_src/tools/xclbinutil"
      time $CMAKE $cmake_flags ../../src/runtime_src/tools/xclbinutil
    else
      # Full build
      echo "$CMAKE $cmake_flags ../../src"
      time $CMAKE $cmake_flags ../../src
    fi
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

  cmake_flags+=" -DCMAKE_BUILD_TYPE=Release"

  if [[ $nocmake == 0 ]]; then
    if [[ $xclbinutil == 1 ]]; then
      # xclbinutil only build
      cmake_flags+=" -DXRT_SOURCE_DIR=$BUILDDIR/../src"
      echo "$CMAKE $cmake_flags ../../src/runtime_src/tools/xclbinutil"
      time $CMAKE $cmake_flags ../../src/runtime_src/tools/xclbinutil
    else
      # Full build
      echo "$CMAKE $cmake_flags ../../src"
      time $CMAKE $cmake_flags ../../src
    fi
  fi

  if [[ $nobuild == 0 ]]; then
      echo "make -j $jcore $verbose DESTDIR=$PWD install"
      time make -j $jcore $verbose DESTDIR=$PWD install

      if [[ $noctest == 0 ]]; then
          time ctest --output-on-failure
      fi

      time make package
  fi

  if [[ $docs == 1 ]]; then
      echo "make xrt_docs"
      make xrt_docs
  fi

  if [[ $driver == 1 ]]; then
    unset CC
    unset CXX
    echo "make -C usr/src/xrt-2.20.0/driver/xocl"
    make -C usr/src/xrt-2.20.0/driver/xocl
    if [[ $CPU == "aarch64" ]]; then
	# I know this is dirty as it messes up the source directory with build artifacts but this is the
	# quickest way to enable native zocl build in Travis CI environment for aarch64
	ZOCL_SRC=`readlink -f ../../src/runtime_src/core/edge/drm/zocl`
	make -C $ZOCL_SRC EXTRA_CFLAGS=-D__NONE_PETALINUX__
    fi
  fi
  cd $BUILDDIR
fi

# Verify compilation on edge
if [[ $CPU != "aarch64" ]] && [[ $edge == 1 ]]; then
  mkdir -p $edge_dir
  cd $edge_dir

  cmake_flags+=" -DCMAKE_BUILD_TYPE=Release"

  if [[ $nocmake == 0 ]]; then
    echo "env XRT_NATIVE_BUILD=no $CMAKE $cmake_flags ../../src"
    time env XRT_NATIVE_BUILD=no $CMAKE $cmake_flags ../../src
  fi
  echo "make -j $jcore $verbose DESTDIR=$PWD"
  time make -j $jcore $verbose DESTDIR=$PWD
  cd $BUILDDIR
fi


if [[ $checkpatch == 1 ]]; then
    # check only driver released files
    DRIVERROOT=`readlink -f $BUILDDIR/$release_dir/usr/src/xrt-2.20.0/driver`

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
