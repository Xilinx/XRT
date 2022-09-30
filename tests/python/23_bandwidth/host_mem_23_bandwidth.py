"""
 Copyright (C) 2020 Xilinx, Inc

 Ctypes based based bandwidth testcase used with every platform as part of
 xbutil validate

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

import ctypes.util
import sys
import time
import math
import errno

# Following found in PYTHONPATH setup by XRT
from xrt_binding import *
from ert_binding import *

# utils_binding.py
sys.path.append('../')
from utils_binding import *

# Define libc helpers
libc_name = ctypes.util.find_library("c")
libc = ctypes.CDLL(libc_name)
libc.memcmp.argtypes = (ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t)
libc.memcmp.restype = (ctypes.c_int)
libc.memcpy.argtypes = (ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t)
libc.memcpy.restype = (ctypes.c_void_p)

current_micro_time = lambda: int(round(time.time() * 1000000))

globalbuffersize = 1024*1024*2    #2 MB

def getThreshold(devHandle):
    threshold = 30000
    deviceInfo = xclDeviceInfo2()
    xclGetDeviceInfo2(devHandle, ctypes.byref(deviceInfo))
    if b"qdma" in deviceInfo.mName or b"qep" in deviceInfo.mName:
        threshold = 30000
    if b"gen3x4" in deviceInfo.mName or b"_u26z_" in deviceInfo.mName:
        threshold = 20000
    if b"u2x4" in deviceInfo.mName or b"U2x4" in deviceInfo.mName or b"u2_gen3x4" in deviceInfo.mName:
        threshold = 10000
    if b"_u25_" in deviceInfo.mName or b"_u30_" in deviceInfo.mName: # so that it doesn't set theshold for u250
        threshold = 9000
    return threshold

def getInputOutputBuffer(devhdl, krnlhdl, argno, isInput):
    grpid = xrtKernelArgGroupId(krnlhdl, argno)
    if grpid < 0:
        raise RuntimeError("failed to find BO group ID: %d" % grpid)
    # To alloc HOST only buffer, we have to specify flags XCL_BO_FLAGS_HOST_ONLY = 1<<29
    bo = xrtBOAlloc(devhdl, globalbuffersize, 1<<29, grpid)
    if bo == 0:
        raise RuntimeError("failed to alloc buffer")

    bobuf = xrtBOMap(bo)
    if bobuf == 0:
        raise RuntimeError("failed to map buffer")
    lst = ctypes.cast(bobuf, ctypes.POINTER(ctypes.c_char))

    for i in range(globalbuffersize):
        lst[i] = i%256 if isInput else 0

    xrtBOSync(bo, xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE, globalbuffersize, 0)
    return bo, bobuf

def runKernel(opt):
    try:
        khandle3 = xrtPLKernelOpen(opt.handle, opt.xuuid, "bandwidth3")
    except Exception as e:
        return errno.EOPNOTSUPP

    kfunc = xrtKernelGetFunc(xrtBufferHandle, xrtBufferHandle, ctypes.c_int, ctypes.c_int)

    output_bo3, output_buf3 = getInputOutputBuffer(opt.handle, khandle3, 0, False)
    input_bo3, input_buf3 = getInputOutputBuffer(opt.handle, khandle3, 1, True)

    typesize = 512
    globalbuffersizeinbeats = globalbuffersize/(typesize>>3)
    tests= int(math.log(globalbuffersizeinbeats, 2.0))+1
    beats = 16

    #lists
    dnsduration = []
    dsduration  = []
    dbytes      = []
    dmbytes     = []
    bpersec     = []
    mbpersec    = []

    #run tests with burst length 1 beat to globalbuffersize
    #double burst length each test
    test=0
    throughput = []
    failed = False
    while beats <= 1024 and not failed:
        print("LOOP PIPELINE %d beats" %beats)

        usduration = 0
        fiveseconds = 5*1000000
        reps = 64
        while usduration < fiveseconds:
            start = current_micro_time()
            rhandle3 = kfunc(khandle3, output_bo3, input_bo3, beats, reps)
            xrtRunWait(rhandle3)
            end = current_micro_time()

            xrtRunClose(rhandle3)

            usduration = end-start

            limit = beats*(typesize>>3)
            xrtBOSync(output_bo3, xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, limit, 0)
            if libc.memcmp(input_buf3, output_buf3, limit):
               failed = True
               break

            # print("Reps = %d, Beats = %d, Duration = %lf us" %(reps, beats, usduration)) # for debug

            if usduration < fiveseconds:
                reps = reps*2

        dnsduration.append(usduration)
        dsduration.append(dnsduration[test]/1000000.0)
        dbytes.append(reps*beats*(typesize>>3))
        dmbytes.append(dbytes[test]/(1024 * 1024))
        bpersec.append(2.0*dbytes[test]/dsduration[test])
        mbpersec.append(2.0*bpersec[test]/(1024 * 1024))
        throughput.append(mbpersec[test])
        print("Test %d, Throughput: %d MB/s" %(test, throughput[test]))
        beats = beats*4
        test+=1

    #cleanup
    xrtBOFree(input_bo3)
    xrtBOFree(output_bo3)
    xrtKernelClose(khandle3)

    if failed:
        raise RuntimeError("ERROR: Failed to copy entries")
    
    print("TTTT: %d" %throughput[0])
    print("Maximum throughput: %d MB/s" %max(throughput))


def main(args):
    opt = Options()
    b_file = "hostmemory.xclbin"
    Options.getOptions(opt, args, b_file)

    try:
        initXRT(opt)
        assert (opt.first_mem >= 0), "Incorrect memory configuration"

        if (runKernel(opt) == errno.EOPNOTSUPP):
            print("NOT SUPPORTED TEST")
            sys.exit(errno.EOPNOTSUPP)
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
    main(sys.argv)
