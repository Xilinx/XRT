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
  parser = argparse.ArgumentParser(formatter_class=RawDescriptionHelpFormatter, description='description:\n  Unit test wrapper for the SoftKernel section')
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

  step = "1) Read in a soft kernel and its metadata"

  inputJSON = os.path.join(args.resource_dir, "softkernel.rtd")
  inputKernel = os.path.join(args.resource_dir, "softkernel.so")
  softKernelName = "my_kernel"

  cmd = [xclbinutil, "--add-section", "SOFT_KERNEL[" + softKernelName + "]-OBJ:RAW:" + inputKernel, "--add-section", "SOFT_KERNEL[" + softKernelName + "]-METADATA:JSON:" + inputJSON]
  execCmd(step, cmd)

  # ---------------------------------------------------------------------------

  step = "2) Read in a soft kernel and its metadata where the symbol name is 1 character too long"

  inputJSON = os.path.join(args.resource_dir, "softkernel_longname.rtd")
  inputKernel = os.path.join(args.resource_dir, "softkernel.so")
  softKernelName = "my_kernel"

  cmd = [xclbinutil, "--add-section", "SOFT_KERNEL[" + softKernelName + "]-OBJ:RAW:" + inputKernel, "--add-section", "SOFT_KERNEL[" + softKernelName + "]-METADATA:JSON:" + inputJSON]

  try:
    print("Note: Testing for an excessive symbol name length (expecting an error)")
    execCmd(step, cmd)
  except:
    print("Test passed - Exception raised")
  else:
    raise Exception("Error: Long symbol name accepted.  DRC check did not trip.")


  # ---------------------------------------------------------------------------

  # If the code gets this far, all is good.
  return False

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

def testDivider():
  print("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~")


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

