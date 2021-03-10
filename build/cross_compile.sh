#!/bin/bash

set -e

error()
{
    echo "ERROR: $1" 1>&2
    usage_and_exit 1
}

usage()
{
    echo "Usage: $PROGRAM [options] "
    echo "  options:"
    echo "          -help                           Print this usage"
    echo "          -aarch                          Architecture <aarch32/aarch64>"
    echo "          -sysroot                        Path to sysroot of target architecture"
    echo "          -compiler                       [optional] Path to 'gcc' cross compiler"
    echo "                                          If not specified default paths in host will be searched"
    echo "          -clean, clean                   Remove build directories"
    echo ""
}

usage_and_exit()
{
    usage
    exit $1
}

SAVED_OPTIONS=$(set +o)
# Don't print all commands
set +x
# Get the canonical file name of the current script
THIS_SCRIPT=`readlink -f ${BASH_SOURCE[0]}`
 
PROGRAM=`basename $0`

BUILDDIR=$(readlink -f $(dirname ${BASH_SOURCE[0]}))
CORE=`grep -c ^processor /proc/cpuinfo`
CMAKE=cmake
source /etc/os-release
OS_HOST=$ID

if [[ $OS_HOST == "centos" ]] || [[ $OS_HOST == "rhel" ]]; then
    CMAKE=cmake3
    if [[ ! -x "$(command -v $CMAKE)" ]]; then
        echo "$CMAKE is not installed, please install it"
        exit 1
    fi
fi

clean=0
jcore=$CORE

while [ $# -gt 0 ]; do
        case $1 in
                -help )
                        usage_and_exit 0
                        ;;
                -aarch )
                        shift
                        AARCH=$1
                        ;;
                -sysroot )
                        shift
                        SYSROOT=$1
                        ;;
                -clean | clean )
                        clean=1
                        ;;
                -compiler )
                        shift
                        COMPILER=$1
                        ;;
                 --* | -* )
                        error "Unregognized option: $1"
                        ;;
                 * )
                        error "Unregognized option: $1"
                        ;;
        esac
        shift
done

edge_dir_aarch64="Edge_aarch64"
edge_dir_aarch32="Edge_aarch32"
toolchain="toolchain-edge.cmake"

here=$PWD
cd $BUILDDIR

if [[ $clean == 1 ]]; then
    echo $PWD
    echo "/bin/rm -rf $edge_dir_aarch64 $edge_dir_aarch32"
    /bin/rm -rf $edge_dir_aarch64 $edge_dir_aarch32 
    exit 0
fi
 
# Sanity Check
if [ -z $AARCH ] || [ -z $SYSROOT ] ; then
    error "Please provide the required options 'aarch', 'sysroot'"
fi

SYSROOT=`readlink -f $SYSROOT`
if [ ! -d $SYSROOT ]; then
    error "SYSROOT is not accessible"
fi

if [ $AARCH == "aarch64" ]; then
    mkdir -p $edge_dir_aarch64 
    cd $edge_dir_aarch64
elif [ $AARCH == "aarch32" ]; then
    mkdir -p $edge_dir_aarch32
    cd $edge_dir_aarch32
    AARCH="arm"
else
    error "$AARCH not exist"
fi

if [ -z $COMPILER ]; then
    echo "INFO: Path to gcc cross compiler is not specified, will be searched in default paths in host"
    if [ $AARCH == "aarch64" ]; then
      target_abi="linux-gnu"
      c_compiler="aarch64-${target_abi}-gcc"
      cxx_compiler="aarch64-${target_abi}-g++"
    else
      target_abi="linux-gnueabihf"
      c_compiler="arm-${target_abi}-gcc"
      cxx_compiler="arm-${target_abi}-g++"
    fi
else
    c_compiler=$COMPILER
    dir=$(dirname "${COMPILER}")
    if [ $AARCH == "aarch64" ]; then
      cxx_compiler=${dir}/aarch64-linux-gnu-g++
    else
      cxx_compiler=${dir}/arm-linux-gnueabihf-g++
    fi
fi

if [ ! -f ${SYSROOT}/etc/os-release ]; then
    #Use case for Petalinux Sysroot
    echo "INFO: OS Flavor of SYSROOT cannot be determined, generating RPM packages of XRT"
    OS_TARGET="CentOS"
    echo "INFO: Cross Compiling XRT for $AARCH" 
else
    source ${SYSROOT}/etc/os-release
    OS_TARGET=$ID
    if [ $OS_TARGET == "ubuntu" ] || [ $OS_TARGET == "debian" ]; then
        OS_TARGET=${OS_TARGET^}
        VERSION=$VERSION_ID
    else
        OS_TARGET=`cat ${SYSROOT}/etc/redhat-release | awk '{print $1}' | tr -d '"'`
        VERSION=`cat ${SYSROOT}/etc/redhat-release | awk '{print $4}' | tr -d '"'`
    fi  
    echo "INFO: Cross Compiling XRT for $AARCH and Target System $OS_TARGET, Version:$VERSION"
fi

echo "INFO: $CMAKE -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_TOOLCHAIN_FILE=$BUILDDIR/$toolchain -Daarch=$AARCH -Dsysroot=$SYSROOT -Dflavor=$OS_TARGET -Dversion=$VERSION -DCROSS_COMPILE=yes -DCMAKE_C_COMPILER=$c_compiler -DCMAKE_CXX_COMPILER=$cxx_compiler ../../src"

time env XRT_NATIVE_BUILD=no $CMAKE -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_TOOLCHAIN_FILE=$BUILDDIR/$toolchain -Daarch=$AARCH -Dsysroot:PATH=$SYSROOT -Dflavor=$OS_TARGET -Dversion=$VERSION -DCROSS_COMPILE=yes -DCMAKE_C_COMPILER=$c_compiler -DCMAKE_CXX_COMPILER=$cxx_compiler ../../src

echo "make -j $jcore DESTDIR=$PWD install"
time make -j $jcore DESTDIR=$PWD install

time make package

eval "$SAVED_OPTIONS"; # Restore shell options
echo "** COMPLETE [${BASH_SOURCE[0]}] **"
echo ""
cd $here
