#!/bin/bash

# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2020-2022 Xilinx, Inc. All rights reserved.
# Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

################################################################
# Use this script to build XRT low level tests using CMake
# The script can be used on both Linux and Windows WSL in bash
#
# Make sure to set XILINX_XRT prior to running the script
#
# % <path-to-this-directory>/build22.sh -help
#
# The test executables are installed under
# build/{Release,Debug,WRelease,WDebug}
################################################################

OSDIST=`awk -F= '$1=="ID" {print $2}' /etc/os-release | tr -d '"' | awk '{print tolower($1)}'`
BUILDDIR=$(readlink -f $(dirname ${BASH_SOURCE[0]}))
WSL=0
CMAKE=cmake

if [[ $OSDIST == "centos" ]] || [[ $OSDIST == "amzn" ]]; then
    CMAKE=cmake3
    if [[ ! -x "$(command -v $CMAKE)" ]]; then
        echo "$CMAKE is not installed, please run xrtdeps.sh"
        exit 1
    fi
fi

if [[ -z $XILINX_XRT ]]; then
    echo "Please set XILINX_XRT (source xrt setup script)"
    exit 1
fi

if [[ "$(< /proc/sys/kernel/osrelease)" == *icrosoft* ]]; then
    WSL=1
    CMAKE="/mnt/c/Program Files/CMake/bin/cmake.exe"
    XILINX_XRT=$(sed -e 's|/mnt/\([A-Za-z]\)/\(.*\)|\1:/\2|' -e 's|/|\\|g' <<< $XILINX_XRT)
fi


cmake_flags="-DXILINX_XRT=$XILINX_XRT"

usage()
{
    echo "Usage: build.sh [options]"
    echo
    echo "[-help]                    List this help"
    echo "[-nocmake]                 Skip CMake call"
    echo "[-clean]                   Remove build directories"
    echo "[-dbg]                     Create a debug build"
    echo "[-opt]                     Create a release build"
    echo "[-nocmake]                 Do not generated makefiles"
    echo ""
    echo "The test executables are installed under"
    echo "build/{WDebug,WRelease,Debug,Release}/<testname>/"
    exit 1
}

clean=0
nocmake=0
dbg=1
release=1
em=""
while [ $# -gt 0 ]; do
    case "$1" in
        -help)
            usage
            ;;
        -nocmake)
            nocmake=1
            shift
            ;;
        -clean)
            clean=1
            shift
            ;;
	-dbg)
	    release=0
	    shift
	    ;;
	-opt)
	    dbg=0
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
echo $cmake_flags

if [[ $clean == 1 ]]; then
    echo $PWD
    echo "/bin/rm -rf WRelease WDebug Release Debug ../*/build/"
    /bin/rm -rf WRelease WDebug Release Debug ../*/build/
    exit 0
fi

if [[ $dbg == 1 ]]; then
    cmake_flags+=" -DCMAKE_BUILD_TYPE=Debug"

    if [[ $WSL == 1 ]]; then
	mkdir WDebug
	cd WDebug
    else
	mkdir Debug
	cd Debug
    fi

    if [[ $nocmake == 0 ]]; then
	"$CMAKE" -DCMAKE_BUILD_TYPE=Debug $cmake_flags ../..
    fi

    "$CMAKE" --build . --config Debug --target install
    cd $BUILDDIR
fi

if [[ $release == 1 ]]; then
    cmake_flags+=" -DCMAKE_BUILD_TYPE=Release"

    if [[ $WSL == 1 ]]; then
	mkdir WRelease
	cd WRelease
    else
	mkdir Release
	cd Release
    fi

    if [[ $nocmake == 0 ]]; then
	"$CMAKE" -DCMAKE_BUILD_TYPE=Release $cmake_flags ../..
    fi

    "$CMAKE" --build . --config Release --target install
    cd $BUILDDIR
fi

cd $here
