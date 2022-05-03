import json
import os
import re
import sys

temp_python_status = sys.argv[1]
temp_json_file = sys.argv[2]
device_filter = sys.argv[3]
xrt_app = sys.argv[4]
prog_args = sys.argv[5]

# Read in the JSON file produced earlier
with open(temp_json_file) as f:
  data = json.load(f)

# Filter on the devices of interest
working_devices = []
devices = data["system"]["host"]["devices"]

regex_string="."
if len(device_filter) != 0:
    regex_string=device_filter

try:
  pattern=re.compile(regex_string) 
except:    # catch all exceptions
  print("Error: Malformed device filter: '%s'" % regex_string)
  exit(1)

for device in devices:
  shell_vbnv = device["vbnv"]

  if len(device_filter) != 0:
    if pattern.search(shell_vbnv):
        print("Match: %s" % shell_vbnv);
    else:
        print("Skip: %s" % shell_vbnv);
        continue

  working_devices.append(device)
  
 
# Perform the operation on the filtered working devices 
failed_devices=0
passed_devices=0
device_count = 1
for device in working_devices: 
  
  # Invoke XRT application for this given device
  print("\n")
  print("=====================================================================")
  print("%d / %d [%s] : %s" % (device_count, len(working_devices), device["bdf"], device["vbnv"]))
  cmd = os.environ['XILINX_XRT'] + "/bin/" + xrt_app + " --device " + device["bdf"] + " " + prog_args
  print("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~")
  print("Command: %s\n" % cmd)
  exit_code = os.system(cmd)

  if exit_code == 0:
    print("\nCommand Return Value: 0 [Operation successful]");
    passed_devices += 1
  else:
    print("\nCommand Return Value: %d [Error(s) occured]" % exit_code)
    failed_devices += 1

  device_count += 1

# Summary of all of the operations
print("\n")
print("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~")
print("Summary:")
print("   Installed device(s) : %d" % len(devices))
print("          Shell Filter : '%s'" % regex_string)
print("      Number Evaluated : %d" % len(working_devices))
print("                Passed : %d" % passed_devices)
print("                Failed : %d" % failed_devices)
print("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~")

# Operation complete
with open(temp_python_status, "w") as f:
    if failed_devices == 0:
        print("PASSED", file=f)
    else:
        print("FAILED", file=f)
