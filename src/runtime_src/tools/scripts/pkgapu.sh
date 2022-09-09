#!/bin/bash
#
# Copyright (C) 2021-2022 Xilinx, Inc. All rights reserved.
#

# This script creates rpm and deb packages for Versal APU firmware and Built-in PS Kernels.
# The firmware file (.xsabin) is installed to /lib/firmware/xilinx
# The Built-in PS Kernel (ps_kernels.xclbin) is installed to /lib/firmware/xilinx/ps_kernels
#
# The script is assumed to run on a host or docker that has all the
# necessary tools is accessible.
#     mkimage
#     xclbinutil
#     bootgen
#     rpmbuild
#     dpkg-deb 
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
	echo "          -images                         Versal images path"
	echo "          -clean                          Remove build files"
        echo "          -output                         output path"
        echo "          -idcode                         id code of the part"
        echo "          -package-name                   package name"
	echo "This script requires tools: mkimage, xclbinutil, bootgen, rpmbuild, dpkg-deb. "
	echo "There is mkimage in petalinux build, e.g."
	echo "/proj/petalinux/2021.2/petalinux-v2021.2_daily_latest/tool/petalinux-v2021.2-final/components/yocto/buildtools/sysroots/x86_64-petalinux-linux/usr/bin/mkimage"
	echo ""
}

usage_and_exit()
{
	usage
	exit $1
}

dodeb()
{
	dir=$BUILD_DIR/debbuild/$PKG_NAME-$PKG_VER
	mkdir -p $dir/DEBIAN

cat <<EOF >$dir/DEBIAN/control

Package: $PKG_NAME
Architecture: all
Version: $PKG_VER
Priority: optional
Description: Xilinx Versal firmware
Maintainer: Xilinx Inc.

EOF

	app_root=$1
	rsync -avz $app_root $dir
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

SYSTEM_DTB_ADDR="0x40000"
KERNEL_ADDR="0x20100000"
ROOTFS_ADDR="0x21000000"

# this address needs to be in sync with VMR
METADATA_ADDR="0x7FBD0000"
METADATA_BUFFER_LEN=131072

# default id code is for vck5000 part
ID_CODE="0x14ca8093"

# default package name is xrt-apu
PKG_NAME="xrt-apu"

clean=0
while [ $# -gt 0 ]; do
	case $1 in
		-help )
			usage_and_exit 0
			;;
		-images )
			shift
			IMAGES_DIR=$1
			;;
                -output )
			shift
                        OUTPUT_DIR=$1
			;;
                -idcode )
			shift
                        ID_CODE=$1
			;;
                -package-name )
			shift
                        PKG_NAME=$1
			;;
		-clean )
			clean=1
			;;
		* )
			error "Unrecognized option: $1"
			;;
	esac
	shift
done

if [[ ! -d $OUTPUT_DIR ]]; then
	error "Please specify the valid output path by -output"
fi
OUTPUT_DIR=`realpath $OUTPUT_DIR`

BUILD_DIR="$OUTPUT_DIR/apu_build"
PACKAGE_DIR="$BUILD_DIR"
FW_FILE="$BUILD_DIR/lib/firmware/xilinx/xrt-versal-apu.xsabin"
INSTALL_ROOT="$BUILD_DIR/lib"
SDK="$BUILD_DIR/lib/firmware/xilinx/sysroot/sdk.sh"

if [[ $clean == 1 ]]; then
	echo $PWD
	echo "/bin/rm -rf $BUILD_DIR"
	/bin/rm -rf $BUILD_DIR
	exit 0
fi

if [[ ! -d $IMAGES_DIR ]]; then
	error "Please specify the valid path of APU images by -images"
fi
IMAGES_DIR=`realpath $IMAGES_DIR`
#hack to fix pipeline. Need to file a CR on xclnbinutil
source /proj/xbuilds/2022.2_0823_1/installs/lin64/Vitis/2022.2/settings64.sh


if [[ ! (`which mkimage` && `which bootgen` && `which xclbinutil`) ]]; then
	error "Please source Xilinx VITIS and Petalinux tools to make sure mkimage, bootgen and xclbinutil is accessible."
fi

PKG_VER_WITH_RELEASE=`cat $IMAGES_DIR/rootfs.manifest | grep "^xrt " | sed s/.*\ //`
if [[ "X$PKG_VER_WITH_RELEASE" == "X" ]]; then
	error "Can not get package version"
fi

PKG_VER=${PKG_VER_WITH_RELEASE#*.}
PKG_RELEASE=${PKG_VER_WITH_RELEASE%%.*}

# Add patch number in version if 'XRT_VERSION_PATCH' env variable is defined
if [[ ! -z $XRT_VERSION_PATCH ]]; then
	PKG_VER=${PKG_VER%.*}.$XRT_VERSION_PATCH
fi

echo APU Package version : "$PKG_VER"
echo APU Package release : "$PKG_RELEASE"

if [ -d $BUILD_DIR ]; then
	rm -rf $BUILD_DIR
fi

mkdir -p $BUILD_DIR
if [[ ! -d $BUILD_DIR ]]; then
	error "failed to create dir $BUILD_DIR"
fi

#
# Generate Linux PDI
#
IMAGE_UB="$BUILD_DIR/Image.gz.u-boot"
BIF_FILE="$BUILD_DIR/apu.bif"
cat << EOF > $BIF_FILE
all:
{
    id_code = $ID_CODE
    extended_id_code = 0x01
    image {
        id = 0x1c000000, name=apu_subsystem
        { core=a72-0, exception_level=el-3, trustzone, file=$IMAGES_DIR/bl31.elf }
        { core=a72-0, exception_level=el-2, file=$IMAGES_DIR/u-boot.elf }
        { load=$ROOTFS_ADDR, file=$IMAGES_DIR/rootfs.cpio.gz.u-boot }
        { load=$KERNEL_ADDR, file=$IMAGE_UB }
        { load=0x20000000, file=$BUILD_DIR/boot.scr }
        { load=$METADATA_ADDR file=$BUILD_DIR/metadata.dat }
    }
}
EOF

MKIMAGE=mkimage

#
# Generate u-boot script
#
UBOOT_SCRIPT="$BUILD_DIR/boot.scr"
UBOOT_CMD="$BUILD_DIR/boot.cmd"
cat << EOF > $UBOOT_CMD
setenv bootargs "console=ttyUL0 clk_ignore_unused modprobe.blacklist=allegro,al5d"
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
IMAGE_ELF_START="0x80000"

cp $IMAGE $BUILD_DIR/Image
yes| gzip $BUILD_DIR/Image
$MKIMAGE -n 'Kernel Image' -A arm64 -O linux -C none -T kernel -C gzip -a $IMAGE_ELF_START -e $IMAGE_ELF_START -d $BUILD_DIR/Image.gz $IMAGE_UB
if [[ ! -e $IMAGE_UB ]]; then
	error "failed to generate kernel image"
fi


# pick bootgen from vitis
if [[ "X$XILINX_VITIS" == "X" ]]; then
  echo " **ERROR: XILINX_VITIS is empty, please source vitis and rerun"
  exit 1;
fi
BOOTGEN=$XILINX_VITIS/bin/bootgen

#
# Generate metadata.dat
#
METADATA="$BUILD_DIR/metadata.dat"
cat $IMAGE $IMAGES_DIR/rootfs.cpio.gz | md5sum | awk '{print $1}' >$METADATA.tmp
cat $IMAGES_DIR/rootfs.manifest >>$METADATA.tmp
dd if=$METADATA.tmp of=$METADATA bs=1 count=$METADATA_BUFFER_LEN

#
# Generate pdi
#
APU_PDI="$BUILD_DIR/apu.pdi"
$BOOTGEN -arch versal -padimageheader=0 -log trace -w -o $APU_PDI -image $BIF_FILE
if [[ ! -e $APU_PDI ]]; then
	error "failed to generate APU pdi"
fi

mkdir -p `dirname $FW_FILE`
echo "xclbinutil --add-section PDI:RAW:$APU_PDI --output $FW_FILE"
xclbinutil --add-section PDI:RAW:$APU_PDI --output $FW_FILE
if [[ ! -e $FW_FILE ]]; then
	error "failed to generate XSABIN"
fi

#copy the sysroot sdk.sh
mkdir -p `dirname $SDK`
if [ -e $IMAGES_DIR/sdk.sh ]; then
        echo "sdk.sh exists copy it to apu package"
        cp $IMAGES_DIR/sdk.sh $SDK
fi
# Generate PS Kernel xclbin
# Hardcoding the ps kernel xclbin name to ps_kernels.xclbin
# We can create one xclbin per PS Kernel also based on future requirements
PS_KERNELS_XCLBIN="$BUILD_DIR/lib/firmware/xilinx/ps_kernels/ps_kernels.xclbin"
#Extract ps_kernels_lib from rootfs tar
tar -C $BUILD_DIR -xf $IMAGES_DIR/rootfs.tar.gz ./usr/lib/ps_kernels_lib
if [ $? -eq 0 ]; then
    PS_KERNEL_DIR=$BUILD_DIR/usr/lib/ps_kernels_lib
    ps_kernel_command="xclbinutil --output $PS_KERNELS_XCLBIN "
    pk_exists=0
    for entry in "$PS_KERNEL_DIR"/*.so
    do
            pk_exists=1
            ps_kernel_command+=" --add-pskernel $entry"
    done
    # Generate xclbin if atleast one PS Kernel exists
    if [ $pk_exists -eq 1 ]; then
        # Run final xclbinutil command to generate the PS Kernels xclbin
        echo "Running $ps_kernel_command"
        $ps_kernel_command
    fi
fi

dodeb $INSTALL_ROOT
dorpm $INSTALL_ROOT

# Add _petalinux in apu package name for xrt pipeline build
# remove this check after apu package build is removed from emb pipeline
if [[ ! -z $XRT_VERSION_PATCH ]]; then
    cp -v $BUILD_DIR/${PKG_NAME}*.rpm $OUTPUT_DIR/${PKG_NAME}_${PKG_RELEASE}.${PKG_VER}_petalinux.noarch.rpm
    cp -v $BUILD_DIR/${PKG_NAME}*.deb $OUTPUT_DIR/${PKG_NAME}_${PKG_RELEASE}.${PKG_VER}_petalinux_all.deb
else
    cp -v $BUILD_DIR/${PKG_NAME}*.rpm $OUTPUT_DIR/${PKG_NAME}_${PKG_RELEASE}.${PKG_VER}.noarch.rpm
    cp -v $BUILD_DIR/${PKG_NAME}*.deb $OUTPUT_DIR/${PKG_NAME}_${PKG_RELEASE}.${PKG_VER}_all.deb
fi
rm -rf $BUILD_DIR
