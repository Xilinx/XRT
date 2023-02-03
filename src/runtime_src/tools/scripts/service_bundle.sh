#!/bin/bash

# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
# This script program plp based on input xclbin
# This will be obsoleted after plp auto download is fully impletmented

TIME=`date +%F-%T`
ID=`id -un`
FULL_CMD=${BASH_SOURCE[0]}
BASE_CMD=`basename "$0"`

progress() {
	echo "running: $func - progress: $(($1+1))/$2"
	#echo -ne "running: $func - progress: \b\b\b$(($1+1))/$(($2+1))\b\b\b"
}

usage() {
    echo "Usage: $BASE_CMD [options]"
    echo "  options:"
    echo "  --out, -o           output directory"
    echo "  --device, -d        card device bdf"
    echo "  --verbose, -v       verbose output"
    exit $1
}

while [ $# -gt 0 ]; do
	case "$1" in
		-help)
			usage 0
			;;
		-o | --out)
			shift
			PATH_TO_OUTPUT=$1
			;;
		-d | --device)
			shift
			DEVICE_BDF=$1
			;;
		-v | --verbose)
			VERBOSE=1
			;;
		* | -* | --*)
			echo "$1 invalid argument."
			usage 1
			;;
	esac
	shift
done


#############################
# Funcs start here
#############################
xbutil_output()
{
	xbutil examine -d $DEVICE_BDF:0 > $1/xbutil_examine.txt
	xbutil examine -d $DEVICE_BDF:0 -r all > $1/xbutil_examine_all.txt
	xbutil validate -d $DEVICE_BDF:0 --verbose> $1/xbutil_validate.txt
}

xbmgmt_output()
{
	xbmgmt examine > $1/xbmgmt_examine.txt
	xbmgmt examine -d $DEVICE_BDF:0 >> $1/xbmgmt_examine.txt

	xbmgmt examine -d $DEVICE_BDF:0 -r all > $1/xbmgmt_examine_all.txt
}

debug_logs()
{
	dmesg -T > $1/dmesg_current.txt
	cp /var/log/dmesg* $1/
	cp /var/log/syslog* $1/
	cat /sys/kernel/debug/xclmgmt/trace > $1/sys_kernel_debug_xclmgmt.txt &
	PID1=$!
	cat /sys/kernel/debug/xocl/trace > $1/sys_kernel_debug_xocl.txt &
	PID2=$!
	sleep 2
	kill $PID1
	kill $PID2
}

vmr_logs()
{
	xbmgmt examine -d $DEVICE_BDF:0 -r vmr --verbose > $1/xbmgmt_vmr_verbose.txt
	
	VMR_SYSFS="/sys/bus/pci/devices/0000\:$DEVICE_BDF\:00.0/xgq_vmr.m.*/"

	nodeArray=(
		vmr_endpoint
		vmr_log
		vmr_mem_stats
		vmr_plm_log
		vmr_status
		vmr_system_dtb
		vmr_task_stats
		vmr_verbose_info
	)

	for node in "${nodeArray[@]}"
	do
		sys_node=`ls $VMR_SYSFS/$node`
		cp $sys_node $1/
	done
}

funcArray=(
	vmr_logs
	debug_logs
	xbmgmt_output
	xbutil_output
)
#############################
# Read program starts here
#############################

if [ -z $DEVICE_BDF ];then
	echo "Please select one device"
	usage 0
fi

XRT_PATH=`which xbmgmt > /dev/null`
if [ $? -ne 0 ];then
	echo "XRT is not found, using default XRT"
	source /opt/xilinx/xrt/setup.sh > /dev/null
	XRT_PATH=`which xbmgmt > /dev/null`
else
	echo "using configured XRT"
fi
echo "$XRT_PATH"

if [ -z $PATH_TO_OUTPUT];then
	PATH_TO_OUTPUT="."
fi

PATH_TO_OUTPUT=$PATH_TO_OUTPUT/xrt_bundle

#a=${!funcArray[@]}
total=${#funcArray[@]}
index=0
for func in "${funcArray[@]}"
do
	progress $index $total $func

	if [ ! -d $PATH_TO_OUTPUT/$func ];then
		mkdir -p "$PATH_TO_OUTPUT/$func"
	fi

	$func "$PATH_TO_OUTPUT/$func"

	((index++))
done

echo "$BASE_CMD finished"
if [ ! -z $VERBOSE ];then
	tree $PATH_TO_OUTPUT
fi
