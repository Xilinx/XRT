#!/bin/bash
#  (C) Copyright 2018-2019, Xilinx, Inc.

# This script is used to build the XRT Embedded Platform PetaLinux Image
#
# 
#
# the output is put into directory $PWD/$PLATFORM_NAME
#
# *** The generated platform will be in $PWD/platform/
#

APPNAME="EMBEDDED Runtime "
echo "** $APPNAME STARTING [${BASH_SOURCE[0]}] **"

SAVED_OPTIONS=$(set +o)
# Don't print all commands
set +x
# Error on non-zero exit code, by default:
set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"

usage() {
    echo "Build platfrom specific embedded runtime (ert)"
    echo
    echo "-platform <NAME>                 Embedded Platform name, e.g. zcu102ng / xcu104_revmin"
    echo "-vivado <PATH>                   Full path to vivado executable"
    echo "-xsct <PATH>                     Full path to xsct executable"
    echo "-petalinux <PATH>                Full path to petalinux folder"
    echo "-xrt <PATH>                      XRT github repo path"
    echo "[-full-peta-linux-build <Yes|No> Do a full peta-linux build or not. Full petalinux builds will take much longer time. Default=No" 
    echo "[-build-dsa <Yes/No>]            Build DSA or not, if not then copy a pre-build DSA from a relative path to vivado executable. Default=Yes"
    echo "[-build-sysroot <Yes/No>]        Build SYSROOT or not, default=No"
    echo "[-bsp <PATH>]                    Optional, full path to the platform bsp file, if not supplied, then the PetaLinux project is created using --template" 
    echo "[-help]                          List this help"
    exit $1
}

BSP_FILE="/null/null/fasan"
GEN_DSA="Yes"
BUILD_SYSROOT="No"
FULL_PETA_BULD="No"
while [ $# -gt 0 ]; do
    case "$1" in
        -help)
            usage 0
            ;;
        -build-dsa)
	    shift
	    GEN_DSA=$1
	    shift
	    ;;
        -build-sysroot)
	    shift
	    BUILD_SYSROOT=$1
	    shift
	    ;;
	-full-peta-linux-build)
	    shift
	    FULL_PETA_BULD=$1
	    shift
	 ;;
        -vivado)
            shift
            PATH_TO_VIVADO=$1
            shift
         ;;
        -xsct)
            shift
            PATH_TO_XSCT=$1
            shift
            ;;
        -petalinux)
            shift
            PETALINUX_LOCATION=$1
            shift
            ;;
        -platform)
            shift
            PLATFORM_NAME=$1
            shift
            ;;
        -bsp)
          shift
          BSP_FILE=$1
          shift
          ;;    
        -xrt)
            shift
            XRT_REPO_DIR=$1
            shift
            ;;
        *)
            echo "$1 invalid argument."
            usage 1
            ;;
    esac
done

if [ "foo${PATH_TO_VIVADO}" == "foo" ] ; then
  echo "full path to vivado is missing!"
  usage 1
fi

if [ "foo${PATH_TO_XSCT}" == "foo" ] ; then
  echo "full path to xsct is missing"
  usage 1
fi

if [ "foo${PETALINUX_LOCATION}" == "foo" ] ; then
  echo "full path to petalinux is missing"
  usage 1
fi

if [ "foo${PLATFORM_NAME}" == "foo" ] ; then
  echo "Embedded platform name is missing"
  usage 1
fi

if [ "foo${XRT_REPO_DIR}" == "foo" ] ; then
  echo "full path to xrt repo path is missing"
  usage 1
fi

if [ ! -f $PATH_TO_VIVADO ]; then
  echo "ERROR: Failed to find vivado executable (it is missing): ${PATH_TO_VIVADO}"
  exit 1
fi

if [ ! -f $PATH_TO_XSCT ]; then
  echo "ERROR: Failed to find xsct executale (it is missing): ${PATH_TO_XSCT}"
  exit 1
fi


# Allow incremental builds
#if [ -d $PLATFORM_NAME ]; then
#  echo "ERROR: PetaLinux project already exists, please remove and rerun:"
#  echo "    $PLATFORM_NAME"
#  exit 1
#fi

ORIGINAL_DIR=$PWD

#######################################################################
# check $1 (string) in $2 (File) and if $1 does not exists in the file #
# append to it                                                         #
######################################################################

addIfNoExists() {
  SAVED_OPTIONS=$(set +o)
  set +e
  str=$1
  file=$2
  grep "$str" $file 
  if [ $? != 0 ]; then
      echo "$str"  >> $file    
  fi
  eval "$SAVED_OPTIONS"
}

if [ "${GEN_DSA}" == "Yes" ]; then
  # Generate DSA and HDF
  #  * ${XRT_REPO_DIR}/src/platform/${PLATFORM_NAME}/${PLATFORM_NAME}.dsa
  #  * ${XRT_REPO_DIR}/src/platform/${PLATFORM_NAME}/${PLATFORM_NAME}_vivado/${PLATFORM_NAME}.hdf
  cp -r ${XRT_REPO_DIR}/src/platform/${PLATFORM_NAME} ${ORIGINAL_DIR}/dsa_build
  cd ${ORIGINAL_DIR}/dsa_build
  echo " * Building Platform (DSA & HDF from: $PWD)"
  echo "   Starting: $PATH_TO_VIVADO"
  ${PATH_TO_VIVADO} -mode batch -notrace -source ./${PLATFORM_NAME}_dsa.tcl
  cd $ORIGINAL_DIR
fi

if [ ! -f ${ORIGINAL_DIR}/dsa_build/${PLATFORM_NAME}.dsa ]; then
  echo "ERROR: Failed to create/locate DSA (it is missing): ${XRT_REPO_DIR}/dsa_build/${PLATFORM_NAME}.dsa"
  exit 1
fi
PLATFOMR_SDK=${ORIGINAL_DIR}/dsa_build/${PLATFORM_NAME}/${PLATFORM_NAME}.sdk
if [ ! -f ${PLATFOMR_SDK}/${PLATFORM_NAME}_wrapper.hdf ]; then
  echo "ERROR: Failed to create/locate HDF (it is missing): ${PLATFOMR_SDK}/${PLATFORM_NAME}_wrapper.hdf"
  exit 1
fi

echo "PETALINUX: $PETALINUX_LOCATION"

# Setup per: 
#   https://xilinx.github.io/XRT/master/html/yocto.html#yocto-recipes-for-embedded-flow

echo " * Setup PetaLinux: $PETALINUX_LOCATION"
. $PETALINUX_LOCATION/settings.sh $PETALINUX_LOCATION

# We want the PetaLinux project to go here:
cd $ORIGINAL_DIR


if [ ! -d $PLATFORM_NAME ]; then
  echo " * Create PetaLinux Project: $PLATFORM_NAME"
  # if .bsp is passed (e.g, /proj/petalinux/2019.1/petalinux-v2019.1_daily_latest/bsp/release/xilinx-zcu104-v2019.1-final.bsp) use that instead of the template
  if [ -f $BSP_FILE ]; then
    echo "petalinux-create -t project -n $PLATFORM_NAME -s $BSP_FILE" 
    petalinux-create -t project -n $PLATFORM_NAME -s $BSP_FILE
  else
    echo "petalinux-create -t project -n $PLATFORM_NAME --template zynqMP"
    petalinux-create -t project -n $PLATFORM_NAME --template zynqMP
  fi  
fi
mkdir -p ${PLATFORM_NAME}/build/conf/
echo " * Configuring PetaLinux Project"
# Allow users to access shell without login
echo "CONFIG_YOCTO_ENABLE_DEBUG_TWEAKS=y" >> ${PLATFORM_NAME}/project-spec/configs/config
echo "petalinux-config -p $PLATFORM_NAME --get-hw-description=${PLATFOMR_SDK} --oldconfig"
petalinux-config -p $PLATFORM_NAME --get-hw-description=${PLATFOMR_SDK} --oldconfig


echo "Replacing CONFIG_SUBSYSTEM_AUTOCONFIG_DEVICE__TREE"
perl -p -i -e  "s/CONFIG_SUBSYSTEM_AUTOCONFIG_DEVICE__TREE/# CONFIG_SUBSYSTEM_AUTOCONFIG_DEVICE__TREE\ is\ not\ set/g" ${PLATFORM_NAME}/project-spec/configs/config
cd ${PLATFORM_NAME}
petalinux-config --oldconfig
cd -

echo " * Change to meta directory: ${PLATFORM_NAME}/project-spec/meta-user/"
cd ${PLATFORM_NAME}/project-spec/meta-user/

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

addIfNoExists 'IMAGE_INSTALL_append = " xrt-dev"' $PETALINUX_IMAGE_BBAPPEND
addIfNoExists 'IMAGE_INSTALL_append = " mnt-sd"'  $PETALINUX_IMAGE_BBAPPEND
addIfNoExists 'IMAGE_INSTALL_append = " xrt"'     $PETALINUX_IMAGE_BBAPPEND
addIfNoExists 'IMAGE_INSTALL_append = " zocl"'    $PETALINUX_IMAGE_BBAPPEND
addIfNoExists 'IMAGE_INSTALL_append = " opencl-headers-dev"' $PETALINUX_IMAGE_BBAPPEND
addIfNoExists 'IMAGE_INSTALL_append = " opencl-clhpp-dev"'   $PETALINUX_IMAGE_BBAPPEND

echo " * Adding XRT Kernel Node to Device Tree"
echo "cat ${XRT_REPO_DIR}/src/runtime_src/driver/zynq/fragments/xlnk_dts_fragment_mpsoc.dts >> recipes-bsp/device-tree/files/system-user.dtsi"
cat ${XRT_REPO_DIR}/src/runtime_src/driver/zynq/fragments/xlnk_dts_fragment_mpsoc.dts >> recipes-bsp/device-tree/files/system-user.dtsi

if [ -f ${ORIGINAL_DIR}/dsa_build/${PLATFORM_NAME}_fragment.dts ]; then
  echo "cat ${ORIGINAL_DIR}/dsa_build/${PLATFORM_NAME}_fragment.dts >> recipes-bsp/device-tree/files/system-user.dtsi"
  cat ${ORIGINAL_DIR}/dsa_build/${PLATFORM_NAME}_fragment.dts >> recipes-bsp/device-tree/files/system-user.dtsi
fi

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
# Saves to: ${PLATFORM_NAME}/project-spec/configs/rootfs_config
cp ../configs/rootfs_config{,.orig}

addIfNoExists 'CONFIG_xrt=y' ../configs/rootfs_config
addIfNoExists  'CONFIG_mnt-sd=y'  ../configs/rootfs_config
addIfNoExists 'CONFIG_xrt-dev=y'  ../configs/rootfs_config
addIfNoExists 'CONFIG_zocl=y'  ../configs/rootfs_config
addIfNoExists 'CONFIG_opencl-headers-dev=y'  ../configs/rootfs_config
addIfNoExists 'CONFIG_opencl-clhpp-dev=y'  ../configs/rootfs_config


petalinux-config -c rootfs --oldconfig

# Build package
echo " * Performing PetaLinux Build (from: ${PWD})"

if [ $FULL_PETA_BULD == "Yes" ]; then
  echo "petalinux-build (FULL)"
  petalinux-build
else
  echo "petalinux-build (XRT ONLY)"
  petalinux-build -c xrt
  echo "petalinux-build (ZOCL Only)"
  petalinux-build -c zocl
fi

cd $ORIGINAL_DIR
echo " * Copying PetaLinux boot files (from: $PWD)"
mkdir -p ${ORIGINAL_DIR}/dsa_build/src/a53/xrt/image
cp ./${PLATFORM_NAME}/images/linux/image.ub ${ORIGINAL_DIR}/dsa_build/src/a53/xrt/image/image.ub
mkdir -p ${ORIGINAL_DIR}/dsa_build/src/boot
cp ./${PLATFORM_NAME}/images/linux/bl31.elf ${ORIGINAL_DIR}/dsa_build/src/boot/bl31.elf
cp ./${PLATFORM_NAME}/images/linux/pmufw.elf ${ORIGINAL_DIR}/dsa_build/src/boot/pmufw.elf
cp ./${PLATFORM_NAME}/images/linux/u-boot.elf ${ORIGINAL_DIR}/dsa_build/src/boot/u-boot.elf

# NOTE: Renames
cp ./${PLATFORM_NAME}/images/linux/zynqmp_fsbl.elf ${ORIGINAL_DIR}/dsa_build/src/boot/fsbl.elf


# Prepare Sysroot directory
echo " * Preparing Sysroot"
mkdir -p $ORIGINAL_DIR/dsa_build/src/aarch64-xilinx-linux
cd       $ORIGINAL_DIR/dsa_build/src/aarch64-xilinx-linux

# if we need to create HW for tests, then
# replace that tar with the 2 commands in step 4 from: http://confluence.xilinx.com/display/XIP/SDAccel+platform+porting+from+SDSoc+2019.1
# cd $ORIGINAL_DIR/${PLATFORM_NAME}
# petalinux-build --sdk
# second command
# to save 5GB remove x86* dir from generated sysroot

if [ $BUILD_SYSROOT == "Yes" ]; then
  echo " * Building SYSROOT"  
  cd ${ORIGINAL_DIR}/${PLATFORM_NAME}
  petalinux-build --sdk
  petalinux-package --sysroot -d .
  cd -
fi

echo " * Expanding $ORIGINAL_DIR/${PLATFORM_NAME}/images/linux/rootfs.tar.gz in $PWD"    
tar zxf $ORIGINAL_DIR/${PLATFORM_NAME}/images/linux/rootfs.tar.gz
  
cd ${ORIGINAL_DIR}/dsa_build
echo " * Building Platform (from: $PWD)"
echo "${PATH_TO_XSCT} -sdx ./${PLATFORM_NAME}_pfm.tcl"
# clear the display, otherwise xsct will fail
unset DISPLAY
${PATH_TO_XSCT} -sdx ./${PLATFORM_NAME}_pfm.tcl

# Copy platform directory to ORIGINAL_DIR/platform
echo " * Copy Platform to $ORIGINAL_DIR/platform"
mkdir -p $ORIGINAL_DIR/platform
cp -r ./output/${PLATFORM_NAME}/export/${PLATFORM_NAME} $ORIGINAL_DIR/platform

# Go back to original directory
cd $ORIGINAL_DIR

eval "$SAVED_OPTIONS"; # Restore shell options
echo "** $APPNAME COMPLETE [${BASH_SOURCE[0]}] **"
