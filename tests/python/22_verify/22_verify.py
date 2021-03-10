"""
 Copyright (C) 2019-2020 Xilinx, Inc

 ctypes based Python binding for XRT

 Licensed under the Apache License, Version 2.0 (the "License"). You may
 not use this file except in compliance with the License. A copy of the
 License is located at

     http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 License for the specific language governing permissions and limitations
 under the License.
"""

import os
import sys
import uuid
import re

# Following found in PYTHONPATH setup by XRT
from xrt_binding import *
from ert_binding import *

# utils_binding.py
sys.path.append('../')
from utils_binding import *


def runKernel(opt):

    rule = re.compile("hello*")
    name = list(filter(lambda val: rule.match, opt.kernels))[0]
    khandle = xrtPLKernelOpen(opt.handle, opt.xuuid, name)

    grpid = xrtKernelArgGroupId(khandle, 0)
    if grpid < 0:
        raise RuntimeError("failed to find BO group ID: %d" % grpid)

    boHandle1 = xrtBOAlloc(opt.handle, opt.DATA_SIZE, 0, grpid)
    buf1 = xrtBOMap(boHandle1)
    bo1 = ctypes.cast(buf1, ctypes.POINTER(ctypes.c_char))
    if bo1 == 0:
        raise RuntimeError("failed to map buffer1")
    ctypes.memset(bo1, 0, opt.DATA_SIZE)

    boHandle2 = xrtBOAlloc(opt.handle, opt.DATA_SIZE, 0, grpid)
    buf2 = xrtBOMap(boHandle2)
    bo2 = ctypes.cast(buf2, ctypes.POINTER(ctypes.c_char))
    if bo2 == 0:
        raise RuntimeError("failed to map buffer2")
    ctypes.memset(bo2, 0, opt.DATA_SIZE)

    xrtBOSync(boHandle1, xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE, opt.DATA_SIZE, 0)
    xrtBOSync(boHandle2, xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE, opt.DATA_SIZE, 0)

    print("Original string = [%s]" % bo1[:64].decode("utf-8"))
    print("Original string = [%s]" % bo2[:64].decode("utf-8"))

    print("Issue kernel start requests")
    kfunc = xrtKernelGetFunc(xrtBufferHandle)
    rhandle1 = kfunc(khandle, boHandle1)
    rhandle2 = kfunc(khandle, boHandle2)

    print("Now wait for the kernels to finish using xrtRunWait()")
    xrtRunWait(rhandle1)
    xrtRunWait(rhandle2)

    print("Get the output data produced by the 2 kernel runs from the device")
    xrtBOSync(boHandle1, xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, opt.DATA_SIZE, 0)
    xrtBOSync(boHandle2, xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, opt.DATA_SIZE, 0)
    result1 = bo1[:len("Hello World")]
    result2 = bo2[:len("Hello World")]
    print("Result string = [%s]" % result1.decode("utf-8"))
    print("Result string = [%s]" % result2.decode("utf-8"))
    assert(result1.decode("utf-8") == "Hello World"), "Incorrect output from kernel"
    assert(result2.decode("utf-8") == "Hello World"), "Incorrect output from kernel"

    xrtRunClose(rhandle2)
    xrtRunClose(rhandle1)
    xrtKernelClose(khandle)
    xrtBOFree(boHandle2)
    xrtBOFree(boHandle1)

def main(args):
    opt = Options()
    Options.getOptions(opt, args)

    try:
        initXRT(opt)
        assert (opt.first_mem >= 0), "Incorrect memory configuration"

        runKernel(opt)
        print("PASSED TEST")

    except OSError as o:
        print(o)
        print("FAILED TEST")
        sys.exit(o.errno)
    except AssertionError as a:
        print(a)
        print("FAILED TEST")
        sys.exit(1)
    except Exception as e:
        print(e)
        print("FAILED TEST")
        sys.exit(1)
    finally:
        xrtDeviceClose(opt.handle)

if __name__ == "__main__":
    #os.environ["Runtime.xrt_bo"] = "false"
    main(sys.argv)
