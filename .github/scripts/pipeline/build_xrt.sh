#!/bin/bash

DISTRO=$(grep '^ID=' /etc/os-release | awk -F= '{print $2}' | tr -d '"')

sudo ./src/runtime_src/tools/scripts/xrtdeps.sh 

if [[ $DISTRO == "centos" ]]; then
        source /opt/rh/devtoolset-*/enable
fi

export XILINX_VITIS=/mnt/packages/vitis
export PATH=/mnt/packages/vitis/gnu/microblaze/lin/bin:/mnt/packages/vitis/gnu/microblaze/linux_toolchain/lin64_le/bin:$PATH

./build/build.sh -opt
