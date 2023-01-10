from argparse import RawDescriptionHelpFormatter
import argparse
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
  parser = argparse.ArgumentParser(formatter_class=RawDescriptionHelpFormatter, description='description:\n  Unit test wrapper for the adding Fixed Kernels')
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

  step = "0) Create working xclbin container"

  inputEmbeddedMetadata = os.path.join(args.resource_dir, "embedded_metadata.xml")
  inputMemTopology = os.path.join(args.resource_dir, "mem_topology.json")
  inputIPLayout = os.path.join(args.resource_dir, "ip_layout.json")
  inputConnectivity = os.path.join(args.resource_dir, "connectivity.json")
  workingXCLBIN = "working.xclbin"

  cmd = [xclbinutil, "--add-section", "EMBEDDED_METADATA:RAW:" + inputEmbeddedMetadata, 
                     "--add-section", "MEM_TOPOLOGY:JSON:" + inputMemTopology, 
                     "--add-section", "IP_LAYOUT:JSON:" + inputIPLayout, 
                     "--add-section", "CONNECTIVITY:JSON:" + inputConnectivity, 
                     "--output", workingXCLBIN, 
                     "--force"]
  execCmd(step, cmd)

  # ---------------------------------------------------------------------------

  step = '''1) Read in a fixed ps kernel, updated and validate the sections
   fixed ps kernel file: fixed_kernel_add.json
   this file has string value for subtype and functional in extended-data'''

  inputJSON = os.path.join(args.resource_dir, "fixed_kernel_add.json")
  outputEmbeddedMetadata = "updated_embedded_metadata.xml"
  expectedEmbeddedMetadata = os.path.join(args.resource_dir, "embedded_metadata_expected.xml")

  outputIpLayout = "updated_ip_layout.json"
  expectedIpLayout = os.path.join(args.resource_dir, "ip_layout_expected.json")

  outputConnectivity = "updated_connectivity.json"
  expectedConnectivity = os.path.join(args.resource_dir, "connectivity_expected.json")

  outputGroupTopology = "updated_group_topology.json"
  expectedGroupTopology = os.path.join(args.resource_dir, "group_topology_expected.json")

  outputGroupConnectivity = "updated_group_connectivity.json"
  expectedGroupConnectivity = os.path.join(args.resource_dir, "group_connectivity_expected.json")

  outputXCLBIN = "pskernel_output.xclbin"

  cmd = [xclbinutil, "--input", workingXCLBIN,
                     "--add-kernel", inputJSON, 
                     "--dump-section", "EMBEDDED_METADATA:RAW:" + outputEmbeddedMetadata,
                     "--dump-section", "IP_LAYOUT:JSON:" + outputIpLayout,
                     "--dump-section", "CONNECTIVITY:JSON:" + outputConnectivity,
                     "--dump-section", "GROUP_TOPOLOGY:JSON:" + outputGroupTopology,
                     "--dump-section", "GROUP_CONNECTIVITY:JSON:" + outputGroupConnectivity,
                     "--output", outputXCLBIN, 
                     "--force"
                     ]
  execCmd(step, cmd)

  # Validate the contents of the various sections
  textFileCompare(outputEmbeddedMetadata, expectedEmbeddedMetadata)
  jsonFileCompare(outputIpLayout, expectedIpLayout)
  jsonFileCompare(outputConnectivity, expectedConnectivity)
  jsonFileCompare(outputGroupTopology, expectedGroupTopology)
  jsonFileCompare(outputGroupConnectivity, expectedGroupConnectivity)

  # ---------------------------------------------------------------------------

  step = '''2) Read in fixed ps kernel, updated and validate the sections
   fixed ps kernel file: fixed_kernel_add_num.json
   this file has numberic value for subtype and functional in extended-data'''

  # fixed_kernel_add_2.json is semantically identical to fixed_kernel_add.json
  # hence all the output files remain the same

  inputJSON = os.path.join(args.resource_dir, "fixed_kernel_add_num.json")

  cmd = [xclbinutil, "--input", workingXCLBIN,
                     "--add-kernel", inputJSON, 
                     "--dump-section", "EMBEDDED_METADATA:RAW:" + outputEmbeddedMetadata,
                     "--dump-section", "IP_LAYOUT:JSON:" + outputIpLayout,
                     "--dump-section", "CONNECTIVITY:JSON:" + outputConnectivity,
                     "--dump-section", "GROUP_TOPOLOGY:JSON:" + outputGroupTopology,
                     "--dump-section", "GROUP_CONNECTIVITY:JSON:" + outputGroupConnectivity,
                     "--output", outputXCLBIN, 
                     "--force"
                     ]
  execCmd(step, cmd)

  # Validate the contents of the various sections
  textFileCompare(outputEmbeddedMetadata, expectedEmbeddedMetadata)
  jsonFileCompare(outputIpLayout, expectedIpLayout)
  jsonFileCompare(outputConnectivity, expectedConnectivity)
  jsonFileCompare(outputGroupTopology, expectedGroupTopology)
  jsonFileCompare(outputGroupConnectivity, expectedGroupConnectivity)

  # ---------------------------------------------------------------------------
  
  step = '''3) Read in fixed ps kernel, updated and validate the sections
   fixed ps kernel file: fixed_kernel_add_2.json
   this file set the functional to 1 (i.e. "PrePost")'''

  inputJSON = os.path.join(args.resource_dir, "fixed_kernel_add_2.json")

  outputEmbeddedMetadata = "updated_embedded_metadata_2.xml"
  expectedEmbeddedMetadata = os.path.join(args.resource_dir, "embedded_metadata_expected_2.xml")

  outputIpLayout = "updated_ip_layout_2.json"
  expectedIpLayout = os.path.join(args.resource_dir, "ip_layout_expected_2.json")

  # other output files should remain the same

  outputXCLBIN = "pskernel_output_2.xclbin"

  cmd = [xclbinutil, "--input", workingXCLBIN,
                     "--add-kernel", inputJSON, 
                     "--dump-section", "EMBEDDED_METADATA:RAW:" + outputEmbeddedMetadata,
                     "--dump-section", "IP_LAYOUT:JSON:" + outputIpLayout,
                     "--dump-section", "CONNECTIVITY:JSON:" + outputConnectivity,
                     "--dump-section", "GROUP_TOPOLOGY:JSON:" + outputGroupTopology,
                     "--dump-section", "GROUP_CONNECTIVITY:JSON:" + outputGroupConnectivity,
                     "--output", outputXCLBIN, 
                     "--force"
                     ]
  execCmd(step, cmd)

  # Validate the contents of the various sections
  textFileCompare(outputEmbeddedMetadata, expectedEmbeddedMetadata)
  jsonFileCompare(outputIpLayout, expectedIpLayout)
  jsonFileCompare(outputConnectivity, expectedConnectivity)
  jsonFileCompare(outputGroupTopology, expectedGroupTopology)
  jsonFileCompare(outputGroupConnectivity, expectedGroupConnectivity)
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

