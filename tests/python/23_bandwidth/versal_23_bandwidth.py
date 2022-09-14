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

globalbuffersize = int(1024*1024*0.07)    #0.07 MB

def getInputOutputBuffer(devhdl, krnlhdl, argno, isInput):
    grpid = xrtKernelArgGroupId(krnlhdl, argno)
    if grpid < 0:
        raise RuntimeError("failed to find BO group ID: %d" % grpid)
    bo = xrtBOAlloc(devhdl, globalbuffersize, 0, grpid)
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
    khandle1 = xrtPLKernelOpen(opt.handle, opt.xuuid, "bandwidth1")
    khandle2 = xrtPLKernelOpen(opt.handle, opt.xuuid, "bandwidth2")
    kfunc = xrtKernelGetFunc(xrtBufferHandle, xrtBufferHandle, xrtBufferHandle, xrtBufferHandle, xrtBufferHandle, xrtBufferHandle, ctypes.c_int, ctypes.c_int)

    output_bo1, output_buf1 = getInputOutputBuffer(opt.handle, khandle1, 0, False)
    output_bo2, output_buf2 = getInputOutputBuffer(opt.handle, khandle1, 0, False)
    output_bo3, output_buf3 = getInputOutputBuffer(opt.handle, khandle1, 0, False)
    output_bo4, output_buf4 = getInputOutputBuffer(opt.handle, khandle2, 0, False)
    output_bo5, output_buf5 = getInputOutputBuffer(opt.handle, khandle2, 0, False)
    output_bo6, output_buf6 = getInputOutputBuffer(opt.handle, khandle2, 0, False)
    input_bo1, input_buf1 = getInputOutputBuffer(opt.handle, khandle1, 1, True)
    input_bo2, input_buf2 = getInputOutputBuffer(opt.handle, khandle1, 1, True)
    input_bo3, input_buf3 = getInputOutputBuffer(opt.handle, khandle1, 1, True)
    input_bo4, input_buf4 = getInputOutputBuffer(opt.handle, khandle2, 1, True)
    input_bo5, input_buf5 = getInputOutputBuffer(opt.handle, khandle2, 1, True)
    input_bo6, input_buf6 = getInputOutputBuffer(opt.handle, khandle2, 1, True)

    typesize = 512
    threshold = 40000
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
            rhandle1 = kfunc(khandle1, output_bo1, input_bo1, output_bo2, input_bo2, output_bo3, input_bo3, beats, reps)
            rhandle2 = kfunc(khandle2, output_bo4, input_bo4, output_bo5, input_bo5, output_bo6, input_bo6, beats, reps)
            xrtRunWait(rhandle1)
            xrtRunWait(rhandle2)
            end = current_micro_time()

            xrtRunClose(rhandle1)
            xrtRunClose(rhandle2)

            usduration = end-start

            limit = beats*(typesize>>3)
            xrtBOSync(output_bo1, xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, limit, 0)
            xrtBOSync(output_bo2, xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, limit, 0)
            xrtBOSync(output_bo3, xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, limit, 0)
            xrtBOSync(output_bo4, xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, limit, 0)
            xrtBOSync(output_bo5, xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, limit, 0)
            xrtBOSync(output_bo6, xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, limit, 0)
            if libc.memcmp(input_buf1, output_buf1, limit) or libc.memcmp(input_buf2, output_buf2, limit):
               failed = True
               break
            if libc.memcmp(input_buf3, output_buf3, limit) or libc.memcmp(input_buf4, output_buf4, limit):
               failed = True
               break
            if libc.memcmp(input_buf5, output_buf5, limit) or libc.memcmp(input_buf6, output_buf6, limit):
               failed = True
               break

            # print("Reps = %d, Beats = %d, Duration = %lf us" %(reps, beats, usduration)) # for debug

            if usduration < fiveseconds:
                reps = reps*2

        dnsduration.append(usduration)
        dsduration.append(dnsduration[test]/1000000.0)
        dbytes.append(reps*beats*(typesize>>3))
        dmbytes.append(dbytes[test]/(1024 * 1024))
        bpersec.append(6.0*dbytes[test]/dsduration[test])
        mbpersec.append(2.0*bpersec[test]/(1024 * 1024))
        throughput.append(mbpersec[test])
        print("Test %d, Throughput: %d MB/s" %(test, throughput[test]))
        beats = beats*4
        test+=1

    #cleanup
    xrtBOFree(input_bo1)
    xrtBOFree(input_bo2)
    xrtBOFree(output_bo1)
    xrtBOFree(output_bo2)
    xrtKernelClose(khandle1)
    xrtKernelClose(khandle2)

    if failed:
        raise RuntimeError("ERROR: Failed to copy entries")
    
    print("TTTT: %d" %throughput[0])
    print("Maximum throughput: %d MB/s" %max(throughput))
    if max(throughput) < threshold:
        raise RuntimeError("ERROR: Throughput is less than expected value of %d GB/sec" %(threshold/1000))

def main(args):
    opt = Options()
    b_file = "bandwidth.xclbin"
    Options.getOptions(opt, args, b_file)

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
    main(sys.argv)
