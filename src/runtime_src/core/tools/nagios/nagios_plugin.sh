#!/bin/bash

source /opt/xilinx/xrt/setup.sh > /dev/null
/opt/xilinx/xrt/etc/nagios-plugins/xrt_nagios_plugin $1
