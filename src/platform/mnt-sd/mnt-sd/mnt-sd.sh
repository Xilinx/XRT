#!/bin/sh

mount /dev/mmcblk0p1 /mnt || mount /dev/mmcblk0 /mnt

if [ -x /mnt/init.sh ]
then
	echo "attempting to run /mnt/init.sh"
	source /mnt/init.sh
fi

