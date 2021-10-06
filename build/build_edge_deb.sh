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
    echo "          -sysroot_name                   Name of sysroot folder created for compilation"
    echo "          -clean, clean                   Remove build directories, pass 'aarch' option to it"
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

# XRT Version variables
XRT_MAJOR_VERSION=2
XRT_MINOR_VERSION=13
RELEASE_VERSION=202210

# Pick XRT_VERSION_PATCH from Env variable
if [ -z $XRT_VERSION_PATCH ]; then
    XRT_VERSION_PATCH=0
fi

source /etc/os-release
OS_HOST=$ID

if [[ $OS_HOST != "ubuntu" ]] && [[ $OS_HOST != "debian" ]]; then
	error "Please use ubuntu/debian machine for building, $OS_HOST not supported"
fi

clean=0

while [ $# -gt 0 ]; do
        case $1 in
                -help | --help )
                        usage_and_exit 0
                        ;;
                -aarch | --aarch )
                        shift
                        AARCH=$1
                        ;;
                -sysroot_name | --sysroot_name )
                        shift
                        SYSROOT=$1
                        ;;
                -clean | clean | --clean )
                        clean=1
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

THIS_SCRIPT_DIR=$(dirname "$THIS_SCRIPT")
XRT_DIR=`readlink -f $THIS_SCRIPT_DIR/../`
DEBIAN=`readlink -f $THIS_SCRIPT_DIR/debian`

if [ -z $AARCH ]; then
    error "-aarch is required option"
fi

if [[ $AARCH == "aarch64" ]]; then
    BUILD_FOLDER=deb_aarch64
elif [[ $AARCH == "aarch32" ]]; then
    BUILD_FOLDER=deb_aarch32
else
    error "$AARCH not valid aarch option"
fi

if [[ $clean == 1 ]]; then
    echo $PWD
    echo "/bin/rm -rf $THIS_SCRIPT_DIR/$BUILD_FOLDER"
    /bin/rm -rf $THIS_SCRIPT_DIR/$BUILD_FOLDER
    exit 0
fi

# Sanity Check
if [ -z $SYSROOT ] ; then
    error "Please provide the required option 'sysroot_name'"
fi

if [ ! -d $DEBIAN ]; then
    error "$DEBIAN is not accessible"
fi

DEBIAN_ARTIFACTS=$THIS_SCRIPT_DIR/$BUILD_FOLDER/debian_artifacts
mkdir -p $DEBIAN_ARTIFACTS
cd $DEBIAN_ARTIFACTS
# TODO: 
# Debian expects XRT source tarball to be in parent directory of folder having debian folder, that way we should place
# tar ball outside XRT directory which makes clean process difficult, find a flag to point to XRT source tar ball.
# Current implementation copies source code to build folder
rsync -r $XRT_DIR/* $DEBIAN_ARTIFACTS --exclude=build

#creating source code tarball for sbuild
tar -cvf $THIS_SCRIPT_DIR/$BUILD_FOLDER/xrt_${XRT_MAJOR_VERSION}.${XRT_MINOR_VERSION}.${XRT_VERSION_PATCH}.orig.tar .
xz -z $THIS_SCRIPT_DIR/$BUILD_FOLDER/xrt_${XRT_MAJOR_VERSION}.${XRT_MINOR_VERSION}.${XRT_VERSION_PATCH}.orig.tar
cp -rf $DEBIAN $DEBIAN_ARTIFACTS

# changing XRT package version number
sed -i "1d" $DEBIAN_ARTIFACTS/debian/changelog
sed -i "1s/^/xrt (${XRT_MAJOR_VERSION}.${XRT_MINOR_VERSION}.${XRT_VERSION_PATCH}) experimental;urgency=medium\n/" $DEBIAN_ARTIFACTS/debian/changelog

time sbuild --no-run-lintian -d focal --arch=arm64 -c $SYSROOT -s

cd $THIS_SCRIPT_DIR/$BUILD_FOLDER
# rename the packages created for consistency
mv xrt-embedded_${XRT_MAJOR_VERSION}.${XRT_MINOR_VERSION}.${XRT_VERSION_PATCH}_arm64.deb xrt_embedded_${RELEASE_VERSION}.${XRT_MAJOR_VERSION}.${XRT_MINOR_VERSION}.${XRT_VERSION_PATCH}_20.04-arm64.deb
mv xrt-zocl-dkms_${XRT_MAJOR_VERSION}.${XRT_MINOR_VERSION}.${XRT_VERSION_PATCH}_arm64.deb xrt_zocl_dkms_${RELEASE_VERSION}.${XRT_MAJOR_VERSION}.${XRT_MINOR_VERSION}.${XRT_VERSION_PATCH}_20.04-arm64.deb
mv xrt-embedded-dbgsym_${XRT_MAJOR_VERSION}.${XRT_MINOR_VERSION}.${XRT_VERSION_PATCH}_arm64.ddeb xrt_embedded_dbgsym_${RELEASE_VERSION}.${XRT_MAJOR_VERSION}.${XRT_MINOR_VERSION}.${XRT_VERSION_PATCH}_20.04-arm64.ddeb

eval "$SAVED_OPTIONS"; # Restore shell options
echo "** COMPLETE [${BASH_SOURCE[0]}] **"
echo ""
cd $THIS_SCRIPT_DIR
