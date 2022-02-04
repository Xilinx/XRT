import subprocess
import os
import argparse
from argparse import RawDescriptionHelpFormatter
import filecmp
import json
import binascii

# Start of our unit test
# -- main() -------------------------------------------------------------------
#
# The entry point to this script.
#
# Note: It is called at the end of this script so that the other functions
#       and classes have been defined and the syntax validated
def main():
  # -- Configure the argument parser
  parser = argparse.ArgumentParser(formatter_class=RawDescriptionHelpFormatter, description='description:\n  Unit test wrapper for the adding PS Kernels')
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

  step = "1a) Create shared ps kernel library (compile objects)"

  # Note: This hex image was created from the pskernel.cpp file and converted
  # to hex via the command
  # xxd -p pskernel.so | tr -d ' \n' > pskernel.hex

  psKernelSharedLibraryHex = os.path.join(args.resource_dir, "pskernel.hex")
  psKernelSharedLibrary = "pskernel.so"

  # Read in the hex array
  with open(psKernelSharedLibraryHex) as file:
      hexImage = file.read();

  binImage = bytes.fromhex(hexImage[ : ])

  with open(psKernelSharedLibrary, 'wb') as file:
      file.write(binImage)

  # ---------------------------------------------------------------------------

  step = "2a) Read in a PS kernel, updated and validate the sections"

  inputPSKernelLib = psKernelSharedLibrary
  outputEmbeddedMetadata = "embedded_metadata_updated.xml"
  expectedEmbeddedMetadata = os.path.join(args.resource_dir, "embedded_metadata_expected.xml")

  outputIpLayout = "ip_layout_updated.json"
  expectedIpLayout = os.path.join(args.resource_dir, "ip_layout_expected.json")

  outputConnectivity = "connectivity_updated.json"
  expectedConnectivity = os.path.join(args.resource_dir, "connectivity_expected.json")

  outputMemTopology = "mem_topology_updated.json"
  expectedMemTopology = os.path.join(args.resource_dir, "mem_topology_expected.json")

  outputXCLBIN = "pskernel_output.xclbin"

  cmd = [xclbinutil, "--input", workingXCLBIN,
                     "--add-pskernel", inputPSKernelLib, 
                     "--dump-section", "EMBEDDED_METADATA:RAW:" + outputEmbeddedMetadata,
                     "--dump-section", "IP_LAYOUT:JSON:" + outputIpLayout,
                     "--dump-section", "CONNECTIVITY:JSON:" + outputConnectivity,
                     "--dump-section", "MEM_TOPOLOGY:JSON:" + outputMemTopology,
                     "--output", outputXCLBIN, 
                     "--force"
                     ]
  execCmd(step, cmd)

  # Validate the contents of the various sections
  textFileCompare(outputEmbeddedMetadata, expectedEmbeddedMetadata)
  jsonFileCompare(outputIpLayout, expectedIpLayout)
  jsonFileCompare(outputConnectivity, expectedConnectivity)
  jsonFileCompare(outputMemTopology, expectedMemTopology)

  # ---------------------------------------------------------------------------
  # Validate the contents of the SoftKernel section

  step = "2b) Validate the soft kernel section"

  expectedKernelJSON = os.path.join(args.resource_dir, "pskernel_expected.json")
  outputKernelJSON = "pskernel_output.json"
  softKernelName = "kernel0"

  cmd = [xclbinutil, "--input", outputXCLBIN, 
                     "--dump-section", "SOFT_KERNEL[" + softKernelName + "]-METADATA:JSON:" + outputKernelJSON, 
                     "--force"]
  execCmd(step, cmd)

  jsonFileCompare(expectedKernelJSON, outputKernelJSON)

  # ---------------------------------------------------------------------------
  step = "3) Validate adding just a soft kernel"

  outputOnlyPSKernelXclbin = "only_pskernel.xclbin"
  outputPSKEmbeddedMetadata = "embedded_metadata_psk.xml"
  expectedOnlyPSKEmbeddedMetadata = os.path.join(args.resource_dir, "embedded_metadata_psk_expected.xml")

  outputPSKIpLayout = "ip_layout_psk.json"
  expectedPSKIpLayout = os.path.join(args.resource_dir, "ip_layout_psk_expected.json")

  outputPSKConnectivity = "connectivity_psk.json"
  expectedPSKConnectivity = os.path.join(args.resource_dir, "connectivity_psk_expected.json")

  outputPSKMemTopology = "mem_topology_psk.json"
  expectedPSKMemTopology = os.path.join(args.resource_dir, "mem_topology_psk_expected.json")


  cmd = [xclbinutil, "--add-pskernel", psKernelSharedLibrary,
                     "--dump-section", "EMBEDDED_METADATA:RAW:" + outputPSKEmbeddedMetadata,
                     "--dump-section", "IP_LAYOUT:JSON:" + outputPSKIpLayout,
                     "--dump-section", "CONNECTIVITY:JSON:" + outputPSKConnectivity,
                     "--dump-section", "MEM_TOPOLOGY:JSON:" + outputPSKMemTopology,
                     "--output", outputOnlyPSKernelXclbin, 
                     "--force"]

  # Validate the contents of the various sections
  textFileCompare(outputEmbeddedMetadata, expectedEmbeddedMetadata)
  jsonFileCompare(outputIpLayout, expectedIpLayout)
  jsonFileCompare(outputConnectivity, expectedConnectivity)
  jsonFileCompare(outputMemTopology, expectedMemTopology)

  execCmd(step, cmd)

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

