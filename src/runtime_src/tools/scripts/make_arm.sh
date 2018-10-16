#! /bin/bash --

set -e

usage()
{
	echo "Usage: make_arm.sh --boost_inc <Boost include> --boost_lib <Boost library> --target <target>"
	echo "--boost_inc	The path of boost include directory"
	echo "--boost_lib	The path of boost library directory"
	echo "--target		Target architecture (arm/aarch64)"
	echo ""
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

make_xilinxopencl()
{
	cc=$1
	runtime_src_dir=$2
	debug_dir=$3
	release_dir=$4
	boost_inc=$5
	boost_lib=$6

	cur_dir=$(pwd)

	cd $runtime_src_dir
	cl_inc=${debug_dir}/include
	out_dir=${debug_dir}/xilinxopencl
	make debug=1 CROSS_COMPILE=$cc OUT_DIR=$out_dir BOOST_INC_DIR=$boost_inc BOOST_LIB_DIR=$boost_lib CL_INC_DIR=$cl_inc -j4

	cl_inc=${release_dir}/include
	out_dir=${release_dir}/xilinxopencl
	make debug=0 CROSS_COMPILE=$cc OUT_DIR=$out_dir BOOST_INC_DIR=$boost_inc BOOST_LIB_DIR=$boost_lib CL_INC_DIR=$cl_inc -j4

	cd $cur_dir
}

target=aarch64
script_dir=$(dirname "$0")
shim_dir=$(readlink -f ${script_dir}/../../driver/zynq/user)
build_dir=$(readlink -f ${script_dir}/../../../../build)
runtime_src_dir=$(readlink -f ${script_dir}/../..)

boost_inc=
boost_lib=

while [ $# -gt 0 ]; do
	case $1 in
		--target | -t)
			if [ $2 == "aarch64" ]; then
				target=aarch64
			else
				target=arm
			fi
			shift
			;;
		--boost_inc)
			boost_inc=$2
			shift
			;;
		--boost_lib)
			echo boost_lib $2
			boost_lib=$2
			shift
			;;
		*)
			usage_exit
			shift
			;;
	esac
	shift
done

echo ${boost_inc}
echo ${boost_lib}

# Build ---- libxclzynqdrv.so ----
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
# end build libxclzynqdrv.so

# Build ---- libxilinxopencl.so ----
# Check required header files and copy it
if [ -d /usr/include/CL ]; then
	mkdir -p ${debug_dir}/include
	mkdir -p ${release_dir}/include
	cp -rf /usr/include/CL ${debug_dir}/include
	cp -rf /usr/include/CL ${release_dir}/include
else
	echo "Could not find OpenCL header at /usr/include"
	echo "OpenCL headers are required to cross-compile libxilinxopencl.so"
	echo "Please install all XRT dependecies by run xrtdeps.sh"
	exit 1
fi

if [ -f /usr/include/ocl_icd.h ]; then
	cp -rf /usr/include/ocl_icd.h ${debug_dir}/include
	cp -rf /usr/include/ocl_icd.h ${release_dir}/include
else
	echo "Could not find ocl_icd.h at /usr/include"
	echo "ocl_icd.h are required to cross-compile libxilinxopencl.so"
	echo "Please install all XRT dependecies by run xrtdeps.sh"
	exit 1
fi

if [ -z $boost_inc ]; then
	echo "--boost_inc could not be empty."
fi

if [ -z $boost_lib ]; then
	echo "--boost_lib could not be empty."
fi

make_xilinxopencl $cc $runtime_src_dir $debug_dir $release_dir $boost_inc $boost_lib

# end build libxilinxopencl.so
