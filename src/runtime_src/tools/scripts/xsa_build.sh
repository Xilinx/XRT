#!/bin/bash --
#  (C) Copyright 2019, Xilinx, Inc.

# This script is to build an embedded system XSA.
#
# This script need path to Vivado Tcl script. The script shoule be <xsa_name>_xsa.tcl
# This script will create a xsa_build/ directory in current path and build xsa in there.
# The generated XSA file is xsa_build/<xsa_name>/<xsa_name>.xsa

error()
{
	echo "ERROR: $1" 1>&2
	usage_and_exit 1
}

usage()
{
	echo "Usage: $PROGRAM [options] /path/to/xsa.tcl"
	echo "	options:"
	echo "		--help,-h           print this usage"
	echo ""
	echo "This script create xsa_build/ folder to build XSA"
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
PATH_TO_VIVADO=`which vivado`
TCLSCRIPT=""

while [ $# -gt 0 ]; do
	case $1 in
		--help | -h )
			usage_and_exit 0
			;;
		--* | -* )
			error "Unregognized option: $1"
			;;
		* )
			TCLSCRIPT=$1
			break
			;;
	esac
	shift
done

# Sanity Check
[ -z "$PATH_TO_VIVADO" ] && error "Vivado not found by 'which'. Please check Vivado installation."

[ ! -f "$TCLSCRIPT" ] && error "Vivado Tcl script no exist. $TCLSCRIPT"

XSA_TCL=`basename $TCLSCRIPT`
# Normalized Tcl script path
TCLSCRIPT=`readlink -f $TCLSCRIPT`
SRC_DIR=`dirname $TCLSCRIPT`
XSA_NAME=${XSA_TCL%_xsa.tcl}

[ -z "$SRC_DIR" ] && error "Script path is empty"

[ ! "${XSA_NAME}_xsa.tcl" == $XSA_TCL ] && error "Should be <xsa_name>_xsa.tcl"
# Sanity check done

ORIGINAL_DIR=`pwd`

echo "** Starting build XSA for $XSA_NAME **"
echo " Current DIR: $ORIGINAL_DIR"
echo " Vivado: $PATH_TO_VIVADO"
echo ""

# Generate XSA and HDF
[ -d "${ORIGINAL_DIR}/xsa_build/$XSA_NAME" ] && error "${ORIGINAL_DIR}/xsa_build/$XSA_NAME is existed. Please remove it."
mkdir -p ${ORIGINAL_DIR}/xsa_build/
cp -r $SRC_DIR ${ORIGINAL_DIR}/xsa_build/$XSA_NAME
cd ${ORIGINAL_DIR}/xsa_build/$XSA_NAME
echo " * Building Platform (XSA & HDF from: $PWD)"
${PATH_TO_VIVADO} -mode batch -notrace -source $XSA_TCL

eval "$SAVED_OPTIONS"; # Restore shell options
echo "** COMPLETE [${BASH_SOURCE[0]}] **"
echo ""

