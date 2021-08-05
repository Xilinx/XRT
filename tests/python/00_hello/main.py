#!/usr/bin/python3

#
# SPDX-License-Identifier: Apache-2.0
#
# Copyright (C) 2019-2021 Xilinx, Inc
#

import os
import sys
import uuid

# Following is found in PYTHONPATH setup by XRT
from xrt_binding import *
from ert_binding import *

# found in PYTHONPATH
import pyxrt

# Following is found in ..
sys.path.append('../')
from utils_binding import *

def runMemTest(opt, d, mem):
    print("Testing memory " + mem.get_tag())
    boHandle1 = pyxrt.bo(d, opt.DATA_SIZE, pyxrt.bo.normal, mem.get_index())
    assert (boHandle1.address() != 0xffffffffffffffff), "Illegal physical address for buffer on memory bank " + mem.get_tag()

    testVector = bytearray(b'hello\nthis is Xilinx OpenCL memory read write test\n:-)\n')
    buf1 = boHandle1.map()
    buf1[:len(testVector)] = testVector
    boHandle1.sync(pyxrt.xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE, opt.DATA_SIZE, 0)
    zeros = bytearray(opt.DATA_SIZE)
    buf1[:len(testVector)] = zeros[:len(testVector)]
#    boHandle1.write(zeros, 0)
    boHandle1.sync(pyxrt.xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, opt.DATA_SIZE, 0)
    assert (buf1[:len(testVector)] == testVector[:]), "Data migration error on memory bank " + mem.get_tag()

def runTest(opt):
    d = pyxrt.device(opt.index)
    xbin = pyxrt.xclbin(opt.bitstreamFile)
    uuid = d.load_xclbin(xbin)
    memlist = xbin.get_mems()
    for m in memlist:
        if (m.get_used() == False):
            continue;
        runMemTest(opt, d, m);

def main(args):
    opt = Options()
    Options.getOptions(opt, args)
    opt.first_mem = 0

    try:
        runTest(opt)
        print("PASSED TEST")
        return 0

    except OSError as o:
        print(o)
        print("FAILED TEST")
        return -o.errno

    except AssertionError as a:
        print(a)
        print("FAILED TEST")
        return -1
    except Exception as e:
        print(e)
        print("FAILED TEST")
        return -1

if __name__ == "__main__":
    result = main(sys.argv)
    sys.exit(result)
