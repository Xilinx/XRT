import subprocess
import os

# Start of our unit test
# -- main() -------------------------------------------------------------------
#
# The entry point to this script.
#
# Note: It is called at the end of this script so that the other functions
#       and classes have been defined and the syntax validated
def main():
  xclbinutil = "xclbinutil"

  print ("Starting test")

  step = "1) Create the keys"
  cmd = ["openssl", "req", "-x509", "-newkey", "rsa:1024", "-keyout", "private.key", "-out", "certificate.cer", "-nodes", "-subj", "/CN=PKCS#7 example"]
  execCmd(step, cmd)

  step = "2) Create an empty unsigned xclbin"
  cmd = [xclbinutil, "--output", "unsigned_empty.xclbin", "--force", "--trace"]
  execCmd(step, cmd)

  step = "3) Sign the xclbin (CER)"
  cmd = [xclbinutil, "--input", "unsigned_empty.xclbin", "--private-key", "private.key", "--certificate", "certificate.cer", "--output", "signed_empty.xclbin", "--force", "--trace"]
  execCmd(step, cmd)

  step = "4) Validate xclbin (CER)"
  cmd = [xclbinutil, "--input", "signed_empty.xclbin", "--certificate", "certificate.cer", "--validate-signature", "--force", "--trace"]
  execCmd(step, cmd)

  step = "5) Convert CER certificate to DER"
  cmd = ["openssl", "x509", "-in", "certificate.cer", "-outform", "der", "-out", "certificate.der"]
  execCmd(step, cmd)

  step = "6) Validate xclbin (DER)"
  cmd = [xclbinutil, "--input", "signed_empty.xclbin", "--certificate", "certificate.der", "--validate-signature", "--force", "--trace"]
  execCmd(step, cmd)

  # If the code gets this far, all is good.
  return False


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

def printEnvironment():
    print("\nLocal files:")
    files = os.listdir(".")
    for file in files:
        print("  " + file)

    print("\nEnvironment Variables")
    for k, v in sorted(os.environ.items()):
        print(k+':', v)
    print('\n')

    # list elements in path environment variable
    print("\nPath breakdown:")
    for item in os.environ['PATH'].split(':'):
        print("  " + item)



# -- Start executing the script functions
if __name__ == '__main__':
  try:
    if main() == True:
      print ("\nError(s) occurred.")
      printEnvironment()
      print("Test Status: FAILED")
      exit(1)
  except Exception as error:
    print(repr(error))
    printEnvironment()
    print("Test Status: FAILED")
    exit(1)


# If the code get this far then no errors occured
printEnvironment()
print("Test Status: PASSED")
exit(0)

