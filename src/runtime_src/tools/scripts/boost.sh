#!/bin/bash

usage()
{
    echo "Usage: boost.sh [options] [<version>]"
    echo ""
    echo "Linux only script to pull a release version of Boost and "
    echo "build it locally for use with XRT.  This script builds"
    echo "static Boost link libraries compiled with -fPIC such that"
    echo "XRT shared libraries can link with static boost."
    echo ""
    echo "Version defaults to boost-1.71.0"
    echo ""
    echo "[-help]                    List this help"
    echo "[-prefix <path>]           Prefix for install and srcdir (default: $PWD/boost)"
    echo "[-install <path>]          Path to install directory (default: $PWD/boost/xrt)"
    echo "[-srcdir <path>]           Directory for boost sources (default: $PWD/boost/build)"
    echo "[-noclone]                 Don't clone fresh, use exist 'srcdir'"
    echo "[-nobuild]                 Don't build boost, just exit with message"
    echo ""
    echo "Clone, build, and install boost"
    echo "% boost.sh -prefix $HOME/tmp/boost"
    echo ""
    echo "Clone boost into $HOME/tmp/boost/build but don't build"
    echo "% boost.sh -prefix $HOME/tmp/boost -nobuild"
    echo ""
    echo "Build boost (from already cloned src) and install under $HOME/tmp/boost/xrt/u16"
    echo "% boost.sh -srcdir $HOME/tmp/boost/build -install $HOME/tmp/boost/xrt/u16"

    exit 1
}

version="boost-1.71.0"
prefix=$PWD/boost
srcdir=$prefix/build
install=$prefix/xrt
noclone=0
nobuild=0

while [ $# -gt 0 ]; do
    case "$1" in
        -help)
            usage
            ;;
        -prefix)
            shift
            prefix=$1
            srcdir=$prefix/build
            install=$prefix/xrt
            shift
            ;;
        -install)
            shift
            install=$1
            shift
            ;;
        -srcdir)
            shift
            srcdir=$1
            shift
            ;;
        -noclone)
            noclone=1
            shift
            ;;
        -nobuild)
            nobuild=1
            shift
            ;;
        *)
            version=$1
            shift
            ;;
    esac
done

if [[ ! -d $prefix ]]; then
    mkdir -p $prefix
    if [[ ! -d $prefix ]]; then
        echo "$prefix: No such directory"
        usage
    fi
fi

if [[ ! -d $srcdir ]]; then
    mkdir -p $srcdir
    if [[ ! -d $srcdir ]]; then
        echo "$srcdir: Does not exist and can't be created"
        usage
    fi
fi

if [[ $noclone == 0 ]] ; then
    echo "Cloning and building boost version '$version' in '$srcdir'"
fi
if [[ $nobuild == 0 ]] ; then
    echo "Building Boost from '$srcdir' and installing in '$install'"
fi

read -p "Ok to continue (Y/n): " answer
answer=${answer:-Y}
echo $answer
if [[ $answer != "Y" ]]; then
    echo "exiting ..."
    exit 1
fi

# Clone
if [[ $noclone == 0 ]]; then
    echo "git clone --recursive --branch=$version https://github.com/boostorg/boost.git $srcdir"
    git clone --recursive --branch=$version https://github.com/boostorg/boost.git $srcdir
    echo "Boost cloned into $srcdir"
fi

# Build
if [[ $nobuild == 0 ]]; then
    set here=$PWD
    cd $srcdir

    if [[ ! -e bootstrap.sh ]]; then
        echo "$srcdir/bootstrap.sh does not exist, some error occurred during cloning"
    fi

    ./bootstrap.sh > /dev/null
    ./b2 -a -d+2 cxxflags="-std=gnu++14 -fPIC" -j6 install --prefix=$install link=static --with-filesystem --with-program_options --with-system --layout=tagged

    # copy _mt-x64.a to .a, to faciliate linking with -lboost_<lib>
    for f in $(find $install/lib -type f -name *-mt-x64.a); do
        cp $f ${f//-mt-x64/}
    done
    for f in $(find $install/lib -type f -name *-mt-a64.a); do
        cp $f ${f//-mt-a64/}
    done

    cd $here
    echo "Boost installed in $prefix"
fi
echo ""
echo "Use local build of Boost with XRT:"
echo "% <xrt>/build.sh -clean"
echo "% <xrt>/build.sh -with-static-boost $install ..."

