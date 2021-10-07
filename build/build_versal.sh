#!/bin/bash

#
# e.g. ./build_versal.sh 5.10.0-xilinx-v2021.2-202120.2.12.0-r0.0 versal
#
error()
{
	echo "ERROR: $1" 1>&2
	usage_and_exit 1
}

usage()
{
	echo "Usage: $PROGRAM [options]"
	echo "  options:"
	echo "          -help                           Print this usage"
	echo "          -setup				Setup file to use"
        echo "          -clean                          Remove build files"
	echo ""
}

usage_and_exit()
{
	usage
	exit $1
}

dodeb()
{
	dir=$BUILD_DIR/debbuild/$PKG_NAME-$version-$revision
	mkdir -p $dir/DEBIAN
cat <<EOF >$dir/DEBIAN/control

Package: $PKG_NAME
Architecture: all
Version: $version-$revision
Priority: optional
Description: Xilinx Versal firmware
Maintainer: Xilinx Inc.

EOF

	mkdir -p $dir/lib/firmware/xilinx
	rsync -avz $1 $dir/lib/firmware/xilinx/$PKG_NAME-$version-$revision.pdi
	dpkg-deb --build $dir $PACKAGE_DIR
}

dorpm()
{
	app_root=$1
	dir=$BUILD_DIR/rpmbuild/$PKG_NAME-$PKG_VER
	mkdir -p $dir/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

	appfiles=(`find $app_root -type f`)
	appdir=${app_root%/*}
	appfiles=( "${appfiles[@]/$appdir/}" )

cat <<EOF > $dir/SPECS/$PKG_NAME.spec

buildroot: %{_topdir}
summary: Xilinx Versal firmware
name: $PKG_NAME
version: $PKG_VER
release: 0
license: apache
vendor: Xilinx Inc.

%description
Xilinx Versal firmware

%prep

%install
rsync -avz $app_root %{buildroot}/

%files
%defattr(-,root,root,-)

EOF

	for f in "${appfiles[@]}"; do
		echo $f >> $dir/SPECS/$PKG_NAME.spec
	done

	echo "rpmbuild --target=noarch --define '_topdir $dir' -bb $dir/SPECS/$PKG_NAME.spec"
	rpmbuild --target=noarch --define '_topdir '"$dir" -bb $dir/SPECS/$PKG_NAME.spec

	cp $dir/RPMS/noarch/*.rpm $PACKAGE_DIR
}

SETTINGS_FILE="petalinux.build"

THIS_SCRIPT=`readlink -f ${BASH_SOURCE[0]}`
THIS_SCRIPT_DIR="$( cd "$( dirname "${THIS_SCRIPT}" )" >/dev/null 2>&1 && pwd )"
VERSAL_BUILD_DIR="$THIS_SCRIPT_DIR/versal"
IMAGES_DIR="$VERSAL_BUILD_DIR/images/linux"
BUILD_DIR="$VERSAL_BUILD_DIR/apu_build"
PACKAGE_DIR="$VERSAL_BUILD_DIR"
FW_FILE="$BUILD_DIR/lib/firmware/xilinx/xrt-versal-apu.xsabin"
INSTALL_ROOT="$BUILD_DIR/lib"
PKG_NAME="xrt-apu"

SYSTEM_DTB_ADDR="0x20001000"
KERNEL_ADDR="0x30000000"
ROOTFS_ADDR="0x32000000"

if [ -f $SETTINGS_FILE ]; then
	source $SETTINGS_FILE
fi
MKIMAGE="$PETALINUX/components/yocto/buildtools/sysroots/x86_64-petalinux-linux/usr/bin/mkimage"
if [ ! -f $MKIMAGE ]; then
	error "Can not find mkimage(1) at $MKIMAGE"
fi

PKG_VER=`basename $VERSAL_BUILD_DIR/xrt-[0-9]* | awk -F'-' '{print $2}'`
if [[ "X$PKG_VER" == "X" ]]; then
	error "Can not get package version"
fi
echo VERSION "$PKG_VER"

if [ -d $BUILD_DIR ]; then
	rm -rf $BUILD_DIR
fi
mkdir -p $BUILD_DIR

#
# Generate Linux PDI
#
BIF_FILE="$BUILD_DIR/apu.bif"
cat << EOF > $BIF_FILE
all:
{
    extended_id_code = 0x01
    image {
        id = 0x1c000000, name=apu_subsystem
        { core=a72-0, exception_level=el-3, trustzone, file=$IMAGES_DIR/bl31.elf }
        { core=a72-0, exception_level=el-2, file=$IMAGES_DIR/u-boot.elf }
        { load=0x32000000, file=$IMAGES_DIR/rootfs.cpio.gz.u-boot }
        { load=0x30000000, file=$BUILD_DIR/Image.ub }
        { load=0x20000000, file=$BUILD_DIR/boot.scr }
    }
}
EOF

#
# Generate u-boot script
#
UBOOT_SCRIPT="$BUILD_DIR/boot.scr"
UBOOT_CMD="$BUILD_DIR/boot.cmd"
cat << EOF > $UBOOT_CMD
bootm $KERNEL_ADDR $ROOTFS_ADDR $SYSTEM_DTB_ADDR
EOF
$MKIMAGE -A arm -O linux -T script -C none -a 0 -e 0 -n "boot" -d $UBOOT_CMD $UBOOT_SCRIPT
if [[ ! -e $UBOOT_SCRIPT ]]; then
	error "failed to generate uboot script"
fi

#
# Generate kernel u-boot image
#
IMAGE="$IMAGES_DIR/Image"
IMAGE_UB="$BUILD_DIR/Image.ub"
IMAGE_ELF_START="0x80000"
$MKIMAGE -n 'Kernel Image' -A arm64 -O linux -C none -T kernel -C gzip -a $IMAGE_ELF_START -e $IMAGE_ELF_START -d $IMAGE $IMAGE_UB
if [[ ! -e $IMAGE_UB ]]; then
	error "failed to generate kernel image"
fi


#
# Generate pdi
#
APU_PDI="$BUILD_DIR/apu.pdi"
bootgen -arch versal -padimageheader=0 -log trace -w -o $APU_PDI -image $BIF_FILE
if [[ ! -e $APU_PDI ]]; then
	error "failed to generate APU pdi"
fi

mkdir -p `dirname $FW_FILE`
xclbinutil --add-section PDI:RAW:$APU_PDI --output $FW_FILE
if [[ ! -e $FW_FILE ]]; then
	error "failed to generate XSABIN"
fi
#dodeb $APU_PDI
dorpm $INSTALL_ROOT
