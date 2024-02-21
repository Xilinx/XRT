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
  aiePartition1111PDIExpected = os.path.join(args.resource_dir, "1111.txt")
  aiePartition1111PDIOutput = "00000000-0000-0000-0000-000000001111.pdi"
  textFileCompare(aiePartition1111PDIExpected, aiePartition1111PDIOutput)

  # 1b) Check for the existance of the dummy PDI images
  aiePartition2222PDIExpected = os.path.join(args.resource_dir, "2222.txt")
  aiePartition2222PDIOutput = "00000000-0000-0000-0000-000000002222.pdi"
  textFileCompare(aiePartition2222PDIExpected, aiePartition2222PDIOutput)

'''
# XRT doesn't allow checking in binary files, so temporarily comment out 
# the following tests. Maybe we can figure out something in the future to 
# re-enable them
  # ---------------------------------------------------------------------------

  step = "3) Add the AIE parition to the xclbin image with pdi transform enabled"
  # copy transform_static, currently the code expect the find it in CWD
  print ("cwd : "+ os.getcwd())
  transformTool = os.path.join(args.resource_dir, "transform_static")
  shutil.copy(transformTool, os.getcwd())

  sectionname = "Trans"
  workingXclbin = "aiePartitionTrans.xclbin"
  aiePartition = os.path.join(args.resource_dir, "aie_partition_trans.json")

  cmd = [xclbinutil, "--add-section", "AIE_PARTITION["+sectionname+"]:JSON:" + aiePartition,
                     "--transform-pdi",
                     "--output", workingXclbin,
                     "--force"
                     ]
  execCmd(step, cmd)

  # ---------------------------------------------------------------------------

  step = "4) Read and dump the AIE parition"
  aiePartitionOutput = "transform/aie_partition_output.json"
  if not os.path.exists("./transform"):
    os.makedirs("./transform")

  cmd = [xclbinutil, "--input", workingXclbin,
                     "--dump-section", "AIE_PARTITION["+sectionname+"]:JSON:" + aiePartitionOutput,
                     "--force"
                     ]
  execCmd(step, cmd)
  # jsonFileCompare(aiePartitionOutputExpected, aiePartitionOutput2)

  # 1a) Check for the existance of the dumped PDI images
  aiePartition1111PDIExpected = os.path.join(args.resource_dir, "1111_expected.pdi")
  aiePartition1111PDIOutput = "transform/00000000-0000-0000-0000-000000001111.pdi"
  binaryFileCompare(aiePartition1111PDIExpected, aiePartition1111PDIOutput)

  # 1b) Check for the existance of the dummy PDI images
  aiePartition2222PDIExpected = os.path.join(args.resource_dir, "2222_expected.pdi")
  aiePartition2222PDIOutput = "transform/00000000-0000-0000-0000-000000002222.pdi"
  binaryFileCompare(aiePartition2222PDIExpected, aiePartition2222PDIOutput)

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

