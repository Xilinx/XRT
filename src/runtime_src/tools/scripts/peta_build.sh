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
	echo "		--help, -v              		Print this usage"
	echo "		--config, -c config.sh			Specific PetaLinux config script"
	echo "		--bsp, -b PetaLinuxBSP			Specific PetaLinux BSP"
	echo ""
	echo "This script will create <dsa_name>/ directory for petalinux project"
	echo "Note: Do NOT move this script to another place. It depends on XRT repo"
}

usage_and_exit()
{
	usage
	exit $1
}

version_lt()
{
	test "$(printf '%s\n' "$@" | sort -Vr | head -n 1)" != "$1"
}

# --- Internal funtions ---
# Could be overrided by
# 	1. user specified config.sh
#	2. if 1 not exist, platform's config.sh
config_peta()
{
	PETA_CONFIG_FILE=$1
	echo "CONFIG_YOCTO_ENABLE_DEBUG_TWEAKS=y" >> $PETA_CONFIG_FILE
}

config_kernel()
{
	KERN_CONFIG_FILE=$1
	# *** Enable or disable Linux kernel features as you need ***
	# AR# 69143 -- To avoid PetaLinux hang when JTAG connected.
	echo '# CONFIG_CPU_IDLE is not set' >> $KERN_CONFIG_FILE
}

config_rootfs()
{
	ROOTFS_CONFIG_FILE=$1
	echo 'CONFIG_xrt=y' 				>> $ROOTFS_CONFIG_FILE
	echo 'CONFIG_mnt-sd=y' 				>> $ROOTFS_CONFIG_FILE
	echo 'CONFIG_xrt-dev=y' 			>> $ROOTFS_CONFIG_FILE
	echo 'CONFIG_zocl=y' 				>> $ROOTFS_CONFIG_FILE
	echo 'CONFIG_opencl-headers-dev=y' 	>> $ROOTFS_CONFIG_FILE
	echo 'CONFIG_opencl-clhpp-dev=y' 	>> $ROOTFS_CONFIG_FILE
}

config_dts()
{
	DTS_FILE=$1
	echo "cat $GLOB_DTS >> recipes-bsp/device-tree/files/system-user.dtsi"
	cat $GLOB_DTS >> recipes-bsp/device-tree/files/system-user.dtsi
	# By default, assume this script is used right after dsa_build.sh.
	if [ -f ${ORIGINAL_DIR}/dsa_build/${DSA_NAME}/${DSA_NAME}_fragment.dts ]; then
		echo "cat ${ORIGINAL_DIR}/dsa_build/${DSA_NAME}/${DSA_NAME}_fragment.dts >> $DTS_FILE"
		cat ${ORIGINAL_DIR}/dsa_build/${DSA_NAME}/${DSA_NAME}_fragment.dts >> $DTS_FILE
	fi
}

install_recipes()
{
	META_USER_PATH=$1

	cp -r ${XRT_REPO_DIR}/src/platform/recipes-xrt ${META_USER_PATH}
	# By default, let XRT recipes point to current XRT workspace.
	# PetaLinux fetch xrt source code from this workspace, instead of fetch from github.
	SAVED_OPTIONS_LOCAL=$(set +o)
	set +e
	XRT_BB=${META_USER_PATH}/recipes-xrt/xrt/xrt_git.bb
	ZOCL_BB=${META_USER_PATH}/recipes-xrt/zocl/zocl_git.bb
	grep "inherit externalsrc" $XRT_BB
	if [ $? != 0 ]; then
		echo "inherit externalsrc" >> $XRT_BB
		echo "EXTERNALSRC = \"$XRT_REPO_DIR/src\"" >> $XRT_BB
		echo 'EXTERNALSRC_BUILD = "${WORKDIR}/build"' >> $XRT_BB
	fi

	grep "inherit externalsrc" $ZOCL_BB
	if [ $? != 0 ]; then
		echo "inherit externalsrc" >> $ZOCL_BB
		echo "EXTERNALSRC = \"$XRT_REPO_DIR/src/runtime_src/core/edge/drm/zocl\"" >> $ZOCL_BB
		echo "EXTERNALSRC_BUILD = \"$XRT_REPO_DIR/src/runtime_src/core/edge/drm/zocl\"" >> $ZOCL_BB
	fi
	eval "$SAVED_OPTIONS_LOCAL"

	# If you are using PetaLinux 2018.3 or earlier version, do below step
	if [[ $PETALINUX_VER == "2018"* ]]; then
		echo " * A 2018.X PetaLinux release (${PETALINUX_VER}) was detected, copying opencl-headers recipe:"
		mkdir -p $META_USER_PATH/recipes-xrt/opencl-headers
		wget -O $META_USER_PATH/recipes-xrt/opencl-headers/opencl-headers_git.bb http://cgit.openembedded.org/meta-openembedded/plain/meta-oe/recipes-core/opencl-headers/opencl-headers_git.bb
	fi

	# mnt-sd will run at the Linux boot time, it do below things
	#  1. mount SD cart to /mnt
	#  2. run init.sh in /mnt if it exist
	cp -r ${XRT_REPO_DIR}/src/platform/mnt-sd $META_USER_PATH/recipes-apps/
}

update_append()
{
	BBAPPEND=$1
	echo 'IMAGE_INSTALL_append = " xrt-dev"'            >> $BBAPPEND
	echo 'IMAGE_INSTALL_append = " mnt-sd"'             >> $BBAPPEND
	echo 'IMAGE_INSTALL_append = " xrt"'                >> $BBAPPEND
	echo 'IMAGE_INSTALL_append = " zocl"'               >> $BBAPPEND
	echo 'IMAGE_INSTALL_append = " opencl-headers-dev"' >> $BBAPPEND
	echo 'IMAGE_INSTALL_append = " opencl-clhpp-dev"'   >> $BBAPPEND
}

rootfs_menu()
{
	ROOTFSCONFIG=$1
	echo 'CONFIG_xrt'                                   >> $ROOTFSCONFIG
	echo 'CONFIG_mnt-sd'                                >> $ROOTFSCONFIG
	echo 'CONFIG_xrt-dev'                               >> $ROOTFSCONFIG
	echo 'CONFIG_zocl'                                  >> $ROOTFSCONFIG
	echo 'CONFIG_opencl-clhpp-dev'                      >> $ROOTFSCONFIG
	echo 'CONFIG_opencl-headers-dev'                    >> $ROOTFSCONFIG
}

pre_build_hook()
{
	PETA_DIR=$1
	# Nothing needs to do
}

post_build_hook()
{
	PETA_DIR=$1
	# Nothing needs to do
}

# --- End internal functions

SAVED_OPTIONS=$(set +o)

# Don't print all commands
set +x
# Error on non-zero exit code, by default:
set -e

# Get real script by read symbol link
THIS_SCRIPT=`readlink -f ${BASH_SOURCE[0]}`

THIS_SCRIPT_DIR="$( cd "$( dirname "${THIS_SCRIPT}" )" >/dev/null 2>&1 && pwd )"

PROGRAM=`basename $0`
PATH_TO_DSA=""
CONFIG_FILE=""
PETA_BSP=""
DSA_NAME=""
XRT_REPO_DIR=`readlink -f ${THIS_SCRIPT_DIR}/../../../..`
PETA_CREATE_OPT="--template zynqMP"
CPU_ARCH="a53"
GLOB_DTS=${XRT_REPO_DIR}/src/runtime_src/core/edge/fragments/xlnk_dts_fragment_mpsoc.dts

while [ $# -gt 0 ]; do
	case $1 in
		--help | -h )
			usage_and_exit 0
			;;
		--config | -c )
			shift
			CONFIG_FILE=$1
			;;
		--bsp | -b )
			shift
			PETA_BSP=$1
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
[ ! -f "$PATH_TO_DSA" ] && error "DSA is not exist"

# the setting.sh script of petalinux would set this two environment variable
[ -z "$PETALINUX" -o -z "$PETALINUX_VER" ] && error "PetaLinux is not installed"

[ ! -d "$XRT_REPO_DIR" ] && error "Could not found XRT repository root directory"
# Sanity check done

# Handle PetaLinux version changes...
# Latest version
PETA_CONFIG_OPT="--silentconfig"
ROOTFS_MENU_CONFIG="conf/user-rootfsconfig"

# Older version
if $(version_lt $PETALINUX_VER 2019.1); then
	PETA_CONFIG_OPT=--oldconfig
fi

if $(version_lt $PETALINUX_VER 2019.2); then
	ROOTFS_MENU_CONFIG=recipes-core/images/petalinux-image-full.bbappend
fi

if $(version_lt $PETALINUX_VER 2018.3); then
	ROOTFS_MENU_CONFIG=recipes-core/images/petalinux-image.bbappend
fi

if $(version_lt $PETALINUX_VER 2018.1); then
	echo "The PetaLinux version is less than 2018.1!!!"
	read -p "The script might failed. Take your own risk to continue (y/n)?" choice
	case "$choice" in
		y|Y ) echo "Good luck." ;;
		n|N ) echo "Please try 2018.1 or later"; exit 0 ;;
		*   ) echo "invalid"; exit 1;;
	esac
fi

# End of PetaLinux version changes...

ORIGINAL_DIR=`pwd`

DSA_FILE=`basename $PATH_TO_DSA`
# Normalized Tcl script path
PATH_TO_DSA=`readlink -f $PATH_TO_DSA`
DSA_DIR=`dirname $PATH_TO_DSA`
DSA_NAME=${DSA_FILE%.dsa}

[ ! "$DSA_NAME.dsa" == "$DSA_FILE" ] && error "$DSA_FILE should be <dsa_name>.dsa"

PETALINUX_NAME=$DSA_NAME

if [ -z "$CONFIG_FILE" -a -f "${DSA_DIR}/config.sh" ]; then
	# No CONFIG_FILE from command line and DSA directory has config.sh
	CONFIG_FILE=${DSA_DIR}/config.sh
fi

if [ -f "$CONFIG_FILE" ]; then
	echo "[CMD]: source $CONFIG_FILE"
	source $CONFIG_FILE
else
	echo " * Could not found configure file, use default"
fi

if [ "X$TEMPLATE" == "XzynqMP" ]; then
	PETA_CREATE_OPT="--template zynqMP"
	CPU_ARCH="a53"
	GLOB_DTS=${XRT_REPO_DIR}/src/runtime_src/core/edge/fragments/xlnk_dts_fragment_mpsoc.dts
fi

if [ "X$TEMPLATE" == "Xzynq" ]; then
	PETA_CREATE_OPT="--template zynq"
	CPU_ARCH="a9"
	GLOB_DTS=${XRT_REPO_DIR}/src/runtime_src/core/edge/fragments/xlnk_dts_fragment_zynq.dts
fi

if [ -f "$PETA_BSP" ]; then
	echo " * Create PetaLinux from BSP (-s $PETA_BSP)"
	PETA_CREATE_OPT="-s $PETA_BSP"
else
	echo " * Create PetaLinux from template ($PETA_CREATE_OPT)"
fi

echo "** START [${BASH_SOURCE[0]}] **"
echo " PETALINUX: $PETALINUX"
echo " petalinux-create option: $PETA_CREATE_OPT"
echo ""

if [ ! -d $PETALINUX_NAME ]; then
  echo " * Create PetaLinux Project: $PETALINUX_NAME"
  echo "[CMD]: petalinux-create -t project -n $PETALINUX_NAME $PETA_CREATE_OPT"
  petalinux-create -t project -n $PETALINUX_NAME $PETA_CREATE_OPT
else
  echo " * PetaLinux Project existed: $PETALINUX_NAME. Skip petalinux-create"
  echo " NOTE: This is not a incremental build script. Remove $PETALINUX_NAME/ or take you own risk."
fi

mkdir -p ${PETALINUX_NAME}/build/conf/

echo " * Configuring PetaLinux Project"
config_peta ${PETALINUX_NAME}/project-spec/configs/config
echo "[CMD]: petalinux-config -p $PETALINUX_NAME --get-hw-description=${DSA_DIR} $PETA_CONFIG_OPT"
petalinux-config -p $PETALINUX_NAME --get-hw-description=${DSA_DIR} $PETA_CONFIG_OPT

echo " * Change directory to ${PETALINUX_NAME}/project-spec/meta-user/"
cd ${PETALINUX_NAME}/project-spec/meta-user/

echo " * Adding XRT, HAL, Driver recipes"
echo " ** Installing recipes to meta-user/"
install_recipes .

if [ ! -d recipes-core/images ]; then
	echo "No project-spec/meta-user/recipes-core/images/ folder."
	echo "After PetaLinux 2019.2, this was replaced by project-spec/meta-user/conf/user-rootfsconfig"
	if [ ! -f conf/user-rootfsconfig ]; then
		echo "oops. conf/user-rootfsconfig not exist.. please check your petalinux version."
		exit 1
	fi
fi

# Looks like 2019.2 only has one of them.
if [ -f conf/user-rootfsconfig ]; then
	rootfs_menu $ROOTFS_MENU_CONFIG
else
	update_append $ROOTFS_MENU_CONFIG
fi

echo " * Adding XRT Kernel Node to Device Tree"
config_dts recipes-bsp/device-tree/files/system-user.dtsi

echo " * Configuring Linux kernel"
mkdir -p recipes-kernel/linux/linux-xlnx
echo 'SRC_URI += "file://user.cfg"' >> recipes-kernel/linux/linux-xlnx_%.bbappend
echo 'FILESEXTRAPATHS_prepend := "${THISDIR}/${PN}:"' >> recipes-kernel/linux/linux-xlnx_%.bbappend

config_kernel recipes-kernel/linux/linux-xlnx/user.cfg
echo "[CMD]: petalinux-config -c kernel $PETA_CONFIG_OPT"
petalinux-config -c kernel $PETA_CONFIG_OPT

echo " * Configuring rootfs"
# Saves to: ${PETALINUX_NAME}/project-spec/configs/rootfs_config
cp ../configs/rootfs_config{,.orig}
config_rootfs ../configs/rootfs_config
echo "[CMD]: petalinux-config -c rootfs $PETA_CONFIG_OPT"
petalinux-config -c rootfs $PETA_CONFIG_OPT

pre_build_hook $ORIGINAL_DIR/$PETALINUX_NAME

# Build package
echo " * Performing PetaLinux Build (from: ${PWD})"
echo "[CMD]: petalinux-build"
petalinux-build

post_build_hook $ORIGINAL_DIR/$PETALINUX_NAME

cd $ORIGINAL_DIR

# If DSA_DIR has src/$CPU_ARCH/xrt/image, let's do below extra steps
if [ -d "${DSA_DIR}/src/${CPU_ARCH}/xrt/image" ]; then
	echo " ** The DSA directory has src/${CPU_ARCH}/xrt/image. Update images and sysroot"
	echo " * Copying PetaLinux boot files (from: $PWD)"
	cp -f ./${PETALINUX_NAME}/images/linux/image.ub 	${DSA_DIR}/src/${CPU_ARCH}/xrt/image/image.ub
	mkdir -p ${DSA_DIR}/src/boot
	if [ "X$TEMPLATE" != "Xzynq" ]; then
		cp -f ./${PETALINUX_NAME}/images/linux/bl31.elf 	${DSA_DIR}/src/boot/bl31.elf
		cp -f ./${PETALINUX_NAME}/images/linux/pmufw.elf 	${DSA_DIR}/src/boot/pmufw.elf
	fi
	cp -f ./${PETALINUX_NAME}/images/linux/u-boot.elf 	${DSA_DIR}/src/boot/u-boot.elf

	# NOTE: Renames
	if [ "X$TEMPLATE" == "Xzynq" ]; then
		cp -f ./${PETALINUX_NAME}/images/linux/zynq_fsbl.elf ${DSA_DIR}/src/boot/fsbl.elf
	else
		cp -f ./${PETALINUX_NAME}/images/linux/zynqmp_fsbl.elf ${DSA_DIR}/src/boot/fsbl.elf
	fi

	# Prepare Sysroot directory
	echo " * Preparing Sysroot"
	if [ "X$TEMPLATE" == "Xzynq" ]; then
		mkdir -p ${DSA_DIR}/src/arm-xilinx-linux
		cd       ${DSA_DIR}/src/arm-xilinx-linux
	else
		mkdir -p ${DSA_DIR}/src/aarch64-xilinx-linux
		cd       ${DSA_DIR}/src/aarch64-xilinx-linux
	fi

	tar zxf $ORIGINAL_DIR/${PETALINUX_NAME}/images/linux/rootfs.tar.gz
fi

eval "$SAVED_OPTIONS"; # Restore shell options
echo "** COMPLETE [${BASH_SOURCE[0]}] **"
echo ""
