# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.

import json
import subprocess
import sys
from pathlib import Path


START_MARKER = b"XCLBIN_MIRROR_DATA_START"
END_MARKER = b"XCLBIN_MIRROR_DATA_END"


def execCmd(step, cmd, expectedError=None):
  print(step)
  print(" ".join(cmd))

  proc = subprocess.Popen(
    cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  stdout, stderr = proc.communicate()

  output = (
    stdout.decode("utf-8", errors="replace")
    + stderr.decode("utf-8", errors="replace")
  )
  print(output)

  if expectedError is None:
    if proc.returncode != 0:
      raise Exception(
        "Operation failed with return code: " + str(proc.returncode))
  else:
    if proc.returncode == 0:
      raise Exception("Operation unexpectedly succeeded")
    if expectedError not in output:
      raise Exception("Expected diagnostic not found: " + expectedError)

  return output


def main():
  xclbinutil = "xclbinutil"
  platform = "test_vendor_test_board_test_platform_1_0"
  expectedKeyValues = [
    {"key": "migration_test", "value": "preserved"}
  ]

  print("Starting test")

  original = Path("original.xclbin")
  migrated = Path("migrated.xclbin")
  dumpedMetadata = Path("keyvalue_metadata.json")
  noMirror = Path("no_mirror.xclbin")
  failedOutput = Path("failed_output.xclbin")

  execCmd(
    "1) Create an xclbin with embedded mirror metadata",
    [
      xclbinutil,
      "--key-value", "USER:migration_test:preserved",
      "--key-value", "SYS:PlatformVBNV:" + platform,
      "--output", str(original),
      "--force",
    ])

  originalBytes = original.read_bytes()
  start = originalBytes.find(START_MARKER)
  if start == -1:
    raise Exception("Generated xclbin has no mirror start marker")

  end = originalBytes.find(END_MARKER, start + len(START_MARKER))
  if end == -1:
    raise Exception("Generated xclbin has no mirror end marker")

  # Keep the binary layout and file length unchanged while removing the
  # mirror markers. This fixture is used only by the migration reader.
  noMirrorBytes = bytearray(originalBytes)
  noMirrorBytes[start:start + len(START_MARKER)] = (
    b"\0" * len(START_MARKER))
  noMirrorBytes[end:end + len(END_MARKER)] = (
    b"\0" * len(END_MARKER))
  noMirror.write_bytes(noMirrorBytes)

  # Remove any output left by a previous failed test run.
  if failedOutput.exists() or failedOutput.is_symlink():
    failedOutput.unlink()

  execCmd(
    "2) Reject an xclbin without mirror metadata",
    [
      xclbinutil,
      "--input", str(noMirror),
      "--migrate-forward",
      "--output", str(failedOutput),
    ],
    expectedError="ERROR: Mirror backup data not found in given file.")

  if failedOutput.exists():
    raise Exception("Failed migration created an output file")

  execCmd(
    "3) Reconstruct an xclbin from its embedded mirror metadata",
    [
      xclbinutil,
      "--input", str(original),
      "--migrate-forward",
      "--output", str(migrated),
      "--force",
    ])

  if not migrated.is_file():
    raise Exception("Migration did not create an output file")

  # Read through the normal binary path, without --migrate-forward.
  info = execCmd(
    "4) Read the migrated xclbin and dump its user metadata",
    [
      xclbinutil,
      "--input", str(migrated),
      "--info",
      "--dump-section",
      "KEYVALUE_METADATA:JSON:" + str(dumpedMetadata),
      "--force",
    ])

  with dumpedMetadata.open() as stream:
    metadata = json.load(stream)

  actualKeyValues = metadata["keyvalue_metadata"]["key_values"]
  if actualKeyValues != expectedKeyValues:
    raise Exception("Migration changed the user key-value metadata")

  platformValues = [
    line.split(":", 1)[1].strip()
    for line in info.splitlines()
    if line.strip().startswith("Platform VBNV:")
  ]
  if platformValues != [platform]:
    raise Exception("Migration did not preserve Platform VBNV")

  if original.read_bytes() != originalBytes:
    raise Exception("Migration modified the input xclbin")


if __name__ == "__main__":
  try:
    main()
  except Exception as error:
    print(str(error))
    print("Test Status: FAILED")
    sys.exit(1)

  print("Test Status: PASSED")
  sys.exit(0)
