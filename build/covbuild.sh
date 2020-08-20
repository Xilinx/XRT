#!/bin/bash

set -e

# Prepare a Coverity Scan build of XRT for manual upload to
# the XRT project on scan.coverity.com
#
# Usage:
#  # Configure the compilers, only needed when building from clean slate
#  % covbuild.sh --configure
#
#  # Build XRT
#  % covbuild.sh
#  ...
#  Coverity build completed, make sure it was successful, then upload xrt.tgz to scan.coverity.com
#  file: /proj/xsjhdstaff3/soeren/git/stsoe/XRT.coverity/build/coverity/xrt.tgz
#  Project Version: 2.7.0.c
#  Description/tag: 6e449a961eda6efa705f3480337b4e4347e3f27b
#
# Update to scan.coverity.com using the version and description per covbuild.sh output
# Make sure to upload from a host with direct access to xrt.tgz (342M)

OSDIST=`lsb_release -i |awk -F: '{print tolower($2)}' | tr -d ' \t'`
BUILDDIR=$(readlink -f $(dirname ${BASH_SOURCE[0]}))
CORE=`grep -c ^processor /proc/cpuinfo`
CMAKE=cmake
#COVBIN=/home/soeren/tools/coverity/cov-analysis-linux64-2017.07/bin
COVBIN=/home/soeren/tools/coverity/cov-analysis-linux64-2019.03/bin

if [[ $OSDIST == "centos" ]]; then
    CMAKE=cmake3
    if [[ ! -x "$(command -v $CMAKE)" ]]; then
        echo "$CMAKE is not installed, please run xrtdeps.sh"
        exit 1
    fi
fi

#if [[ $(git branch | grep '*' | cut -d ' ' -f2) != "2018.3" ]]; then
#    echo "Currently only analyzing 2018.3"
#    echo "covbuild.sh must be run on branch 2018.3"
#    exit
#fi

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
configure=0
while [ $# -gt 0 ]; do
    case "$1" in
        -help)
            usage
            ;;
        clean|-clean)
            clean=1
            shift
            ;;
        -configure|--configure)
            configure=1
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

if [ $configure == 1 ]; then
    $COVBIN/cov-configure --template --config $BUILDDIR/coverity/config/conf.xml --compiler c++
    $COVBIN/cov-configure --template --config $BUILDDIR/coverity/config/conf.xml --compiler gcc
    exit 0
fi

mkdir -p coverity
cd coverity
/bin/rm -f xrt.tgz
imed=$PWD/cov-int
$CMAKE -DCMAKE_BUILD_TYPE=Release ../../src
$COVBIN/cov-build --config $BUILDDIR/coverity/config/conf.xml --dir $imed make -j4
make DESTDIR=$PWD install
cd usr/src/xrt-2.7.0/driver/xocl
$COVBIN/cov-build --config $BUILDDIR/coverity/config/conf.xml --dir $imed make
cd $BUILDDIR/coverity
tar czvf xrt.tgz cov-int
cd $here

if [[ -e $BUILDDIR/coverity/xrt.tgz ]]; then
    echo "Coverity build completed, make sure it was successful, then upload xrt.tgz to scan.coverity.com"
    echo "file: $BUILDDIR/coverity/xrt.tgz"
    echo "Project Version: 2.7.0.c"
    echo "Description/tag: $(git rev-parse HEAD)"
fi
