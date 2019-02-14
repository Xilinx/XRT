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
  cd ${XRT_REPO_DIR}/src/platform/zcu102ng/
  echo " * Building Platform (DSA & HDF from: $PWD)"
  echo "   Starting: $PATH_TO_VIVADO"
  ${PATH_TO_VIVADO} -mode batch -notrace -source ./zcu102ng_dsa.tcl
  cd $ORIGINAL_DIR
fi
if [ ! -f ${XRT_REPO_DIR}/src/platform/zcu102ng/zcu102ng.dsa ]; then
  echo "ERROR: Failed to create DSA (it is missing): ${XRT_REPO_DIR}/src/platform/zcu102ng/zcu102ng.dsa"
  exit 1
fi
if [ ! -f ${XRT_REPO_DIR}/src/platform/zcu102ng/zcu102ng_vivado/zcu102ng.hdf ]; then
  echo "ERROR: Failed to create HDF (it is missing): ${XRT_REPO_DIR}/src/platform/zcu102ng/zcu102ng_vivado/zcu102ng.hdf"
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
echo "petalinux-config -p $PETALINUX_NAME --get-hw-description=${XRT_REPO_DIR}/src/platform/zcu102ng/zcu102ng_vivado/ --oldconfig"
petalinux-config -p $PETALINUX_NAME --get-hw-description=${XRT_REPO_DIR}/src/platform/zcu102ng/zcu102ng_vivado/ --oldconfig
 
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
cp recipes-core/images/petalinux-image.bbappend{,.orig}
echo 'IMAGE_INSTALL_append = " xrt-dev"' >> recipes-core/images/petalinux-image.bbappend
echo 'IMAGE_INSTALL_append = " mnt-sd"' >> recipes-core/images/petalinux-image.bbappend
echo 'IMAGE_INSTALL_append = " xrt"' >> recipes-core/images/petalinux-image.bbappend
echo 'IMAGE_INSTALL_append = " zocl"' >> recipes-core/images/petalinux-image.bbappend 
echo 'IMAGE_INSTALL_append = " opencl-headers-dev"' >> recipes-core/images/petalinux-image.bbappend 

echo " * Adding XRT Kernel Node to Device Tree"
echo "cat ${XRT_REPO_DIR}/src/runtime_src/driver/zynq/fragments/xlnk_dts_fragment_mpsoc.dts >> recipes-bsp/device-tree/files/system-user.dtsi"
cat ${XRT_REPO_DIR}/src/runtime_src/driver/zynq/fragments/xlnk_dts_fragment_mpsoc.dts >> recipes-bsp/device-tree/files/system-user.dtsi
echo "cat ${XRT_REPO_DIR}/src/platform/zcu102ng/zcu102_fragment.dts >> recipes-bsp/device-tree/files/system-user.dtsi"
cat ${XRT_REPO_DIR}/src/platform/zcu102ng/zcu102_fragment.dts >> recipes-bsp/device-tree/files/system-user.dtsi

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
# Saves to: ${PETALINUX_NAME}/project-spec/configs/rootfs_config
cp ../configs/rootfs_config{,.orig}
echo 'CONFIG_xrt=y' >> ../configs/rootfs_config
echo 'CONFIG_mnt-sd=y' >> ../configs/rootfs_config
echo 'CONFIG_xrt-dev=y' >> ../configs/rootfs_config
echo 'CONFIG_zocl=y' >> ../configs/rootfs_config
echo 'CONFIG_opencl-headers-dev=y' >> ../configs/rootfs_config
petalinux-config -c rootfs --oldconfig

# Build package
echo " * Performing PetaLinux Build (from: ${PWD})"
echo "petalinux-build"
petalinux-build

cd $ORIGINAL_DIR
echo " * Copying PetaLinux boot files (from: $PWD)"
cp ./${PETALINUX_NAME}/images/linux/image.ub ${XRT_REPO_DIR}/src/platform/zcu102ng/src/a53/xrt/image/image.ub
mkdir -p ${XRT_REPO_DIR}/src/platform/zcu102ng/src/boot
cp ./${PETALINUX_NAME}/images/linux/bl31.elf ${XRT_REPO_DIR}/src/platform/zcu102ng/src/boot/bl31.elf
cp ./${PETALINUX_NAME}/images/linux/pmufw.elf ${XRT_REPO_DIR}/src/platform/zcu102ng/src/boot/pmufw.elf
cp ./${PETALINUX_NAME}/images/linux/u-boot.elf ${XRT_REPO_DIR}/src/platform/zcu102ng/src/boot/u-boot.elf

# NOTE: Renames
cp ./${PETALINUX_NAME}/images/linux/zynqmp_fsbl.elf ${XRT_REPO_DIR}/src/platform/zcu102ng/src/boot/fsbl.elf

cd ${XRT_REPO_DIR}/src/platform/zcu102ng/ 
echo " * Building Platform (from: $PWD)"
echo "${PATH_TO_XSCT} -sdx ./zcu102ng_pfm.tcl"
${PATH_TO_XSCT} -sdx ./zcu102ng_pfm.tcl

# Copy platform directory to ORIGINAL_DIR/platform
echo " * Copy Platform to $ORIGINAL_DIR/platform"
mkdir -p $ORIGINAL_DIR/platform
cp -r ./output/zcu102ng/export/zcu102ng $ORIGINAL_DIR/platform

# Prepare Sysroot directory
echo " * Prepare Sysroot $ORIGINAL_DIR/platform/zcu102ng/sw/xrt/sysroot"
mkdir -p $ORIGINAL_DIR/platform/zcu102ng/sw/xrt/sysroot
cd $ORIGINAL_DIR/platform/zcu102ng/sw/xrt/sysroot
tar zxf $ORIGINAL_DIR/${PETALINUX_NAME}/images/linux/rootfs.tar.gz 

# Go back to original directory
cd $ORIGINAL_DIR

eval "$SAVED_OPTIONS"; # Restore shell options
echo "** $APPNAME COMPLETE [${BASH_SOURCE[0]}] **"

