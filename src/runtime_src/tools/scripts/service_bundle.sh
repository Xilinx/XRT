#!/bin/bash

# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2023 Xilinx, Inc. All rights reserved.
#
# This script collects debugging logs for XRT managed sub-systems

TIME=`date +%F-%T`
ID=`id -un`
FULL_CMD=${BASH_SOURCE[0]}
BASE_CMD=`basename "$0"`

# $1 index, $2 total, $3 func name
progress() {
	echo -n "running: $3 - progress: $(($1+1))/$2"
}

usage() {
    echo "Usage: $BASE_CMD [options]"
    echo "  options:"
    echo "  --out, -o           output directory"
    echo "  --device, -d        card device bdf"
    echo "  --all, -a           collect all, including large size system core dump"
    echo "  --verbose, -v       verbose output"
    echo "Example: $BASE_CMD -d 9f"
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
		-a | --all)
			ALL_LOG=1
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

# $1 output directory
core_dumps()
{
	if [ -z $ALL_LOG ];then
		echo -n " - skipped: -a to enable"
		return 0
	fi

	mkdir -p $1/var/crash/
	cp -r /var/crash/** $1/var/crash/
	
	mkdir -p $1/cores/
	ls *.core > /dev/null 2>&1
	if [ $? -eq 0 ];then
		cp *.core $1/cores/
	fi

	ls /opt/xilinx/xrt/bin/*.core > /dev/null 2>&1
	if [ $? -eq 0 ];then
		cp /opt/xilinx/xrt/bin/*.core $1/cores/
	fi
}

# $1 output directory
xbutil_output()
{
	xbutil examine -d $DEVICE_BDF:0 > $1/xbutil_examine.txt
	xbutil examine -d $DEVICE_BDF:0 -r all > $1/xbutil_examine_all.txt
	xbutil validate -d $DEVICE_BDF:0 -r quick --verbose> $1/xbutil_validate.txt
}

# $1 output directory
xbmgmt_output()
{
	xbmgmt examine > $1/xbmgmt_examine.txt
	xbmgmt examine -d $DEVICE_BDF:0 >> $1/xbmgmt_examine.txt

	xbmgmt examine -d $DEVICE_BDF:0 -r all > $1/xbmgmt_examine_all.txt
}

# $1 output directory
debug_logs()
{
	mkdir -p $1/var/log/
	cp -r /var/log/** $1/var/log/

	for xclmgmt in `ls /sys/module/xclmgmt/parameters/`
	do
		cat /sys/module/xclmgmt/parameters/$xclmgmt > $1/xclmgmt_parameters_$xclmgmt.txt
	done

	for xocl in `ls /sys/module/xocl/parameters/`
	do
		cat /sys/module/xocl/parameters/$xocl > $1/xocl_parameters_$xocl.txt
	done

	dmesg -T > $1/dmesg_current.txt

	cat /sys/kernel/debug/xclmgmt/trace > $1/sys_kernel_debug_xclmgmt.txt &
	PID1=$!
	cat /sys/kernel/debug/xocl/trace > $1/sys_kernel_debug_xocl.txt &
	PID2=$!
	sleep 2
	# -s 2(SIGINT)
	kill -s 2 $PID1
	kill -s 2 $PID2
}

# $1 output directory
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

# $1 output directory
host_info()
{
	cat /proc/iomem > $1/proc_iomem.txt
	cat /proc/interrupts > $1/proc_interrupts.txt
	cat /etc/default/grub > $1/etc_grub_cfg.txt
	dmesg | grep iommu > $1/iommu_dump.txt

	#########################IOMMU Groups########################################
	rm -r $1/iommu_groups.txt 2> /dev/null
	export devs=$(find /sys/kernel/iommu_groups/* -maxdepth 0 -type d | sort -V)
	for g in $devs; do
		echo "IOMMU Group ${g##*/}:" >> $1/iommu_groups.txt
		for d in $g/devices/*; do
			VALUE="$(/sbin/lspci -nns ${d##*/})"
			echo -e "\t$VALUE"
			printf "	%s\n" "$VALUE" >> $1/iommu_groups.txt
		done;
	done;
	##############################################################################

	uname -a > $1/uname_a.txt
	lsb_release -d > $1/lsb_release_d.txt
	lscpu > $1/lscpu.txt
	lsmem > $1/lsmem.txt
	lspci > $1/lspci.txt
	lspci -vvv > $1/lspci_vvv.txt
	lsblk > $1/lsblk.txt
	vmstat -s > $1/vmstat_s.txt

	top -b > $1/top.txt &
	PID3=$!
	sleep 2
	kill -s 2 $PID3
}

#
# all collecting methods should be defined in this array
#
funcArray=(
	host_info
	xbmgmt_output
	xbutil_output
	vmr_logs
	debug_logs
	core_dumps
)

#############################
# Real program starts here
#############################

if [ -z $DEVICE_BDF ];then
	echo "Please select one device"
	usage 0
else
	if [[ "$DEVICE_BDF" == *":"* ]] || [[ "$DEVICE_BDF" == *"."* ]];then
		echo "only support single bdf without : or . now"
		usage 0
	fi
	echo "card: $DEVICE_BDF"
fi

which xbmgmt > /dev/null
if [ $? -ne 0 ];then
	echo "XRT is not found, use default XRT"
	source /opt/xilinx/xrt/setup.sh > /dev/null
else
	echo "use configured XRT"
fi

if [ -z $PATH_TO_OUTPUT ];then
	PATH_TO_OUTPUT="."
fi

PATH_TO_BUNDLE=$PATH_TO_OUTPUT/xrt_bundle
echo "dump into: $PATH_TO_BUNDLE"

total=${#funcArray[@]}
index=0
for func in "${funcArray[@]}"
do
	progress $index $total $func

	if [ ! -d $PATH_TO_BUNDLE/$func ];then
		mkdir -p "$PATH_TO_BUNDLE/$func"
	fi

	$func "$PATH_TO_BUNDLE/$func"
	echo "."

	((index++))
done

echo "packaging data..."
FROMSIZE=`du -sk --apparent-size $PATH_TO_BUNDLE |cut -f 1`;
CHECKPOINT=`echo ${FROMSIZE}/50 |bc`
echo "Estimated: [==================================================]"
echo -n "Progress:  [";
tar --record-size=1K --checkpoint="${CHECKPOINT}" --checkpoint-action="ttyout=>" -czf $PATH_TO_OUTPUT/xrt_bundle.tar.gz --absolute-names $PATH_TO_BUNDLE
echo "]"
realpath $PATH_TO_OUTPUT/xrt_bundle.tar.gz
du -sh $PATH_TO_OUTPUT/xrt_bundle.tar.gz|cut -f1

if [ ! -z $VERBOSE ];then
	tree $PATH_TO_BUNDLE
fi
