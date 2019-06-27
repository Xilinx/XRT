#!/bin/bash --
#  (C) Copyright 2019, Xilinx, Inc.

# This script is to build an embedded system platform.
#
# This script need path to XSCT PFM Tcl script. The script shoule be <platform_name>_pfm.tcl

error()
{
	echo "ERROR: $1" 1>&2
	usage_and_exit 1
}

usage()
{
	echo "Usage: $PROGRAM [options] /path/to/pfm.tcl"
	echo "	options:"
	echo "		--help,-h           print this usage"
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
# Error on non-zero exit code, by default:
set -e

PROGRAM=`basename $0`
PATH_TO_XSCT=`which xsct`
PFMSCRIPT=""

while [ $# -gt 0 ]; do
	case $1 in
		--help | -h )
			usage_and_exit 0
			;;
		--* | -* )
			error "Unregognized option: $1"
			;;
		* )
			PFMSCRIPT=$1
			break
			;;
	esac
	shift
done

# Sanity Check
[ -z "$PATH_TO_XSCT" ] && error "xsct not found by 'which'. Please check Vivado installation."

[ ! -e "$PFMSCRIPT" ] && error "XSCT Tcl script no exist. Is path correct? $PFMSCRIPT"

PFM_TCL=`basename $PFMSCRIPT`
# Normalized Tcl script path
PFMSCRIPT=`readlink -f $PFMSCRIPT`
SRC_DIR=`dirname $PFMSCRIPT`
PLATFORM_NAME=${PFM_TCL%_pfm.tcl}

[ -z "$SRC_DIR" ] && error "Source code path is empty"

[ ! "${PLATFORM_NAME}_pfm.tcl" == $PFM_TCL ] && error "Should be <platform_name>_pfm.tcl"

# Sanity check done

ORIGINAL_DIR=`pwd`

echo "** Starting build platform for $PLATFORM_NAME **"
echo "Current DIR: $ORIGINAL_DIR"
echo "xsct: $PATH_TO_XSCT"
echo ""

[ -d "$ORIGINAL_DIR/platform" ] && error "$ORIGINAL_DIR/platform is existed. Please remove it."

cd $SRC_DIR
echo " * Building Platform (from: $PWD)"
echo "${PATH_TO_XSCT} -sdx $PFMSCRIPT"
# clear the display, otherwise xsct will fail
unset DISPLAY
${PATH_TO_XSCT} -sdx $PFMSCRIPT

# Copy platform directory to ORIGINAL_DIR/platform
echo " * Copy Platform to $ORIGINAL_DIR/platform"
mkdir -p $ORIGINAL_DIR/platform
cp -r ./output/${PLATFORM_NAME}/export/${PLATFORM_NAME} $ORIGINAL_DIR/platform

# Go back to original directory
cd $ORIGINAL_DIR

eval "$SAVED_OPTIONS"; # Restore shell options
echo "** COMPLETE [${BASH_SOURCE[0]}] **"
echo ""
