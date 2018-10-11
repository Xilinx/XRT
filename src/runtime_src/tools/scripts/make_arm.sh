#! /bin/bash --

set -e

usage()
{
	echo "Usage: make_arm.sh <target>"
	echo ""
	echo "target:"
	echo "    arm      Build ARM 32bit library (e.g. zc702/zc706)"
	echo "    aarch64  Build ARM 64bit library (e.g. zcu102/zcu106)"
}

usage_exit()
{
	usage
	exit 1
}

make_libxclzynqdrv()
{
	cc=$1
	shim_dir=$2
	debug_dir=$3
	release_dir=$4
	cur_dir=$(pwd)

	cd $shim_dir

	make clean > /dev/null
	make debug=1 CROSS_COMPILE=$cc
	mv libxclzynqdrv.so $debug_dir
	make clean

	make clean > /dev/null
	make debug=0 CROSS_COMPILE=$cc
	mv libxclzynqdrv.so $release_dir
	make clean

	cd $cur_dir
}

target=aarch64
script_dir=$(dirname "$0")
shim_dir=$(readlink -f ${script_dir}/../../driver/zynq/user)
build_dir=$(readlink -f ${script_dir}/../../../../build/)

if [ $# -eq 1 ]; then
	case $1 in
		arm)
			target=arm
			;;
		aarch64)
			target=aarch64
			;;
		*)
			usage_exit
			;;
	esac
else
			usage_exit
fi

# Check Debug/Release directory. If not exist, create
if [ ! -d $build_dir/Debug ]; then
	echo "Directory $build_dir/Debug no exist, creating"
	mkdir $build_dir/Debug
fi

if [ ! -d $build_dir/Release ]; then
	echo "Directory $build_dir/Release no exist, creating"
	mkdir $build_dir/Release
fi

if [ $target == "arm" ]; then
	if [ ! -x "$(command -v arm-linux-gnueabihf-gcc)" ]; then
		echo "Could not found arm-linux-gnueabihf-gcc. Please install Xilinx SDK"
		echo "Then source $XILINX_SDK/setting64.sh or $XILINX_SDK/setting64.csh"
		exit 1
	fi

	cc=arm-linux-gnueabihf-
else
	if [ ! -x "$(command -v aarch64-linux-gnu-gcc)" ]; then
		echo "Could not found aarch64-linux-gnu-gcc. Please install Xilinx SDK"
		echo "Then source $XILINX_SDK/setting64.sh or $XILINX_SDK/setting64.csh"
		exit 1
	fi

	cc=aarch64-linux-gnu-
fi

mkdir -p ${build_dir}/Debug/arm_libs/${target}
debug_dir=${build_dir}/Debug/arm_libs/${target}
mkdir -p ${build_dir}/Release/arm_libs/${target}
release_dir=${build_dir}/Release/arm_libs/${target}

make_libxclzynqdrv $cc $shim_dir $debug_dir $release_dir
