#!/bin/bash

set -e

BUILDDIR=$(readlink -f $(dirname ${BASH_SOURCE[0]}))

usage()
{
    echo "Usage: build.sh [options]"
    echo
    echo "[-help]                    List this help"
    echo "[clean|-clean]             Remove build directories"
    echo "[-coverity]                Run a Coverity build, requires admin priviledges to Coverity"

    exit 1
}

clean=0
covbuild=0
while [ $# -gt 0 ]; do
    case "$1" in
        -help)
            usage
            ;;
        clean|-clean)
            clean=1
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
    /bin/rm -rf Release Debug Coverity
    exit 0
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
    make -j4 COVUSER=$covuser COVPW=$covpw DATE="`git rev-parse --short HEAD`" coverity
    cd $here
    exit 0
fi

mkdir -p Debug Release
cd Debug
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ../../src
make -j4 DESTDIR=$PWD install
cd $BUILDDIR

cd Release
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ../../src
make -j4 DESTDIR=$PWD install
make package
cd $here
