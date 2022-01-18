#!/bin/bash

# After installing xrt, platform packages of xilinx cards need to be installed
# For u250 card, it needs to be programmed with partition.xsabin before running validate tests
# In order to distinguish u250 card from other cards, device id is being used. Device id for u250 is 5005

DEVID1=`lspci -d 10ee: | awk '{print $7}' | awk ' FNR == 1'`
DEVID2=`lspci -d 10ee: | awk '{print $7}' | awk ' FNR == 2'`

DISTRO=`grep '^ID=' /etc/os-release | awk -F= '{print $2}' | tr -d '"'`

# The required platform packages will be copied to home directory in setup.sh script which is included in the snapshot itself
cd ~/u*

# check for u250 card
if [ $DEVID1 != "5005" ] && [ $DEVID2 != "5005" ]; then
    if [[ $DISTRO == "centos" ]]; then
        sudo yum install -y ./xilinx*.rpm
    else
        sudo apt install ./xilinx*.deb
    fi
else
    sudo apt install ./xilinx-cmc*.deb
    sudo apt install ./xilinx-sc-fw*.deb
    sudo apt install ./xilinx*base*.deb
    sudo /opt/xilinx/xrt/bin/xbmgmt program --base -d
    sudo apt install ./xilinx*validate*.deb
    sudo apt install ./xilinx*shell*.deb
    sudo /opt/xilinx/xrt/bin/xbmgmt program --shell /opt/xilinx/firmware/u250/gen3x16/xdma-shell/partition.xsabin -d
    sudo /opt/xilinx/xrt/bin/xbmgmt examine -d
fi
