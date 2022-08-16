# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Xilinx, Inc
# Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#!/bin/bash

# Source setup and ignore output
source $XILINX_XRT/setup.sh > /dev/null
# Generate the output JSON file. Ignore both error and standard output
xbutil examine $@ -f JSON-plain -o /tmp/nagios_output.json &> /dev/null
# Depending on command status return OK or FAILURE
case $? in
  0)
    echo "STATUS: OK |"
    ;;
  *)
    echo "STATUS: FAILURE |"
    ;;
esac
# Output the generated JSON
cat /tmp/nagios_output.json
# Cleanup all extra files
rm /tmp/nagios_output.json
