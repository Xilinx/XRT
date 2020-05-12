#!/bin/bash

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
    echo "[clean|-clean]             Remove build directories"
    exit 1
}

clean=0
nocmake=0
while [ $# -gt 0 ]; do
    case "$1" in
        -help)
            usage
            ;;
        -nocmake)
            nocmake=1
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
