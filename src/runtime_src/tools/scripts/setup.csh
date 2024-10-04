#!/bin/csh -f

# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#

set called=($_)
set script_path=""
set xrt_dir=""

# check if script is sourced or executed
if ("$called" != "") then
# sourced 
    set script=$called[2]
else
# executed
    set script=$0
endif

set script_rel_rootdir = `dirname $script`
set script_path = `cd $script_rel_rootdir && pwd`

set xrt_dir = $script_path

if ( $xrt_dir !~ */xrt ) then
    echo "Invalid location: $xrt_dir"
    echo "This script must be sourced from XRT install directory"
    exit 1
endif

set OSDIST=`cat /etc/os-release | grep -i "^ID=" | awk -F= '{print $2}'`
if ( "$OSDIST" =~ "centos" ) then
    set OSREL=`cat /etc/redhat-release | awk '{print $4}' | tr -d '"' | awk -F. '{print $1*100+$2}'`
else    
    set OSREL=`cat /etc/os-release | grep -i "^VERSION_ID=" | awk -F= '{print $2}' | tr -d '"' | awk -F. '{print $1*100+$2}'`
endif

if ( "$OSDIST" =~ "ubuntu" ) then
    if ( $OSREL < 1604 ) then
        echo "ERROR: Ubuntu release version must be 16.04 or later"
        exit 1
    endif
endif

if ( "$OSDIST" =~ centos  || "$OSDIST" =~ rhel* ) then
    if ( $OSREL < 704 ) then
        echo "ERROR: Centos or RHEL release version must be 7.4 or later"
        exit 1
    endif
endif

setenv XILINX_XRT $xrt_dir

if ( ! $?LD_LIBRARY_PATH ) then
   setenv LD_LIBRARY_PATH $XILINX_XRT/lib
else
   setenv LD_LIBRARY_PATH $XILINX_XRT/lib:$LD_LIBRARY_PATH
endif

if ( ! $?PATH ) then
   setenv PATH $XILINX_XRT/bin
else
   setenv PATH $XILINX_XRT/bin:$PATH
endif

if ( ! $?PYTHONPATH ) then
    setenv PYTHONPATH $XILINX_XRT/python
else
    setenv PYTHONPATH $XILINX_XRT/python:$PYTHONPATH
endif

# Enable autocompletion for the xbutil and xbmgmt commands
source $XILINX_XRT/share/completions/xbutil-csh-completion-wrapper
source $XILINX_XRT/share/completions/xbmgmt-csh-completion-wrapper

# To use the newest version of the XRT tools, either uncomment or set 
# the following environment variable in your profile:
#   setenv XRT_TOOLS_NEXTGEN true

echo "XILINX_XRT        : $XILINX_XRT"
echo "PATH              : $PATH"
echo "LD_LIBRARY_PATH   : $LD_LIBRARY_PATH"
echo "PYTHONPATH        : $PYTHONPATH"
