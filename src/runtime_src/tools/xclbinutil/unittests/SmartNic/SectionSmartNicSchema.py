import subprocess
import os
import argparse
from argparse import RawDescriptionHelpFormatter
import filecmp

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

  step = "1) Read in the JSON file, write out the CBOR image, and write out the JSON-to-CBOR-to-JSON file"

  inputJSON = os.path.join(args.resource_dir, "smartnic_all_syntax.json")
  outputJSON = "smartnic_all_syntax_output.json"
  outputCBOR = "smartnic_all_syntax_output.cbor"

  cmd = [xclbinutil, "--add-section", "SMARTNIC:JSON:"+inputJSON, "--dump-section", "SMARTNIC:RAW:"+outputCBOR, "--dump-section", "SMARTNIC:JSON:"+outputJSON, "--force"]
  execCmd(step, cmd)

  # Validate that the round trip files are identical
  fileCompare(inputJSON, outputJSON)

  # ---------------------------------------------------------------------------

  step = "2) Read in the byte files, merge them into the in memory CBOR image, write out the CBOR image, and write out the JSON-to-CBOR-to-JSON file"
  inputJSON = os.path.join(args.resource_dir, "smartnic_relative_bytefiles.json")
  outputJSON = "smartnic_relative_bytefiles_output.json"
  outputCBOR = "smartnic_relative_bytefiles_output.cbor"
  expectedJSON = os.path.join(args.resource_dir,"smartnic_relative_bytefiles_expected.json")
  
  cmd = [xclbinutil, "--add-section", "SMARTNIC:JSON:"+inputJSON, "--dump-section", "SMARTNIC:RAW:"+outputCBOR, "--dump-section", "SMARTNIC:JSON:"+outputJSON, "--force"]
  execCmd(step, cmd)

  # Validate that the output file matches expectation
  fileCompare(expectedJSON, outputJSON)

  # If the code gets this far, all is good.
  return False

def fileCompare(file1, file2):
  if not os.path.isfile(file1):
    raise Exception("Error: The following file does not exist: '" + file1 +"'")

  if not os.path.isfile(file2):
    raise Exception("Error: The following file does not exist: '" + file2 +"'")

  if not filecmp.cmp(file1, file2):
    print ("File1 : "+ file1)
    print ("File2 : "+ file2)
    raise Exception("Error: The two files are not identical")

def execCmd(pretty_name, cmd):
  print(pretty_name)
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

