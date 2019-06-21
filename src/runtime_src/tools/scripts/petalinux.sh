#!/bin/bash
#  (C) Copyright 2018-2019, Xilinx, Inc.

# This script is used to build the XRT Embedded Platform PetaLinux Image
#
# petalinux.sh <PATH_TO_VIVADO> <PATH_TO_XSCT> <PETALINUX_LOCATION> <PLATFORM_NAME> <XRT_REPO_DIR>
#
# PetaLinux output is put into directory $PWD/$PLATFORM_NAME
#
# *** The generated platform will be in $PWD/platform/
#

APPNAME="XRT EMBEDDED PETALINUX"
echo "** !!!! This script is obsoleted !!!! Please use ertbuild.sh **"

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
VIVADO_ROOT=`dirname ${PATH_TO_VIVADO}`/..
source $VIVADO_ROOT/settings64.sh

PATH_TO_XSCT=$2
if [ ! -f $PATH_TO_XSCT ]; then
  echo "ERROR: Failed to find xsct (it is missing): ${PATH_TO_XSCT}"
  exit 1
fi

PETALINUX_LOCATION=$3
PLATFORM_NAME=$4
XRT_REPO_DIR=$5
BSP=$6

ORIGINAL_DIR=$PWD

BUILD_DSA=true
if [ "${BUILD_DSA}" = true ]; then
  # Generate DSA and HDF, dsa_build/ directory will be generated
  ${XRT_REPO_DIR}/src/runtime_src/tools/scripts/dsa_build.sh ${XRT_REPO_DIR}/src/platform/${PLATFORM_NAME}/${PLATFORM_NAME}_dsa.tcl
fi

[ ! -f ${ORIGINAL_DIR}/dsa_build/${PLATFORM_NAME}.dsa ] && error "Failed to create DSA (it is missing): ./dsa_build/${PLATFORM_NAME}.dsa"

# Setup per:
#   https://xilinx.github.io/XRT/master/html/yocto.html#yocto-recipes-for-embedded-flow
echo " * Setup PetaLinux: $PETALINUX_LOCATION"
. $PETALINUX_LOCATION/settings.sh $PETALINUX_LOCATION

# dsa_build.sh will create ${PLATFORM_NAME}/ directory for petalinux project
if [ -z $BSP ]; then
	${XRT_REPO_DIR}/src/runtime_src/tools/scripts/peta_build.sh ${ORIGINAL_DIR}/dsa_build/${PLATFORM_NAME}.dsa
else
	${XRT_REPO_DIR}/src/runtime_src/tools/scripts/peta_build.sh --bsp $BSP ${ORIGINAL_DIR}/dsa_build/${PLATFORM_NAME}.dsa
fi

${XRT_REPO_DIR}/src/runtime_src/tools/scripts/pfm_build.sh ${ORIGINAL_DIR}/dsa_build/${PLATFORM_NAME}_pfm.tcl

