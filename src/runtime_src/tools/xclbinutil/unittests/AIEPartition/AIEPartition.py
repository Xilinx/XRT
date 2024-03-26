from argparse import RawDescriptionHelpFormatter
import argparse
import binascii
import filecmp
import json
import os
import subprocess
import shutil

# Start of our unit test
# -- main() -------------------------------------------------------------------
#
# The entry point to this script.
#
# Note: It is called at the end of this script so that the other functions
#       and classes have been defined and the syntax validated
def main():
  # -- Configure the argument parser
  parser = argparse.ArgumentParser(formatter_class=RawDescriptionHelpFormatter, description='description:\n  Unit test wrapper for the adding AIE Partitions')
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

  step = "1) Add the AIE parition to the xclbin image"

  sectionname = "Flavor"
  workingXclbin = "aiePartition.xclbin"
  aiePartitionOutput1 = "aie_partition_output1.json"
  aiePartition = os.path.join(args.resource_dir, "aie_partition.json")
  aiePartitionOutputExpected = os.path.join(args.resource_dir, "aie_partition_expected.json")

  cmd = [xclbinutil, "--add-section", "AIE_PARTITION["+sectionname+"]:JSON:" + aiePartition,
                     "--dump-section", "AIE_PARTITION["+sectionname+"]:JSON:" + aiePartitionOutput1,
                     "--output", workingXclbin,
                     "--force",
                     "--trace"
                     ]
  execCmd(step, cmd)
  jsonFileCompare(aiePartitionOutputExpected, aiePartitionOutput1)

  # ---------------------------------------------------------------------------

  step = "2) Read and dump the AIE parition"
  aiePartitionOutput2 = "aie_partition_output2.json"

  cmd = [xclbinutil, "--input", workingXclbin,
                     "--dump-section", "AIE_PARTITION["+sectionname+"]:JSON:" + aiePartitionOutput2,
                     "--force",
                     "--trace"
                     ]
  execCmd(step, cmd)
  jsonFileCompare(aiePartitionOutputExpected, aiePartitionOutput2)

  # 1a) Check for the existance of the dummy PDI images
  aiePartition1110PDIExpected = os.path.join(args.resource_dir, "1110.txt")
  aiePartition1110PDIOutput = "00000000-0000-0000-0000-000000001110.pdi"
  textFileCompare(aiePartition1110PDIExpected, aiePartition1110PDIOutput)

  # 1b) Check for the existance of the dummy PDI images
  aiePartition1111PDIExpected = os.path.join(args.resource_dir, "1111.txt")
  aiePartition1111PDIOutput = "00000000-0000-0000-0000-000000001111.pdi"
  textFileCompare(aiePartition1111PDIExpected, aiePartition1111PDIOutput)

  '''
  # XRT doesn't allow checking in binary files, so temporarily comment out 
  # the following tests. Maybe we can figure out something in the future to 
  # re-enable them
  # ---------------------------------------------------------------------------

  step = "3) Add the AIE parition to the xclbin image with pdi transform enabled"
  # copy aie_partition_trans.json currently the code expect the find it in CWD
  print ("cwd : "+ os.getcwd())
  aiePartitionTran = os.path.join(args.resource_dir, "aie_partition_trans.json")
  shutil.copy(aiePartitionTran, os.getcwd())

  PdiHex2220 = os.path.join(args.resource_dir, "2220.hex")
  Pdi2220 = "00000000-0000-0000-0000-000000002220.pdi"

  # Read in the hex array
  with open(PdiHex2220) as file:
      hexImage = file.read();

  binImage = bytes.fromhex(hexImage[ : ])

  with open(Pdi2220, 'wb') as file:
      file.write(binImage)

  PdiHex2221 = os.path.join(args.resource_dir, "2221.hex")
  Pdi2221 = "00000000-0000-0000-0000-000000002221.pdi"

  # Read in the hex array
  with open(PdiHex2221) as file:
      hexImage = file.read();

  binImage = bytes.fromhex(hexImage[ : ])

  with open(Pdi2221, 'wb') as file:
      file.write(binImage)

  sectionname = "Trans"
  workingXclbin = "aiePartitionTrans.xclbin"
  # aiePartition = os.path.join(args.resource_dir, "aie_partition_trans.json")
  aiePartition = os.path.join(os.getcwd(), "aie_partition_trans.json")

  cmd = [xclbinutil, "--add-section", "AIE_PARTITION["+sectionname+"]:JSON:" + aiePartition,
                     "--transform-pdi",
                     "--output", workingXclbin,
                     "--trace",
                     "--force"
                     ]
  execCmd(step, cmd)

  # ---------------------------------------------------------------------------

  step = "4) Read and dump the AIE parition"
  aiePartitionOutput = "transform/aie_partition_output.json"
  if not os.path.exists("./transform"):
    os.makedirs("./transform")

  PdiHexExpected2220 = os.path.join(args.resource_dir, "2220_expected.hex")
  aiePartition2220PDIExpected = "2220_expected.pdi"

  # Read in the hex array
  with open(PdiHexExpected2220) as file:
      hexImage = file.read();

  binImage = bytes.fromhex(hexImage[ : ])

  with open(aiePartition2220PDIExpected, 'wb') as file:
      file.write(binImage)

  PdiHexExpected2221 = os.path.join(args.resource_dir, "2221_expected.hex")
  aiePartition2221PDIExpected = "2221_expected.pdi"

  # Read in the hex array
  with open(PdiHexExpected2221) as file:
      hexImage = file.read();

  binImage = bytes.fromhex(hexImage[ : ])

  with open(aiePartition2221PDIExpected, 'wb') as file:
      file.write(binImage)

  cmd = [xclbinutil, "--input", workingXclbin,
                     "--dump-section", "AIE_PARTITION["+sectionname+"]:JSON:" + aiePartitionOutput,
                     "--force"
                     ]
  execCmd(step, cmd)
  # jsonFileCompare(aiePartitionOutputExpected, aiePartitionOutput2)

  # 1a) Check for the existance of the dumped PDI images
  # aiePartition2220PDIExpected = os.path.join(args.resource_dir, "2220_expected.pdi")
  aiePartition2220PDIOutput = "transform/00000000-0000-0000-0000-000000002220.pdi"
  binaryFileCompare(aiePartition2220PDIExpected, aiePartition2220PDIOutput)

  # 1b) Check for the existance of the dummy PDI images
  # aiePartition2221PDIExpected = os.path.join(args.resource_dir, "2221_expected.pdi")
  aiePartition2221PDIOutput = "transform/00000000-0000-0000-0000-000000002221.pdi"
  binaryFileCompare(aiePartition2221PDIExpected, aiePartition2221PDIOutput)

  # ---------------------------------------------------------------------------
  '''

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

      raise Exception("Error: The given files are not the same")

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

def binaryFileCompare(file1, file2):
    if not os.path.isfile(file1):
      raise Exception("Error: The following json file does not exist: '" + file1 +"'")

    if not os.path.isfile(file2):
      raise Exception("Error: The following json file does not exist: '" + file2 +"'")

    if filecmp.cmp(file1, file2) == False:
        print ("\nFile1 : "+ file1)
        print ("\nFile2 : "+ file2)

        raise Exception("Error: The two files are not binary the same")

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

