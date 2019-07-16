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
    echo "-platform <NAME>                 Embedded Platform name, e.g. zcu102ng / zcu104_revmin"
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
VIVADO_ROOT=`dirname ${PATH_TO_VIVADO}`/..
source $VIVADO_ROOT/settings64.sh

if [ ! -f $PATH_TO_XSCT ]; then
  echo "ERROR: Failed to find xsct executale (it is missing): ${PATH_TO_XSCT}"
  exit 1
fi
XSCT_ROOT=`dirname ${PATH_TO_XSCT}`/..
source $XSCT_ROOT/settings64.sh

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
  if [ -d dsa_build/${PLATFORM_NAME} ]; then
	  rm -rf ./dsa_build/${PLATFORM_NAME}
  fi
  ${XRT_REPO_DIR}/src/runtime_src/tools/scripts/dsa_build.sh ${XRT_REPO_DIR}/src/platform/${PLATFORM_NAME}/${PLATFORM_NAME}_dsa.tcl
fi

if [ ! -f ${ORIGINAL_DIR}/dsa_build/${PLATFORM_NAME}/${PLATFORM_NAME}.dsa ]; then
  echo "ERROR: Failed to create/locate DSA (it is missing): ${XRT_REPO_DIR}/dsa_build/${PLATFORM_NAME}/${PLATFORM_NAME}.dsa"
  exit 1
fi

# Setup per: 
#   https://xilinx.github.io/XRT/master/html/yocto.html#yocto-recipes-for-embedded-flow
. $PETALINUX_LOCATION/settings.sh $PETALINUX_LOCATION

# We want the PetaLinux project to go here:
cd $ORIGINAL_DIR

if [ $FULL_PETA_BULD == "Yes" ]; then
	echo ""
	echo "* petalinux build (FULL)"
	# peta_build.sh will create ${PLATFORM_NAME}/ directory for petalinux project
	if [ -d $PLATFORM_NAME ]; then
		echo " WARNING: $PLATFORM_NAME/ folder exist. This is not allowed in full petalinux build mode. Remove it then create petalinux project."
		rm -rf $PLATFORM_NAME
	fi

	if [ -f $BSP_FILE ]; then
		${XRT_REPO_DIR}/src/runtime_src/tools/scripts/peta_build.sh --bsp $BSP_FILE ${ORIGINAL_DIR}/dsa_build/${PLATFORM_NAME}/${PLATFORM_NAME}.dsa
	else
		${XRT_REPO_DIR}/src/runtime_src/tools/scripts/peta_build.sh ${ORIGINAL_DIR}/dsa_build/${PLATFORM_NAME}/${PLATFORM_NAME}.dsa
	fi
else
	if [ ! -d $PLATFORM_NAME ]; then
		echo "ERROR: No petalinux direcotry $PLATFORM_NAME/. Please run \"-full-peta-linux-build Yes\""
		exit 1
	fi

	echo ""
	echo "* petalinux build (INCREMENTAL)"
	cd $PLATFORM_NAME
	echo "* petalinux-build (XRT ONLY)"
	petalinux-build -c xrt
	echo "* petalinux-build (ZOCL Only)"
	petalinux-build -c zocl
	cd $ORIGINAL_DIR
fi

if [ ! -f ${ORIGINAL_DIR}/${PLATFORM_NAME}/images/linux/image.ub ]; then
	echo "ERROR: Failed to create/locate image.ub (it is missing): ${ORIGINAL_DIR}/${PLATFORM_NAME}/images/linux/image.ub"
	exit 1
fi

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

# We want the platform folder to go here:
cd $ORIGINAL_DIR

# Create platform
if [ -d platform/${PLATFORM_NAME} ]; then
	rm -rf ./platform/${PLATFORM_NAME}
fi
${XRT_REPO_DIR}/src/runtime_src/tools/scripts/pfm_build.sh ${ORIGINAL_DIR}/dsa_build/${PLATFORM_NAME}/${PLATFORM_NAME}_pfm.tcl

# Go back to original directory
cd $ORIGINAL_DIR

eval "$SAVED_OPTIONS"; # Restore shell options
echo "** $APPNAME COMPLETE [${BASH_SOURCE[0]}] **"
