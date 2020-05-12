#!/usr/bin/python3

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

import sys

# Following found in PYTHONPATH setup by XRT
from xrt_binding import *

sys.path.append('../')
from utils_binding import *

def runKernel(opt):
    COUNT = 1024
    DATA_SIZE = ctypes.sizeof(ctypes.c_int32) * COUNT

    khandle = xrtPLKernelOpen(opt.handle, opt.xuuid, "simple")

    boHandle1 = xclAllocBO(opt.handle, DATA_SIZE, 0, opt.first_mem)
    boHandle2 = xclAllocBO(opt.handle, DATA_SIZE, 0, opt.first_mem)

    bo1 = xclMapBO(opt.handle, boHandle1, True)
    bo2 = xclMapBO(opt.handle, boHandle2, True)

    ctypes.memset(bo1, 0, DATA_SIZE)
    ctypes.memset(bo2, 0, DATA_SIZE)

    bo1Int = ctypes.cast(bo1, ctypes.POINTER(ctypes.c_int))
    bo2Int = ctypes.cast(bo2, ctypes.POINTER(ctypes.c_int))

    for i in range(COUNT):
        bo2Int[i] = i

    # bufReference
    bufReference = [i + i*16 for i in range(COUNT)]

    xclSyncBO(opt.handle, boHandle1, xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE, DATA_SIZE, 0)
    xclSyncBO(opt.handle, boHandle2, xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE, DATA_SIZE, 0)

    print("Issue kernel start requests using xrtKernelRun()")
    rhandle1 = xrtKernelRun(khandle, boHandle1, boHandle2, 0x10)

    print("Now wait for the kernels to finish using xrtRunWait()")
    xrtRunWait(rhandle1)

    # get the output xclSyncBO
    print("Get the output data from the device")
    xclSyncBO(opt.handle, boHandle1, xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, DATA_SIZE, 0)

    xrtRunClose(rhandle1)
    xrtKernelClose(khandle)

    assert (bufReference[:COUNT] == bo1Int[:COUNT]), "Computed value does not match reference"
    xclUnmapBO(opt.handle, boHandle2, bo2)
    xclFreeBO(opt.handle, boHandle2)
    xclUnmapBO(opt.handle, boHandle1, bo1)
    xclFreeBO(opt.handle, boHandle1)

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
        xclClose(opt.handle)

if __name__ == "__main__":
    main(sys.argv)
