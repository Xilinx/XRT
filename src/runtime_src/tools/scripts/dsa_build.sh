#!/bin/bash --
#  (C) Copyright 2019, Xilinx, Inc.

# This script is to build an embedded system DSA.
#
# This script need path to Vivado Tcl script. The script shoule be <dsa_name>_dsa.tcl
# This script will create a dsa_build/ directory in current path and build dsa in there.
# The generated DSA file is dsa_build/<dsa_name>.dsa

error()
{
	echo "ERROR: $1" 1>&2
	usage_and_exit 1
}

usage()
{
	echo "Usage: $PROGRAM [options] /path/to/dsa.tcl"
	echo "	options:"
	echo "		--help,-h           print this usage"
	echo ""
	echo "This script create dsa_build/ folder to build DSA"
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

DSA_TCL=`basename $TCLSCRIPT`
# Normalized Tcl script path
TCLSCRIPT=`readlink -f $TCLSCRIPT`
SRC_DIR=`dirname $TCLSCRIPT`
DSA_NAME=${DSA_TCL%_dsa.tcl}

[ -z "$SRC_DIR" ] && error "Script path is empty"

[ ! "${DSA_NAME}_dsa.tcl" == $DSA_TCL ] && error "Should be <dsa_name>_dsa.tcl"
# Sanity check done

ORIGINAL_DIR=`pwd`

echo "** Starting build DSA for $DSA_NAME **"
echo " Current DIR: $ORIGINAL_DIR"
echo " Vivado: $PATH_TO_VIVADO"
echo ""

# Generate DSA and HDF
[ -d "${ORIGINAL_DIR}/dsa_build" ] && error "${ORIGINAL_DIR}/dsa_build is existed. Please remove it."
cp -r $SRC_DIR ${ORIGINAL_DIR}/dsa_build
cd ${ORIGINAL_DIR}/dsa_build
echo " * Building Platform (DSA & HDF from: $PWD)"
${PATH_TO_VIVADO} -mode batch -notrace -source $DSA_TCL

eval "$SAVED_OPTIONS"; # Restore shell options
echo "** COMPLETE [${BASH_SOURCE[0]}] **"
echo ""

