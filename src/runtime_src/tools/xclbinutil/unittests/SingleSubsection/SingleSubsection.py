from argparse import RawDescriptionHelpFormatter
import argparse
import binascii
import filecmp
import json
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
  parser = argparse.ArgumentParser(formatter_class=RawDescriptionHelpFormatter, description='description:\n  Unit test wrapper use to validated single subsection sections')
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

  step = "1) Create working xclbin container with multiple single vender sections"

  inputVenderMetadata1 = os.path.join(args.resource_dir, "sample_data1.txt")
  inputVenderMetadata1Name = "ACME";
  inputVenderMetadata2 = os.path.join(args.resource_dir, "sample_data2.txt")
  inputVenderMetadata2Name = "Xilinx";
  workingXCLBIN = "working.xclbin"

  cmd = [xclbinutil, "--add-section", "VENDER_METADATA[" + inputVenderMetadata1Name + "]:RAW:" + inputVenderMetadata1, 
                     "--add-section", "VENDER_METADATA[" + inputVenderMetadata2Name + "]:RAW:" + inputVenderMetadata2, 
                     "--output", workingXCLBIN, 
                     "--force" 
                     ]
  execCmd(step, cmd)


  # ---------------------------------------------------------------------------

  step = "2) Read in a PS kernel, updated and validate the sections"
  outputVenderMetadata1 = "output_sample_data1.txt";
  outputVenderMetadata2 = "output_sample_data2.txt";

  cmd = [xclbinutil, "--input", workingXCLBIN,
                     "--dump-section", "VENDER_METADATA[" + inputVenderMetadata1Name + "]:RAW:" + outputVenderMetadata1, 
                     "--dump-section", "VENDER_METADATA[" + inputVenderMetadata2Name + "]:RAW:" + outputVenderMetadata2, 
                     "--force"
                     ]
  execCmd(step, cmd)

  # Validate the contents of the various sections
  textFileCompare(inputVenderMetadata1, outputVenderMetadata1)
  textFileCompare(inputVenderMetadata2, outputVenderMetadata2)

  # ---------------------------------------------------------------------------

  # If the code gets this far, all is good.
  return False

  # ---- Helper procedures ----------------------------------------------------


def textFileCompare(file1, file2):
    if not os.path.isfile(file1):
      raise Exception("Error: The following file does not exist: '" + file1 +"'")

    with open(file1) as f:
      data1 = f.read()

    if not os.path.isfile(file2):
      raise Exception("Error: The following file does not exist: '" + file2 +"'")

    with open(file2) as f:
      data2 = f.read()

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

        raise Exception("Error: The given files are not the same")


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

