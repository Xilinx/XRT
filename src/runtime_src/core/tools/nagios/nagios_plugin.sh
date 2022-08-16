#!/bin/bash

source $XILINX_XRT/setup.sh > /dev/null
xbutil examine -d $1 -r $2 -f JSON-internal -o /tmp/nagios_output.json &> /dev/null
case $? in
  0)
    echo "STATUS: OK |"
    ;;
  *)
    echo "STATUS: FAILURE |"
    ;;
esac
cat /tmp/nagios_output.json
rm /tmp/nagios_output.json