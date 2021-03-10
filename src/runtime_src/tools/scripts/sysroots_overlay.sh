#! /bin/bash --
# This script overlays the sysroot with given rpms. 
# Usage : ./sysroots_overlay.sh -s <sysroot path which need to be overlaid> -r <file which contains RPM Paths>
# Example: ./sysroots_overlay.sh -s sysroots/aarch64-xilinx-linux -r /scratch/xrtrpms

error()
{
	echo "ERROR: $1" 1>&2
	usage_and_exit 1
}

remove_and_error()
{
	rm -rf $ORIGINAL_DIR/$PROJECT
	error $1
}

usage()
{
	echo "Usage: $PROGRAM [options] -s <> -r <> "
	echo "	options:"
	echo "		--help / -h              		Print this usage"
	echo "		--sysroot / -s  			sysroot path which need to be overlaid"
	echo "		--rpms-file / -r 			File which contain Path of RPMS to be overlaid"
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

echo "** STARTED [${BASH_SOURCE[0]}] **"
# Get real script by read symbol link
THIS_SCRIPT=`readlink -f ${BASH_SOURCE[0]}`

THIS_SCRIPT_DIR="$( cd "$( dirname "${THIS_SCRIPT}" )" >/dev/null 2>&1 && pwd )"

PROGRAM=`basename $0`
RPMSPATH=""
PROJECT=".systmp"
SYSROOT=""

while [ $# -gt 0 ]; do
	case $1 in
		--help | -h )
			usage_and_exit 0
			;;
		--rpms-file | -r )
			shift
			RPMSPATH=$1
			;;
		--sysroot | -s )
			shift
			SYSROOT=$1
			;;
		--* | -* )
			error "Unregognized option: $1"
			;;
		* )
	esac
	shift
done

if [ ! -f $RPMSPATH ]; then
  error "$RPMSPATH does not exist"
fi

if [ ! -d $SYSROOT ]; then
  error "$SYSROOT does not exist"
fi

# Sanity check done


ORIGINAL_DIR=`pwd`

echo "[CMD] Creating Project"
mkdir -p $ORIGINAL_DIR/$PROJECT
cd $ORIGINAL_DIR/$PROJECT

echo "[CMD] Extracting RPM"
while IFS= read -r line
do
  echo "$line"
  if [ ! -f $line ]; then
    remove_and_error "$line not exist"
  fi
  rpm2cpio $line | cpio -ivdmu
done < "$RPMSPATH"

echo "[CMD] Creating Tar"
tar -zcf sysroot_overlay.tar.gz *
echo "[CMD] Creating Tar Completed"
cd $ORIGINAL_DIR

echo "[CMD] Extract Tar $ORIGINAL_DIR/sysroot_overlay.tar.gz"
cd $SYSROOT
tar -zxvf $ORIGINAL_DIR/$PROJECT/sysroot_overlay.tar.gz
cd $ORIGINAL_DIR

echo "[CMD] Removing tmp"
rm -rf $ORIGINAL_DIR/$PROJECT

eval "$SAVED_OPTIONS"; # Restore shell options
echo "** COMPLETE [${BASH_SOURCE[0]}] **"
echo ""
