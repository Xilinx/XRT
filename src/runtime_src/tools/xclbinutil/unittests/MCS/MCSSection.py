# Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
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
  parser = argparse.ArgumentParser(formatter_class=RawDescriptionHelpFormatter, description='description:\n  Unit test wrapper use to validated MCS section')
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

  step = "1) Create working xclbin container with a MCS section"

  inputMCSPrimaryImage = os.path.join(args.resource_dir, "sample_data1.txt")
  inputMCSSecondaryImage = os.path.join(args.resource_dir, "sample_data2.txt")
  workingXCLBIN = "working.xclbin"

  cmd = [xclbinutil, "--add-section", "MCS-PRIMARY:RAW:" + inputMCSPrimaryImage, 
                     "--add-section", "MCS-SECONDARY:RAW:" + inputMCSSecondaryImage, 
                     "--output", workingXCLBIN, 
                     "--force"
                     ]
  execCmd(step, cmd)


  # ---------------------------------------------------------------------------

  step = "2) Read in the working xclbin image and validate the MCS section"
  outputMCSPrimaryImage = "output_primary_image.txt";
  outputMCSSecondaryImage = "output_secondary_image.txt";

  cmd = [xclbinutil, "--input", workingXCLBIN,
                     "--dump-section", "MCS-PRIMARY:RAW:" + outputMCSPrimaryImage, 
                     "--dump-section", "MCS-SECONDARY:RAW:" + outputMCSSecondaryImage, 
                     "--force"
                     ]

  execCmd(step, cmd)

  # Validate the contents of the various sections
  textFileCompare(inputMCSPrimaryImage, outputMCSPrimaryImage)
  textFileCompare(inputMCSSecondaryImage, outputMCSSecondaryImage)

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

