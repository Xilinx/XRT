#!/bin/csh -f

# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#

#set called=($_)
set script_path=""
set xrt_dir=""

# revisit if there is a better way than lsof to obtain the script path
# in non-interactive mode.  If lsof is needed, then revisit why
# why sbin need to be prepended looks like some environment issue in
# user shell, e.g. /usr/local/bin/mis_env: No such file or directory.
# is because user path contain bad directories that are searched when
# looking of lsof.
set path=(/usr/sbin $path)
set called=(`\lsof +p $$ |\grep setup.csh`)

# look for the right cmd component that contains setup.csh
foreach x ($called)
    if ( "$x" =~ *setup.csh ) then
        set script_path=`readlink -f $x`
        set xrt_dir=`dirname $script_path`
    endif
    if ( $xrt_dir =~ */opt/xilinx/xrt ) break
end

if ( $xrt_dir !~ */opt/xilinx/xrt ) then
    echo "Invalid location: $xrt_dir"
    echo "This script must be sourced from XRT install directory"
    exit 1
endif

set OSDIST=`cat /etc/os-release | grep -i "^ID=" | awk -F= '{print $2}'`
if [[ $OSDIST == "centos" ]]; then
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
