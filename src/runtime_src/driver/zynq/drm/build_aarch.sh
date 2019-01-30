#!/bin/bash --

CROSS_COMPILE=aarch64-linux-gnu-
DRIVER=zocl
LINUXDIR=/scratch/minm/linux-xlnx/oe-workdir/linux-xlnx-4.14-xilinx-v2018.1+git999/

while getopts ":l:d:c:" opt; do
	case $opt in
		l)
			LINUXDIR=$OPTARG
			;;
		d)
			DRIVER=$OPTARG
			;;
		c)
			CROSS_COMPILE=$OPTARG
			;;
		\?)
			echo "Unknown option: -$OPTARG" >&2
			;;
	esac

done

echo "make LINUXDIR=${LINUXDIR} CROSS_COMPILE=${CROSS_COMPILE} DRIVER=${DRIVER}"
make LINUXDIR=${LINUXDIR} CROSS_COMPILE=${CROSS_COMPILE} DRIVER=${DRIVER}

