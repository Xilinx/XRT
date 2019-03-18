#!/bin/bash

set -e

OSDIST=`lsb_release -i |awk -F: '{print tolower($2)}' | tr -d ' \t'`
BUILDDIR=$(readlink -f $(dirname ${BASH_SOURCE[0]}))
CORE=`grep -c ^processor /proc/cpuinfo`
CMAKE=cmake
COVBIN=/home/soeren/tools/coverity/cov-analysis-linux64-2017.07/bin

if [[ $OSDIST == "centos" ]]; then
    CMAKE=cmake3
    if [[ ! -x "$(command -v $CMAKE)" ]]; then
        echo "$CMAKE is not installed, please run xrtdeps.sh"
        exit 1
    fi
fi

if [[ $(git branch | grep '*' | cut -d ' ' -f2) != "2018.3" ]]; then
    echo "Currently only analyzing 2018.3"
    echo "covbuild.sh must be run on branch 2018.3"
    exit
fi

usage()
{
    echo "Usage: cov.sh [options]"
    echo
    echo "[-help]                    List this help"
    echo "[clean|-clean]             Remove build directories"
    exit 1
}

clean=0
ccache=1
while [ $# -gt 0 ]; do
    case "$1" in
        -help)
            usage
            ;;
        clean|-clean)
            clean=1
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

if [ $clean == 1 ]; then
    echo $PWD
    echo "/bin/rm -rf coverity"
    /bin/rm -rf coverity
    exit 0
fi

mkdir -p coverity
cd coverity
/bin/rm xrt.tgz
imed=$PWD/cov-int
$CMAKE -DCMAKE_BUILD_TYPE=Release ../../src
$COVBIN/cov-build --dir $imed make -j4
make DESTDIR=$PWD install
cd usr/src/xrt-2.1.0/driver/xclng/drm/xocl
$COVBIN/cov-build --dir $imed make
cd $BUILDDIR/coverity
tar czvf xrt.tgz cov-int
cd $here

if [[ -e $BUILDDIR/coverity/xrt.tgz ]]; then
    echo "Coverity build completed, make sure it was successful, then upload xrt.tgz to scan.coverity.com"
    echo "file: $BUILDDIR/coverity/xrt.tgz"
    echo "Project Version: 2.1.0"
    echo "Description/tag: $(git rev-parse HEAD)"
fi
