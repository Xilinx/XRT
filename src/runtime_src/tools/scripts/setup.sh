#!/bin/sh

# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
# Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.
#
# Script to setup environment for XRT
# This script is installed in xrt install location, e.g. /opt/xilinx/xrt and must
# be sourced from that location

# Check OS version requirement
OSDIST=$(awk -F= '/^ID=/{print $2}' /etc/os-release)
if [ "$OSDIST" = "centos" ]; then
    OSREL=$(awk '{print $4}' /etc/redhat-release | tr -d '"' | awk -F. '{print $1*100+$2}')
else
    OSREL=$(awk -F= '/^VERSION_ID=/{print $2}' /etc/os-release | tr -d '"' | awk -F. '{print $1*100+$2}')
fi

if [ "$OSDIST" = "ubuntu" ] && [ "$OSREL" -lt 1604 ]; then
    echo "ERROR: Ubuntu release version must be 16.04 or later"
    return 1
fi

case "$OSDIST" in
    centos|rhel*)
        if [ "$OSREL" -lt 704 ]; then
            echo "ERROR: Centos or RHEL release version must be 7.4 or later"
            return 1
        fi
        ;;
esac

if [ -n "$BASH_VERSION" ]; then
    XILINX_XRT=$(readlink -f "$(dirname "${BASH_SOURCE[0]}")")
elif [ -n "$ZSH_VERSION" ]; then
    XILINX_XRT=$(readlink -f "$(dirname "${(%):-%N}")")
else
    if [ ! -f "./setup.sh" ]; then
       echo "ERROR: When sourcing in dash, please 'cd' to the script's directory first."
       return 1
    fi
    XILINX_XRT=$(dirname "$(readlink -f "$0")")
fi

# Poor test to ensure we are in an install location
if [ ! -f "$XILINX_XRT/version.json" ]; then
    echo "Invalid location: $XILINX_XRT"
    echo "This script must be sourced from XRT install directory"
    return 1
fi

COMP_FILE="/usr/share/bash-completion/bash_completion"
if [ -n "$BASH_VERSION" ] && [ -f "$COMP_FILE" ]; then
    # Enable autocompletion for the xbutil and xbmgmt commands
    . "$COMP_FILE"
    . "$XILINX_XRT/share/completions/xbutil-bash-completion"
    . "$XILINX_XRT/share/completions/xbmgmt-bash-completion"
else
    echo "Autocomplete not enabled for XRT tools"
fi

export XILINX_XRT
export LD_LIBRARY_PATH=$XILINX_XRT/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
export PATH=$XILINX_XRT/bin${PATH:+:$PATH}
export PYTHONPATH=$XILINX_XRT/python${PYTHONPATH:+:$PYTHONPATH}

echo "XILINX_XRT        : $XILINX_XRT"
echo "PATH              : $PATH"
echo "LD_LIBRARY_PATH   : $LD_LIBRARY_PATH"
echo "PYTHONPATH        : $PYTHONPATH"
