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
# Following is found in PYTHONPATH
from xrt_binding import *
# Following is found in ..
sys.path.append('../')
from utils_binding import *

def main(args):
    opt = Options()
    Options.getOptions(opt, args)
    try:
        initXRT(opt)
        assert (opt.first_mem >= 0), "Incorrect memory configuration"

        boHandle1 = xclAllocBO(opt.handle, opt.DATA_SIZE, 0, opt.first_mem)
        bo1 = xclMapBO(opt.handle, boHandle1, True)

        testVector = "hello\nthis is Xilinx OpenCL memory read write test\n:-)\n"
        ctypes.memset(bo1, 0, opt.DATA_SIZE)
        ctypes.memmove(bo1, testVector, len(testVector))

        xclSyncBO(opt.handle, boHandle1, xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE, opt.DATA_SIZE, 0)

        p = xclBOProperties(boHandle1)
        xclGetBOProperties(opt.handle, boHandle1, p)
        assert (p.paddr != 0xffffffffffffffff), "Illegal physical address for buffer"

        # Clear our shadow buffer on host
        ctypes.memset(bo1, 0, opt.DATA_SIZE)

        # Move the buffer from device back to the shadow buffer on host
        xclSyncBO(opt.handle, boHandle1, xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, opt.DATA_SIZE, False)

        assert (bo1[:len(testVector)] == testVector[:]), "Data migration error"
        print("PASSED TEST")
        xclUnmapBO(opt.handle, boHandle1, bo1)
        xclFreeBO(opt.handle, boHandle1)

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
