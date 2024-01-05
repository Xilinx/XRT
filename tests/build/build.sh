#!/bin/bash -x

# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
#
# Script to enable compilation of testcases (host code only) in the
# XRT/tests directory.

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

# Use GCC 9 on CentOS 8 and RHEL 8 for std::filesystem
# The dependency is installed by xrtdeps.sh
if [[ $CPU == "x86_64" ]] && [[ $OSDIST == "centos" || $OSDIST == "rhel" ]] && [[ $MAJOR == 8 ]]; then
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
    echo "[-hip]                      Enable hip bindings"
    echo "[-disable-werror]           Disable compilation with warnings as error"
    echo "[-noinit]                   Do not initialize Git submodules"
    echo "[-clangtidy]                Run clang-tidy as part of build"
    echo "[-cppstd]                   Cpp standard (default: 17)"
    echo "[-j <n>]                    Compile parallel (default: system cores)"
    echo "[-toolchain <file>]         Extra toolchain file to configure CMake"
    echo "[-verbose]                  Turn on verbosity when compiling"
    echo ""
    echo "Compile the test cases in the tests directory. Previously compiled XRT "
    echo "build must be available in XRT/build/Debug and or XRT/build/Release area(s)."
    echo ""

    exit 1
}

clean=0
ccache=0
ci=0
docs=0
verbose=""
jcore=$CORE
opt=1
dbg=1
nocmake=0
werror=1
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
        -dbg)
            dbg=1
            opt=0
            shift
            ;;
        -hip)
            shift
            cmake_flags+=" -DXRT_ENABLE_HIP=ON"
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
        -disable-werror|--disable-werror)
            werror=0
            shift
            ;;
        -j)
            shift
            jcore=$1
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
        -clangtidy)
            cmake_flags+=" -DXRT_CLANG_TIDY=ON"
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
        *)
            echo "unknown option"
            usage
            ;;
    esac
done

debug_dir=${DEBUG_DIR:-Debug}
release_dir=${REL_DIR:-Release}

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

if [[ $dbg == 1 ]]; then
  mkdir -p $debug_dir
  cd $debug_dir

  cmake_flags+=" -DCMAKE_BUILD_TYPE=Debug"

  source ../../../build/Debug/opt/xilinx/xrt/setup.sh
  if [[ $nocmake == 0 ]]; then
	  echo "$CMAKE $cmake_flags ../../src"
	  time $CMAKE $cmake_flags ../../
  fi

  echo "make -j $jcore $verbose DESTDIR=$PWD"
  time make -j $jcore $verbose DESTDIR=$PWD

  cd $BUILDDIR
fi

if [[ $opt == 1 ]]; then
  mkdir -p $release_dir
  cd $release_dir

  cmake_flags+=" -DCMAKE_BUILD_TYPE=Release"

  source ../../../build/Release/opt/xilinx/xrt/setup.sh
  if [[ $nocmake == 0 ]]; then
	  echo "$CMAKE $cmake_flags ../../"
	  time $CMAKE $cmake_flags ../../
  fi

  echo "make -j $jcore $verbose DESTDIR=$PWD"
  time make -j $jcore $verbose DESTDIR=$PWD

  cd $BUILDDIR
fi

cd $here
