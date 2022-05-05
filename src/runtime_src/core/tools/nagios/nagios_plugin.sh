#!/bin/bash

source $XILINX_XRT/setup.sh > /dev/null
$XILINX_XRT/etc/nagios-plugins/xrt_nagios_plugin $@
