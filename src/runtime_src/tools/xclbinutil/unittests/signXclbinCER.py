# Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
import os
import subprocess
import tempfile

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
  cmd = [xclbinutil, "--output", "unsigned_empty.xclbin", "--force"]
  execCmd(step, cmd)

  step = "3) Sign the xclbin (CER)"
  cmd = [xclbinutil, "--input", "unsigned_empty.xclbin", "--private-key", "private.key", "--certificate", "certificate.cer", "--output", "signed_empty.xclbin", "--force"]
  execCmd(step, cmd)

  step = "4) Validate xclbin (CER)"
  cmd = [xclbinutil, "--input", "signed_empty.xclbin", "--certificate", "certificate.cer", "--validate-signature", "--force"]
  execCmd(step, cmd)

  step = "5) Convert CER certificate to DER"
  cmd = ["openssl", "x509", "-in", "certificate.cer", "-outform", "der", "-out", "certificate.der"]
  execCmd(step, cmd)

  step = "6) Validate xclbin (DER)"
  cmd = [xclbinutil, "--input", "signed_empty.xclbin", "--certificate", "certificate.der", "--validate-signature", "--force"]
  execCmd(step, cmd)


  # ----------------------------------------------------------------------
  # Regression: an invalid digest must not create or overwrite the output.
  # Use a fresh directory so previous test runs cannot affect the assertions.
  with tempfile.TemporaryDirectory(prefix="digest-regression-", dir=".") as testDir:
    output = os.path.join(testDir, "signed.xclbin")

    cmd = [
      xclbinutil,
      "--input", "unsigned_empty.xclbin",
      "--private-key", "private.key",
      "--certificate", "certificate.cer",
      "--digest-algorithm", "bogus_digest_algo",
      "--output", output,
    ]

    step = "7) Reject an invalid digest without creating output"
    checkInvalidDigest(step, cmd)

    if os.path.lexists(output):
      raise Exception("Invalid digest created an output file")

    step = "8) Reject an invalid digest without overwriting existing output"
    # create a dummy output
    originalContents = b"Existing output must remain unchanged.\n"
    with open(output, "wb") as stream:
      stream.write(originalContents)

    checkInvalidDigest(step, cmd + ["--force"])

    with open(output, "rb") as stream:
      if stream.read() != originalContents:
        raise Exception("Invalid digest changed the existing output file")


    # ----------------------------------------------------------------------
    # Verify explicit supported digest algorithms
    for index, digest in enumerate(("sha256", "sha512"), start=1):
      signedOutput = os.path.join(testDir, "signed_" + digest + ".xclbin")

      step = f"9.{index}) Sign the xclbin using {digest}"
      cmd = [
        xclbinutil,
        "--input", "unsigned_empty.xclbin",
        "--private-key", "private.key",
        "--certificate", "certificate.cer",
        "--digest-algorithm", digest,
        "--output", signedOutput,
      ]
      execCmd(step, cmd)

      step = f"10.{index}) Verify the signed xclbin using {digest}"
      cmd = [
        xclbinutil,
        "--input", signedOutput,
        "--certificate", "certificate.cer",
        "--validate-signature",
      ]
      outputText = execCmd(step, cmd)

      if ("Signed xclbin archive verification [SUCCESSFUL]" not in outputText
          or "Signed xclbin archive verification [FAILED]" in outputText):
        raise Exception("Signature verification failed for " + digest)

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

  return (
    o.decode("utf-8", errors="replace")
    + e.decode("utf-8", errors="replace")
  )


def checkInvalidDigest(pretty_name, cmd):
  print(pretty_name)
  print(' '.join(cmd))

  proc = subprocess.Popen(
    cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  stdout, stderr = proc.communicate()

  outputText = (
    stdout.decode("utf-8", errors="replace")
    + stderr.decode("utf-8", errors="replace")
  )
  print(outputText)

  if proc.returncode == 0:
    raise Exception("Invalid digest unexpectedly succeeded")

  expectedError = "ERROR: Invalid digest algorithm: 'bogus_digest_algo'"
  if expectedError not in outputText:
    raise Exception("Command failed without the expected digest error")

  if ("Successfully wrote" in outputText
      or "Signature calculated and added successfully" in outputText):
    raise Exception("Invalid digest produced a success message")

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

