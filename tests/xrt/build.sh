#!/bin/bash

################################################################
# Use this script to build XRT low level tests using CMake
# The script can be used on both Linux and Windows WSL in bash
#
# Make sure to set XILINX_XRT prior to running the script
#
# % <path-to-this-directory>/build.sh -help
#
# The test executables are installed under
# build/{Linux,Windows}/Debug/{hw,hw_emu,sw_emu}/<testname>/
################################################################



OSDIST=`lsb_release -i |awk -F: '{print tolower($2)}' | tr -d ' \t'`
SRCDIR=$(readlink -f $(dirname ${BASH_SOURCE[0]}))
BUILDDIR=$SRCDIR/build
CMAKEDIR+="$BUILDDIR/cmake"
WSL=0
CMAKE=cmake

if [[ $OSDIST == "centos" ]] || [[ $OSDIST == "amazon" ]]; then
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

if [[ "$(< /proc/sys/kernel/osrelease)" == *Microsoft ]]; then
    WSL=1
    CMAKEDIR+="/windows"
    CMAKE=cmake.exe

    XILINX_XRT=$(sed -e 's|/mnt/\([A-Za-z]\)/\(.*\)|\1:/\2|' -e 's|/|\\|g' <<< $XILINX_XRT)
    SRCDIR=$(sed -e 's|/mnt/\([A-Za-z]\)/\(.*\)|\1:/\2|' -e 's|/|\\|g' <<< $SRCDIR)
    CMAKEGEN="-G \"Visual Studio 15 2017 Win64\""
else
    CMAKEDIR+="/linux"
fi

usage()
{
    echo "Usage: build.sh [options]"
    echo
    echo "[-help]                    List this help"
    echo "[-nocmake]                 Skip CMake call"
    echo "[-em <hw_emu | sw_emu>]    Link for emulation mode"
    echo "[clean|-clean]             Remove build directories"
    echo ""
    echo "The test executables are installed under"
    echo "build/{Linux,Windows}/Debug/{hw,hw_emu,sw_emu}/<testname>/"
    exit 1
}

clean=0
nocmake=0
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
        -em)
            shift
            echo "x=$1"
            em=$1
            shift
            ;;
        clean|-clean)
            clean=1
            shift
            ;;
        *)
            echo "unknown option '$1'"
            usage
            ;;
    esac
done

if [[ $clean == 1 ]]; then
    echo $PWD
    echo "/bin/rm -rf $BUILDDIR"
    /bin/rm -rf $BUILDDIR
    exit 0
fi

if [[ "X$em" != "X" ]]; then
    echo "Building for $em emulation mode"
    export XCL_EMULATION_MODE=$em
    CMAKEDIR+="/$em"
else
    CMAKEDIR+="/hw"
fi

here=$PWD
mkdir -p $CMAKEDIR
cd $CMAKEDIR
if [[ $nocmake == 0 ]]; then
  if [[ $WSL == 0 ]]; then
      $CMAKE -DCMAKE_BUILD_TYPE=Debug -DXILINX_XRT=$XILINX_XRT $SRCDIR
  else
      $CMAKE -G "Visual Studio 15 2017 Win64" -DCMAKE_BUILD_TYPE=Debug -DXILINX_XRT=$XILINX_XRT $SRCDIR
  fi
fi
$CMAKE --build . --config Debug --target install
cd $here
