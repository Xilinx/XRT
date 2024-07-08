#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022 Xilinx, Inc
# Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

""":" # Hide bash from python
# Generate the output JSON file. Ignore both error and standard output
temp_json_file=$(mktemp -u --suffix=.nagios.json)
/opt/xilinx/xrt/bin/xrt-smi examine $@ -o $temp_json_file &> /dev/null
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
python $0 "${temp_json_file}"
# Cleanup all extra files
rm $temp_json_file
exit $EXIT_CODE
""" # Hide bash from python
# Python script starts here
import json
import sys

temp_json_file = sys.argv[1]

# Read in the JSON file produced earlier
with open(temp_json_file) as f:
  data = json.load(f)

print(json.dumps(data))
