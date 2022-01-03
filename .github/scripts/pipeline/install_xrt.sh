#!/bin/bash

RETURN_CODE=0
DISTRO=`grep '^ID=' /etc/os-release | awk -F= '{print $2}' | tr -d '"'`

cd build/Release
if [[ $DISTRO == "centos" ]]; then
    sudo yum install -y ./xrt_*xrt.rpm
else
    sudo apt install ./xrt_*xrt.deb
fi

# Install XRT step should fail when drivers dont get loaded
# Currently, it is shown as success eventhough driver loading fails
# As workaround, lsmod is performed to find if drivers are loaded
# If not loaded, this script throws error with Return code 1

DRIVER=`lsmod | grep -i xocl`
if [ -z "$DRIVER" ]; then
    echo "Drivers are NOT loaded"
    RETURN_CODE=1
else
    echo "Drivers are loaded"
fi

cat `find /var/lib/dkms/xrt/ -iname "make.log"`
exit $RETURN_CODE
