#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Xilinx, Inc
# Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

# Generate the output JSON file. Ignore both error and standard output
/opt/xilinx/xrt/bin/xbutil examine $@ -f JSON-plain -o /tmp/nagios_output.json &> /dev/null
# Depending on command status return OK or FAILURE
EXIT_CODE=0
case $? in
  0)
    echo "STATUS: OK |"
    EXIT_CODE=0
    ;;
  *)
    echo "STATUS: FAILURE |"
    EXIT_CODE=1
    ;;
esac
# Output the generated JSON
cat /tmp/nagios_output.json
# Cleanup all extra files
rm /tmp/nagios_output.json
exit $EXIT_CODE
