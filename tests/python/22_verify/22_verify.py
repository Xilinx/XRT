#!/usr/bin/python3

#
# SPDX-License-Identifier: Apache-2.0
#
# Copyright (C) 2019-2021 Xilinx, Inc
#

import os
import sys
import uuid
import re

# Following found in PYTHONPATH setup by XRT
from xrt_binding import *
from ert_binding import *

# found in PYTHONPATH
import pyxrt

# utils_binding.py
sys.path.append('../')
from utils_binding import *


def runKernel(opt):
    d = pyxrt.device(opt.index)
    xbin = pyxrt.xclbin(opt.bitstreamFile)
    uuid = d.load_xclbin(xbin)

    kernellist = xbin.get_kernels()

    rule = re.compile("hello*")
    kernel = list(filter(lambda val: rule.match(val.get_name()), kernellist))[0]
    hello = pyxrt.kernel(d, uuid, kernel.get_name(), pyxrt.kernel.shared)

    zeros = bytearray(opt.DATA_SIZE)
    boHandle1 = pyxrt.bo(d, opt.DATA_SIZE, pyxrt.bo.normal, hello.group_id(0))
    boHandle1.write(zeros, 0)
    buf1 = boHandle1.map()

    boHandle2 = pyxrt.bo(d, opt.DATA_SIZE, pyxrt.bo.normal, hello.group_id(0))
    boHandle2.write(zeros, 0)
    buf2 = boHandle2.map()

    boHandle1.sync(pyxrt.xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE, opt.DATA_SIZE, 0)
    boHandle2.sync(pyxrt.xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE, opt.DATA_SIZE, 0)

    print("Original string = [%s]" % buf1[:64].tobytes())
    print("Original string = [%s]" % buf2[:64].tobytes())

    print("Issue kernel start requests")
    run1 = hello(boHandle1)
    run2 = hello(boHandle2)

    print("Now wait for the kernels to finish using xrtRunWait()")
    state1 = run1.wait()
    state2 = run2.wait()

    print("Get the output data produced by the 2 kernel runs from the device")
    boHandle1.sync(pyxrt.xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, opt.DATA_SIZE, 0)
    boHandle2.sync(pyxrt.xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, opt.DATA_SIZE, 0)

    golden = memoryview(b'Hello World')
    result1 = buf1[:len(golden)]
    result2 = buf2[:len(golden)]
    print("Result string = [%s]" % result1.tobytes())
    print("Result string = [%s]" % result2.tobytes())
    assert(result1 == golden), "Incorrect output from kernel"
    assert(result2 == golden), "Incorrect output from kernel"

def main(args):
    opt = Options()
    b_file = "verify.xclbin"
    Options.getOptions(opt, args, b_file)

    try:
        runKernel(opt)
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
