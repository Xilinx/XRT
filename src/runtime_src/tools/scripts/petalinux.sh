#!/bin/bash
#  (C) Copyright 2018-2019, Xilinx, Inc.

# This script is used to build the XRT Embedded Platform PetaLinux Image
#
# petalinux.sh <PATH_TO_VIVADO> <PATH_TO_XSCT> <PETALINUX_LOCATION> <PETALINUX_NAME> <XRT_REPO_DIR>
#
# PetaLinux output is put into directory $PWD/$PETALINUX_NAME
#
# *** The generated platform will be in $PWD/platform/
#

APPNAME="XRT EMBEDDED PETALINUX"
echo "** $APPNAME STARTING [${BASH_SOURCE[0]}] **"

SAVED_OPTIONS=$(set +o)
# Don't print all commands
set +x
# Error on non-zero exit code, by default:
set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"

PATH_TO_VIVADO=$1
if [ ! -f $PATH_TO_VIVADO ]; then
  echo "ERROR: Failed to find vivado (it is missing): ${PATH_TO_VIVADO}"
  exit 1
fi

PATH_TO_XSCT=$2
if [ ! -f $PATH_TO_XSCT ]; then
  echo "ERROR: Failed to find xsct (it is missing): ${PATH_TO_XSCT}"
  exit 1
fi

PETALINUX_LOCATION=$3

PETALINUX_NAME=$4
# Allow incremental builds
#if [ -d $PETALINUX_NAME ]; then
#  echo "ERROR: PetaLinux project already exists, please remove and rerun:"
#  echo "    $PETALINUX_NAME"
#  exit 1
#fi

XRT_REPO_DIR=$5

ORIGINAL_DIR=$PWD

BUILD_DSA=true
if [ "${BUILD_DSA}" = true ]; then
  # Generate DSA and HDF
  #  * ${XRT_REPO_DIR}/src/platform/zcu102ng/zcu102ng.dsa
  #  * ${XRT_REPO_DIR}/src/platform/zcu102ng/zcu102_vivado/zcu102ng.hdf
  cp -r ${XRT_REPO_DIR}/src/platform/zcu102ng ${ORIGINAL_DIR}/dsa_build
  cd ${ORIGINAL_DIR}/dsa_build
  echo " * Building Platform (DSA & HDF from: $PWD)"
  echo "   Starting: $PATH_TO_VIVADO"
  ${PATH_TO_VIVADO} -mode batch -notrace -source ./zcu102ng_dsa.tcl
  cd $ORIGINAL_DIR
fi

if [ ! -f ${ORIGINAL_DIR}/dsa_build/zcu102ng.dsa ]; then
  echo "ERROR: Failed to create DSA (it is missing): ${XRT_REPO_DIR}/dsa_build/zcu102ng.dsa"
  exit 1
fi

if [ ! -f ${ORIGINAL_DIR}/dsa_build/zcu102ng_vivado/zcu102ng.hdf ]; then
  echo "ERROR: Failed to create HDF (it is missing): ${XRT_REPO_DIR}/dsa_build/zcu102ng_vivado/zcu102ng.hdf"
  exit 1
fi

echo "PETALINUX: $PETALINUX_LOCATION"

# Setup per: 
#   https://xilinx.github.io/XRT/master/html/yocto.html#yocto-recipes-for-embedded-flow

echo " * Setup PetaLinux: $PETALINUX_LOCATION"
. $PETALINUX_LOCATION/settings.sh $PETALINUX_LOCATION

# We want the PetaLinux project to go here:
cd $ORIGINAL_DIR

if [ ! -d $PETALINUX_NAME ]; then
  echo " * Create PetaLinux Project: $PETALINUX_NAME"
  echo "petalinux-create -t project -n $PETALINUX_NAME --template zynqMP"
  petalinux-create -t project -n $PETALINUX_NAME --template zynqMP
fi

mkdir -p ${PETALINUX_NAME}/build/conf/

echo " * Configuring PetaLinux Project"
# Allow users to access shell without login
echo "CONFIG_YOCTO_ENABLE_DEBUG_TWEAKS=y" >> ${PETALINUX_NAME}/project-spec/configs/config
echo "petalinux-config -p $PETALINUX_NAME --get-hw-description=${ORIGINAL_DIR}/dsa_build/zcu102ng_vivado/zcu102ng_vivado/ --oldconfig"
petalinux-config -p $PETALINUX_NAME --get-hw-description=${ORIGINAL_DIR}/dsa_build/zcu102ng_vivado/ --oldconfig
 
echo " * Change to meta directory: ${PETALINUX_NAME}/project-spec/meta-user/"
cd ${PETALINUX_NAME}/project-spec/meta-user/

echo " * Copying ${XRT_REPO_DIR}/src/platform/recipes-xrt to `readlink -f .`"
cp -r ${XRT_REPO_DIR}/src/platform/recipes-xrt .

# If you are using PetaLinux 2018.3 or earlier version, do below step
if [[ $PETALINUX_VER == "2018"* ]]; then
  echo " * A 2018.X PetaLinux release (${PETALINUX_VER}) was detected, copying opencl-headers recipe:"
  mkdir -p recipes-xrt/opencl-headers
  wget -O recipes-xrt/opencl-headers/opencl-headers_git.bb http://cgit.openembedded.org/meta-openembedded/plain/meta-oe/recipes-core/opencl-headers/opencl-headers_git.bb
fi

# mnt-sd will run at the Linux boot time, it do below things
#  1. mount SD cart to /mnt
#  2. run init.sh in /mnt if it exist
cp -r ${XRT_REPO_DIR}/src/platform/mnt-sd recipes-apps/

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

echo 'IMAGE_INSTALL_append = " xrt-dev"'            >> $PETALINUX_IMAGE_BBAPPEND
echo 'IMAGE_INSTALL_append = " mnt-sd"'             >> $PETALINUX_IMAGE_BBAPPEND
echo 'IMAGE_INSTALL_append = " xrt"'                >> $PETALINUX_IMAGE_BBAPPEND
echo 'IMAGE_INSTALL_append = " zocl"'               >> $PETALINUX_IMAGE_BBAPPEND
echo 'IMAGE_INSTALL_append = " opencl-headers-dev"' >> $PETALINUX_IMAGE_BBAPPEND
echo 'IMAGE_INSTALL_append = " opencl-clhpp-dev"'   >> $PETALINUX_IMAGE_BBAPPEND

echo " * Adding XRT Kernel Node to Device Tree"
echo "cat ${XRT_REPO_DIR}/src/runtime_src/driver/zynq/fragments/xlnk_dts_fragment_mpsoc.dts >> recipes-bsp/device-tree/files/system-user.dtsi"
cat ${XRT_REPO_DIR}/src/runtime_src/driver/zynq/fragments/xlnk_dts_fragment_mpsoc.dts >> recipes-bsp/device-tree/files/system-user.dtsi
echo "cat ${ORIGINAL_DIR}/dsa_build/zcu102_fragment.dts >> recipes-bsp/device-tree/files/system-user.dtsi"
cat ${ORIGINAL_DIR}/dsa_build/zcu102_fragment.dts >> recipes-bsp/device-tree/files/system-user.dtsi

echo " * Configuring the kernel"
#Configure Linux kernel (default kernel config is good for zocl driver)
petalinux-config -c kernel --oldconfig

echo " * Configuring rootfs"
# Configure rootfs, enable below components:
#   menu -> "user packages" -> xrt
#   menu -> "user packages" -> mnt-sd
#   menu -> "user packages" -> xrt-dev
#   menu -> "user packages" -> zocl
#   menu -> "user packages" -> opencl-headers-dev
#   menu -> "user packages" -> opencl-clhpp-dev
# Saves to: ${PETALINUX_NAME}/project-spec/configs/rootfs_config
cp ../configs/rootfs_config{,.orig}
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

cd $ORIGINAL_DIR
echo " * Copying PetaLinux boot files (from: $PWD)"
cp ./${PETALINUX_NAME}/images/linux/image.ub ${ORIGINAL_DIR}/dsa_build/src/a53/xrt/image/image.ub
mkdir -p ${ORIGINAL_DIR}/dsa_build/src/boot
cp ./${PETALINUX_NAME}/images/linux/bl31.elf ${ORIGINAL_DIR}/dsa_build/src/boot/bl31.elf
cp ./${PETALINUX_NAME}/images/linux/pmufw.elf ${ORIGINAL_DIR}/dsa_build/src/boot/pmufw.elf
cp ./${PETALINUX_NAME}/images/linux/u-boot.elf ${ORIGINAL_DIR}/dsa_build/src/boot/u-boot.elf

# NOTE: Renames
cp ./${PETALINUX_NAME}/images/linux/zynqmp_fsbl.elf ${ORIGINAL_DIR}/dsa_build/src/boot/fsbl.elf

# Prepare Sysroot directory
echo " * Preparing Sysroot"
mkdir -p $ORIGINAL_DIR/dsa_build/src/aarch64-xilinx-linux
cd       $ORIGINAL_DIR/dsa_build/src/aarch64-xilinx-linux
tar zxf $ORIGINAL_DIR/${PETALINUX_NAME}/images/linux/rootfs.tar.gz 

cd ${ORIGINAL_DIR}/dsa_build
echo " * Building Platform (from: $PWD)"
echo "${PATH_TO_XSCT} -sdx ./zcu102ng_pfm.tcl"
${PATH_TO_XSCT} -sdx ./zcu102ng_pfm.tcl

# Copy platform directory to ORIGINAL_DIR/platform
echo " * Copy Platform to $ORIGINAL_DIR/platform"
mkdir -p $ORIGINAL_DIR/platform
cp -r ./output/zcu102ng/export/zcu102ng $ORIGINAL_DIR/platform

# Go back to original directory
cd $ORIGINAL_DIR

eval "$SAVED_OPTIONS"; # Restore shell options
echo "** $APPNAME COMPLETE [${BASH_SOURCE[0]}] **"

