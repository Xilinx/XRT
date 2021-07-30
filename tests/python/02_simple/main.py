#!/usr/bin/python3

#
# SPDX-License-Identifier: Apache-2.0
#
# Copyright (C) 2019-2021 Xilinx, Inc
#

import os
import sys
import numpy

# Following found in PYTHONPATH setup by XRT
from xrt_binding import *

sys.path.append('../')
from utils_binding import *

def runKernel(opt):
    d = pyxrt.device(opt.index)
    xbin = pyxrt.xclbin(opt.bitstreamFile)
    uuid = d.load_xclbin(xbin)

    COUNT = 1024
    DATA_SIZE = ctypes.sizeof(ctypes.c_int32) * COUNT

    # Instantiate simple
    simple = pyxrt.kernel(d, uuid, "simple")

    print("Allocate and initialize buffers")
    boHandle1 = pyxrt.bo(d, DATA_SIZE, pyxrt.bo.normal, simple.group_id(0))
    boHandle2 = pyxrt.bo(d, DATA_SIZE, pyxrt.bo.normal, simple.group_id(1))
    bo1 = numpy.asarray(boHandle1.map())
    bo2 = numpy.asarray(boHandle2.map())

    for i in range(COUNT):
        bo1[i] = 0
        bo2[i] = i

    bufReference = [i + i*16 for i in range(COUNT)]

    boHandle1.sync(pyxrt.xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE, DATA_SIZE, 0)
    boHandle2.sync(pyxrt.xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE, DATA_SIZE, 0)

    print("Start the kernel, simple")
    run = simple(boHandle1, boHandle2, 0x10)
    print("Now wait for the kernel simple to finish")
    state = run.wait(5)

    print("Get the output data from the device and validate it")
    boHandle1.sync(pyxrt.xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, DATA_SIZE, 0)
    assert (bufReference[:COUNT] == bo1[:COUNT]), "Computed value does not match reference"

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
    os.environ["Runtime.xrt_bo"] = "false"
    result = main(sys.argv)
    sys.exit(result)
