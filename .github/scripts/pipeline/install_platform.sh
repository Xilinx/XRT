#!/bin/bash

DEVID1=$(lspci -d 10ee: | awk '{print $7}' | awk ' FNR == 1')

DEVID2=$(lspci -d 10ee: | awk '{print $7}' | awk ' FNR == 2')

DISTRO=$(grep '^ID=' /etc/os-release | awk -F= '{print $2}' | tr -d '"')

cd ~/u*
if [ $DEVID1 != "5005" ] && [ $DEVID2 != "5005" ]; then	#for u250 card, shell needs to be updated
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
fi
