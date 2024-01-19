#!/bin/bash

# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
# Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#
# Script to setup environment for XRT
# This script is installed in xrt install location, e.g. /opt/xilinx/xrt and must
# be sourced from that location

# Check OS version requirement
OSDIST=`cat /etc/os-release | grep -i "^ID=" | awk -F= '{print $2}'`
if [[ $OSDIST == "centos" ]]; then
    OSREL=`cat /etc/redhat-release | awk '{print $4}' | tr -d '"' | awk -F. '{print $1*100+$2}'`
else
    OSREL=`cat /etc/os-release | grep -i "^VERSION_ID=" | awk -F= '{print $2}' | tr -d '"' | awk -F. '{print $1*100+$2}'`
fi

if [[ $OSDIST == "ubuntu" ]]; then
    if (( $OSREL < 1604 )); then
        echo "ERROR: Ubuntu release version must be 16.04 or later"
        return 1
    fi
fi

if [[ $OSDIST == "centos" ]] || [[ $OSDIST == "rhel"* ]]; then
    if (( $OSREL < 704 )); then
        echo "ERROR: Centos or RHEL release version must be 7.4 or later"
        return 1
    fi
fi


if [ -n "$BASH_VERSION" ]; then
    XILINX_XRT=$(readlink -f $(dirname ${BASH_SOURCE[0]:-${(%):-%x}}))
elif [ -n "$ZSH_VERSION" ]; then
    XILINX_XRT=$(readlink -f $(dirname ${(%):-%N}))
else
    echo "ERROR: Unsupported shell. Only bash and zsh are supported"
    return 1
fi

if [[ $XILINX_XRT != *"/xrt" ]]; then
    echo "Invalid location: $XILINX_XRT"
    echo "This script must be sourced from XRT install directory"
    return 1
fi

COMP_FILE="/usr/share/bash-completion/bash_completion"
# 1. This is a workaround for set -e
# The issue is chaining conditionals with actual commands
# The issue is caused when sourcing the ${COMP_FILE}.
# Specifically ${COMP_FILE}::_sysvdirs. Each check in that function
# will fail the script due to the issues.
# If set -e is removed from the pipeline then check can be removed
# 2. Make sure that the shell is bash! The completion may not function
# correctly or setup on other shells.
# 3. Make sure the bash completion file exists
if [[ $- != *e* ]] && [[ "$BASH" == *"/bash" ]] && [ -f "${COMP_FILE}" ]; then
    # Enable autocompletion for the xbutil and xbmgmt commands
    source $COMP_FILE
    source $XILINX_XRT/share/completions/xbutil-bash-completion
    source $XILINX_XRT/share/completions/xbmgmt-bash-completion
else
    echo Autocomplete not enabled for XRT tools
fi

# To use the newest version of the XRT tools, either uncomment or set
# the following environment variable in your profile:
#   export XRT_TOOLS_NEXTGEN=true

export XILINX_XRT
export LD_LIBRARY_PATH=$XILINX_XRT/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
export PATH=$XILINX_XRT/bin${PATH:+:$PATH}
export PYTHONPATH=$XILINX_XRT/python${PYTHONPATH:+:$PYTHONPATH}

echo "XILINX_XRT        : $XILINX_XRT"
echo "PATH              : $PATH"
echo "LD_LIBRARY_PATH   : $LD_LIBRARY_PATH"
echo "PYTHONPATH        : $PYTHONPATH"
