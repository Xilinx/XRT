# Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
from argparse import RawDescriptionHelpFormatter
import argparse
import os
import subprocess

# Start of our unit test
# -- main() -------------------------------------------------------------------
#
# The entry point to this script.
#
# Note: It is called at the end of this script so that the other functions
#       and classes have been defined and the syntax validated
def main():
  # -- Configure the argument parser
  parser = argparse.ArgumentParser(formatter_class=RawDescriptionHelpFormatter, description='description:\n  Unit test wrapper for the AIE TRACE METADATA section')
  parser.add_argument('--resource-dir', nargs='?', default=".", help='directory containing data to be used by this unit test')
  args = parser.parse_args()

  # Validate that the resource directory is valid
  if not os.path.exists(args.resource_dir):
      raise Exception("Error: The resource-dir '" + args.resource_dir +"' does not exist")

  if not os.path.isdir(args.resource_dir):
      raise Exception("Error: The resource-dir '" + args.resource_dir +"' is not a directory")

  # Prepare for testing
  xclbinutil = "xclbinutil"

  # Start the tests
  print ("Starting test")

  # ---------------------------------------------------------------------------

  step = "1) Create a valid xclbin that has platformVBNV information and at least one section"

  inputJSON = os.path.join(args.resource_dir, "debug_ip_layout.rtd")
  outputXCLBIN = "valid.xclbin"

  cmd = [xclbinutil, "--add-section", "DEBUG_IP_LAYOUT:JSON:" + inputJSON, "--key-value", "SYS:PlatformVBNV:xilinx.com_xd_xilinx_vck190_base_202410_1_202410_1", "--output", outputXCLBIN, "--force"]
  execCmd(step, cmd)

  
  # ---------------------------------------------------------------------------

  step = "2) Run file-check option on the valid xclbin"

  cmd = [xclbinutil, "--file-check", "-i", outputXCLBIN] 
  execCmd(step, cmd)

  # ---------------------------------------------------------------------------

  step = "3) Create an invalid xclbin that has only platformVBNV information"

  outputXCLBIN = "invalid.xclbin"

  cmd = [xclbinutil, "--key-value", "SYS:PlatformVBNV:xilinx.com_xd_xilinx_vck190_base_202410_1_202410_1", "--output", outputXCLBIN, "--force"]
  execCmd(step, cmd)

  # ---------------------------------------------------------------------------

  step = "4) Run file-check option on the invalid xclbin and validate the error message"

  expectedMsg = "ERROR: The xclbin is missing at least one section required by the 'file' command to identify its file type and display file characteristics.\n"

  cmd = [xclbinutil, "--file-check", "-i", outputXCLBIN]
  compErrorMsg(step, cmd, expectedMsg)

  # ----------------------------------------------------------------------------

  step = "5) Create an invalid xclbin with one section and no platformVBNV information"

  outputXCLBIN = "invalid.xclbin"

  cmd = [xclbinutil, "--add-section", "DEBUG_IP_LAYOUT:JSON:" + inputJSON, "--output", outputXCLBIN, "--force"]
  execCmd(step, cmd)

  # ----------------------------------------------------------------------------

  step = "6) Run file-check on the invalid xclbin and validate the error message"

  expectedMsg = "ERROR: The xclbin is missing platformVBNV information required by the 'file' command to identify its file type and display file characteristics.\n"

  cmd = [xclbinutil, "--file-check", "-i", outputXCLBIN]
  compErrorMsg(step, cmd, expectedMsg)

  # ----------------------------------------------------------------------------
  # If the code gets this far, all is good.
  return False


def testDivider():
  print("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~")


def compErrorMsg(pretty_name, cmd, expectedMsg):
  testDivider()
  print(pretty_name)
  testDivider()
  cmdLine = ' '.join(cmd)
  print(cmdLine)
  proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  o, e = proc.communicate()
  print(o.decode('ascii'))
  print(e.decode('ascii'))
  if(e.decode('ascii') != expectedMsg):
      raise Exception("The error message for file-check on invalid xclbin does not match " + str(expectedMsg))

def execCmd(pretty_name, cmd):
  testDivider()
  print(pretty_name)
  testDivider()
  cmdLine = ' '.join(cmd)
  print(cmdLine)
  proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  o, e = proc.communicate()
  print(o.decode('ascii'))
  print(e.decode('ascii'))
  errorCode = proc.returncode

  if errorCode != 0:
    raise Exception("Operation failed with the return code: " + str(errorCode))

# -- Start executing the script functions
if __name__ == '__main__':
  try:
    if main() == True:
      print ("\nError(s) occurred.")
      print("Test Status: FAILED")
      exit(1)
  except Exception as error:
    print(repr(error))
    print("Test Status: FAILED")
    exit(1)


# If the code get this far then no errors occured
print("Test Status: PASSED")
exit(0)

