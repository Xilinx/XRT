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
    uuid = d.load_xclbin(opt.bitstreamFile)

    rule = re.compile("hello*")
    name = list(filter(lambda val: rule.match, opt.kernels))[0]
    hello = pyxrt.kernel(d, uuid, name)

    grpid = xrtKernelArgGroupId(khandle, 0)
    if grpid < 0:
        raise RuntimeError("failed to find BO group ID: %d" % grpid)

    boHandle1 = pyxrt.bo(d, opt.DATA_SIZE, pyxrt.bo.normal, hello.group_id(0))
    buf1 = numpy.asarray(boHandle1.map())

    boHandle2 = pyxrt.bo(d, opt.DATA_SIZE, pyxrt.bo.normal, hello.group_id(0))
    buf2 = numpy.asarray(boHandle2.map())

    boHandle1.sync(pyxrt.xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE, opt.DATA_SIZE, 0)
    boHandle2.sync(pyxrt.xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE, opt.DATA_SIZE, 0)

    print("Original string = [%s]" % buf1[:64].decode("utf-8"))
    print("Original string = [%s]" % buf2[:64].decode("utf-8"))

    print("Issue kernel start requests")
    run1 = hello(boHandle1)
    run2 = hello(boHandle2)

    print("Now wait for the kernels to finish using xrtRunWait()")
    state1 = run1.wait(5)
    state2 = run2.wait(5)

    print("Get the output data produced by the 2 kernel runs from the device")
    boHandle1.sync(pyxrt.xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, opt.DATA_SIZE, 0)
    boHandle2.sync(pyxrt.xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, opt.DATA_SIZE, 0)

    result1 = buf1[:len("Hello World")]
    result2 = buf2[:len("Hello World")]
    print("Result string = [%s]" % result1.decode("utf-8"))
    print("Result string = [%s]" % result2.decode("utf-8"))
    assert(result1.decode("utf-8") == "Hello World"), "Incorrect output from kernel"
    assert(result2.decode("utf-8") == "Hello World"), "Incorrect output from kernel"

def main(args):
    opt = Options()
    Options.getOptions(opt, args)

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
