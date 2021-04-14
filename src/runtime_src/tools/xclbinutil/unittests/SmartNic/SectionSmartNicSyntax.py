import subprocess
import os
import argparse
from argparse import RawDescriptionHelpFormatter
import filecmp
import json

# Start of our unit test
# -- main() -------------------------------------------------------------------
#
# The entry point to this script.
#
# Note: It is called at the end of this script so that the other functions
#       and classes have been defined and the syntax validated
def main():
  # -- Configure the argument parser
  parser = argparse.ArgumentParser(formatter_class=RawDescriptionHelpFormatter, description='description:\n  Unit test wrapper for the SmartNic section')
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
  step = "1) Read in a validate JSON file and validate it against the schema"

  inputJSON = os.path.join(args.resource_dir, "smartnic_full_validate_syntax.json")

  cmd = [xclbinutil, "--add-section", "SMARTNIC:JSON:" + inputJSON]
  execCmd(step, cmd)

  # ---------------------------------------------------------------------------
  step = "2) Read in a byte file and validate it was transformed correctly"

  inputJSON = os.path.join(args.resource_dir, "simple_bytefiles.json")
  outputJSON = "simple_bytefiles_output.json"
  expectedJSON = os.path.join(args.resource_dir,"simple_bytefiles_expected.json")


  cmd = [xclbinutil, "--add-section", "SMARTNIC:JSON:" + inputJSON, "--dump-section", "SMARTNIC:JSON:" + outputJSON, "--force"]
  execCmd(step, cmd)

  # Validate that the output file matches expectation
  jsonFileCompare(expectedJSON, outputJSON)

  # ---------------------------------------------------------------------------
  step = "3a) Merging Validation : Adding v++ linker contents to the xclbin"

  vppLinkerJSON = os.path.join(args.resource_dir, "vpp_linker.json")
  vppLinkerXclbin = "vpp_linker.xclbin"

  cmd = [xclbinutil, "--add-merge-section", "SMARTNIC:JSON:" + vppLinkerJSON, "--output", vppLinkerXclbin, "--force"]
  execCmd(step, cmd)

  # ...........................................................................
  step = "3b) Merging Validation : Merging Extension Metadata"

  extensionJSON = os.path.join(args.resource_dir, "extension.json")
  vppPackerXclbin1 = "vpp_packager1.xclbin"

  cmd = [xclbinutil, "--input", vppLinkerXclbin, "--add-merge-section", "SMARTNIC:JSON:" + extensionJSON, "--output", vppPackerXclbin1, "--force"]
  execCmd(step, cmd)

  # ...........................................................................
  step = "3c) Merging Validation : Merging Softhub metadata"

  softhubJSON = os.path.join(args.resource_dir, "softhub.json")
  vppPackerXclbin2 = "vpp_packager2.xclbin"

  cmd = [xclbinutil, "--input", vppPackerXclbin1, "--add-merge-section", "SMARTNIC:JSON:" + softhubJSON, "--output", vppPackerXclbin2, "--force"]
  execCmd(step, cmd)

  # ...........................................................................
  step = "3d) Merging Validation : Merging eBPF"

  ebpfJSON = os.path.join(args.resource_dir, "eBPF.json")
  expectedJSON = os.path.join(args.resource_dir,"vitis_merged_expected.json")
  vppPackerXclbin3 = "vpp_packager3.xclbin"
  outputJSON = "vitis_merged_output.json"

  cmd = [xclbinutil, "--input", vppPackerXclbin2, "--add-merge-section", "SMARTNIC:JSON:" + ebpfJSON, "--dump-section", "SMARTNIC:JSON:" + outputJSON, "--output", vppPackerXclbin3, "--force"]
  execCmd(step, cmd)


  # Validate that the output file matches expectation
  jsonFileCompare(expectedJSON, outputJSON)

  # ---------------------------------------------------------------------------

  # If the code gets this far, all is good.
  return False

def testDivider():
  print("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~")
  

def jsonFileCompare(file1, file2):
  if not os.path.isfile(file1):
    raise Exception("Error: The following json file does not exist: '" + file1 +"'")

  with open(file1) as f:
    data1 = json.dumps(json.load(f), indent=2)

  if not os.path.isfile(file2):
    raise Exception("Error: The following json file does not exist: '" + file2 +"'")

  with open(file2) as f:
    data2 = json.dumps(json.load(f), indent=2)

  if data1 != data2:
      # Print out the contents of file 1
      print ("\nFile1 : "+ file1)
      print ("vvvvv")
      print (data1)
      print ("^^^^^")

      # Print out the contents of file 1
      print ("\nFile2 : "+ file2)
      print ("vvvvv")
      print (data2)
      print ("^^^^^")

      raise Exception("Error: The two files are not the same")

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

