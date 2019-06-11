#! /bin/bash --
#  (C) Copyright 2018-2019, Xilinx, Inc.

# This script is to build an embedded system PetaLinux image
# which contain XRT library and driver.
#

error()
{
	echo "ERROR: $1" 1>&2
	usage_and_exit 1
}

usage()
{
	echo "Usage: $PROGRAM [options] /path/to/<dsa_name>.dsa"
	echo "	options:"
	echo "		--help,-v              print this usage"
	echo ""
	echo "This script will create <dsa_name>/ directory for petalinux project"
	echo "Note: Do NOT move this script to another place. It depends on XRT repo"
}

usage_and_exit()
{
	usage
	exit $1
}

SAVED_OPTIONS=$(set +o)

# Don't print all commands
set +x
# Error on non-zero exit code, by default:
set -e

THIS_SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

PATH_TO_DSA=""
DSA_NAME=""
RECIPE_DIR=`readlink -f ${THIS_SCRIPT_DIR}/../../../platform/recipes-xrt/`

while [ $# -gt 0 ]; do
	case $1 in
		--help | -h )
			usage_and_exit 0
			;;
		--* | -* )
			error "Unregognized option: $1"
			;;
		* )
			PATH_TO_DSA=$1
			break
			;;
	esac
	shift
done

# Sanity Check
[ ! -e "$PATH_TO_DSA" ] && error "DSA is not exist"

[ -z "$PETALINUX" ] && error "PetaLinux is not installed"

[ ! -d "$RECIPE_DIR" ] && error "Could not found recipe-xrt/ directory"
# Sanity check done

ORIGINAL_DIR=`pwd`

DSA_FILE=`basename $PATH_TO_DSA`
DSA_DIR=`dirname $PATH_TO_DSA`
DSA_NAME=${DSA_FILE%.dsa}

PETALINUX_NAME=$DSA_NAME

if [ ! -d $PETALINUX_NAME ]; then
  echo " * Create PetaLinux Project: $PETALINUX_NAME"
  echo "petalinux-create -t project -n $PETALINUX_NAME --template zynqMP"
  petalinux-create -t project -n $PETALINUX_NAME --template zynqMP
else
  echo " * PetaLinux Project existed: $PETALINUX_NAME"
fi

mkdir -p ${PETALINUX_NAME}/build/conf/

echo " * Configuring PetaLinux Project"
# Allow users to access shell without login
#echo "******************* configure petalinux project hook ********************"
echo "CONFIG_YOCTO_ENABLE_DEBUG_TWEAKS=y" >> ${PETALINUX_NAME}/project-spec/configs/config
echo "petalinux-config -p $PETALINUX_NAME --get-hw-description=${DSA_DIR} --oldconfig"
petalinux-config -p $PETALINUX_NAME --get-hw-description=${DSA_DIR} --oldconfig

echo " * Change to meta directory: ${PETALINUX_NAME}/project-spec/meta-user/"
cd ${PETALINUX_NAME}/project-spec/meta-user/

echo " * Copying ${RECIPE_DIR} to `readlink -f .`"
cp -r ${RECIPE_DIR} .

# If you are using PetaLinux 2018.3 or earlier version, do below step
if [[ $PETALINUX_VER == "2018"* ]]; then
  echo " * A 2018.X PetaLinux release (${PETALINUX_VER}) was detected, copying opencl-headers recipe:"
  mkdir -p recipes-xrt/opencl-headers
  wget -O recipes-xrt/opencl-headers/opencl-headers_git.bb http://cgit.openembedded.org/meta-openembedded/plain/meta-oe/recipes-core/opencl-headers/opencl-headers_git.bb
fi

#echo "******************* application hook ********************"
# mnt-sd will run at the Linux boot time, it do below things
#  1. mount SD cart to /mnt
#  2. run init.sh in /mnt if it exist
cp -r ${RECIPE_DIR}/../mnt-sd recipes-apps/

echo " * Adding XRT, HAL, Driver recipes"

# In 2018.3 Petalinux the name of this file changed..
if [ -f recipes-core/images/petalinux-image.bbappend ]; then
	PETALINUX_IMAGE_BBAPPEND=recipes-core/images/petalinux-image.bbappend
elif [ -f recipes-core/images/petalinux-image-full.bbappend ]; then
	PETALINUX_IMAGE_BBAPPEND=recipes-core/images/petalinux-image-full.bbappend
else
	echo "Not petalinux image .bbappend file in project-spec/meta-user/recipes-core/images/"
	exit 1;
fi

#echo "******************* petalinux-image hook ********************"
echo 'IMAGE_INSTALL_append = " xrt-dev"'            >> $PETALINUX_IMAGE_BBAPPEND
echo 'IMAGE_INSTALL_append = " mnt-sd"'             >> $PETALINUX_IMAGE_BBAPPEND
echo 'IMAGE_INSTALL_append = " xrt"'                >> $PETALINUX_IMAGE_BBAPPEND
echo 'IMAGE_INSTALL_append = " zocl"'               >> $PETALINUX_IMAGE_BBAPPEND
echo 'IMAGE_INSTALL_append = " opencl-headers-dev"' >> $PETALINUX_IMAGE_BBAPPEND
echo 'IMAGE_INSTALL_append = " opencl-clhpp-dev"'   >> $PETALINUX_IMAGE_BBAPPEND

echo " * Adding XRT Kernel Node to Device Tree"
#echo "******************* Device Tree hook ********************"
echo "cat ${RECIPE_DIR}/../../runtime_src/core/edge/fragments/xlnk_dts_fragment_mpsoc.dts >> recipes-bsp/device-tree/files/system-user.dtsi"
cat ${RECIPE_DIR}/../../runtime_src/core/edge/fragments/xlnk_dts_fragment_mpsoc.dts >> recipes-bsp/device-tree/files/system-user.dtsi
echo "cat ${ORIGINAL_DIR}/dsa_build/zcu102_fragment.dts >> recipes-bsp/device-tree/files/system-user.dtsi"
cat ${ORIGINAL_DIR}/dsa_build/zcu102_fragment.dts >> recipes-bsp/device-tree/files/system-user.dtsi

echo " * Configuring the kernel"
#Configure Linux kernel (default kernel config is good for zocl driver)
#echo "******************* configure kernel hook ********************"
petalinux-config -c kernel --oldconfig

echo " * Configuring rootfs"
# Saves to: ${PETALINUX_NAME}/project-spec/configs/rootfs_config
cp ../configs/rootfs_config{,.orig}

#echo "******************* Configure Rootfs hook ********************"
echo 'CONFIG_xrt=y' >> ../configs/rootfs_config
echo 'CONFIG_mnt-sd=y' >> ../configs/rootfs_config
echo 'CONFIG_xrt-dev=y' >> ../configs/rootfs_config
echo 'CONFIG_zocl=y' >> ../configs/rootfs_config
echo 'CONFIG_opencl-headers-dev=y' >> ../configs/rootfs_config
echo 'CONFIG_opencl-clhpp-dev=y' >> ../configs/rootfs_config
petalinux-config -c rootfs --oldconfig

# Build package
echo " * Performing PetaLinux Build (from: ${PWD})"
echo "petalinux-build"
petalinux-build

eval "$SAVED_OPTIONS"; # Restore shell options
echo "** COMPLETE [${BASH_SOURCE[0]}] **"
echo ""

