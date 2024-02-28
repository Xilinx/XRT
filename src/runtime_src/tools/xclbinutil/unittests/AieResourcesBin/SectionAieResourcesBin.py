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
  parser = argparse.ArgumentParser(formatter_class=RawDescriptionHelpFormatter, description='description:\n  Unit test wrapper for the AIE RESOURCES BIN section')
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

  step = "1) Read in two AIE RESOURCES BIN sections and its metadata"

  firstInputJSON = os.path.join(args.resource_dir, "graph1.rtd")
  secondInputJSON = os.path.join(args.resource_dir, "graph2.rtd")
  inputAieResourcesBin = os.path.join(args.resource_dir, "dummyAieResourcesBin.txt")
  firstName = "graph1"
  secondName = "graph2"

  cmd = [xclbinutil, "--add-section", "AIE_RESOURCES_BIN[" + firstName + "]-OBJ:RAW:" + inputAieResourcesBin, "--add-section", "AIE_RESOURCES_BIN[" + firstName + "]-METADATA:JSON:" + firstInputJSON, "--add-section", "AIE_RESOURCES_BIN[" + secondName + "]-OBJ:RAW:" + inputAieResourcesBin, "--add-section", "AIE_RESOURCES_BIN[" + secondName + "]-METADATA:JSON:" + secondInputJSON]
  execCmd(step, cmd)

  # ---------------------------------------------------------------------------

  # If the code gets this far, all is good.
  return False


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

