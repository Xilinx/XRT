#!/bin/bash
# This script program plp based on input xclbin
# This will be obsoleted after plp auto download is fully impletmented

usage() {
    echo "Program PLP"
    echo
    echo "-i <PATH>	Full path to xclbin file"
    echo "[-help]		List this help"
    exit $1
}

while [ $# -gt 0 ]; do
	case "$1" in
		-help)
			usage 0
			;;
		-i)
			shift
			PATH_TO_XCLBIN=$1
			shift
			;;
		*)
			echo "$1 invalid argument."
			usage 1
			;;
	esac
done

if [ "foo${PATH_TO_XCLBIN}" == "foo" ] ; then
	echo "full path to xclbin is missing!"
	usage 1
fi

INTERFACE_UUID=`xclbinutil -i $PATH_TO_XCLBIN --dump-section PARTITION_METADATA:JSON:/tmp/dt.json --force >/dev/null && cat /tmp/dt.json | grep interface_ | awk -F: '{print $2}' | awk -F\" '{print $2}'`

if [ "foo${INTERFACE_UUID}" == "foo" ] ; then
	echo "failed to get interface uuid by xclbinutil"
	exit 1
fi

# workaround mailbox
RP_DEVICE=`xbutil scan | grep user | grep -v xilinx | sed 's/.*\[//' | sed 's/].*//'`
if [ "foo${RP_DEVICE}" == "foo" ] ; then
	echo "No board!"
	exit 1;
fi

echo 1 >/sys/bus/pci/devices/$RP_DEVICE/remove
sleep 5
if [ -f "/sys/bus/pci/devices/$RP_DEVICE" ]; then
	echo "Shutdown userpf failed!"
	exit 1;
fi

echo "xbmgmt partition --program --id $INTERFACE_UUID --force"
xbmgmt partition --program --id $INTERFACE_UUID --force

echo 1 >/sys/bus/pci/rescan
