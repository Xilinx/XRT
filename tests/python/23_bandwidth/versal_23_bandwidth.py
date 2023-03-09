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

import sys
import time
import math

# Following found in PYTHONPATH setup by XRT
import pyxrt

# utils_binding.py
sys.path.append('../')
from utils_binding import *

current_micro_time = lambda: int(round(time.time() * 1000000))

DATASIZE = int(1024*1024*0.07)    #0.07 MB

def getInputOutputBuffer(devhdl, krnlhdl, argno, isInput):
    bo = pyxrt.bo(devhdl, DATASIZE, pyxrt.bo.normal, krnlhdl.group_id(argno))
    buf = bo.map()

    for i in range(DATASIZE):
        buf[i] = i%256 if isInput else 0

    bo.sync(pyxrt.xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE, DATASIZE, 0)
    return bo, buf

def runKernel(opt):
    d = pyxrt.device(opt.index)
    xbin = pyxrt.xclbin(opt.bitstreamFile)
    uuid = d.load_xclbin(xbin)

    khandle1 = pyxrt.kernel(d, uuid, "bandwidth1", pyxrt.kernel.shared)
    khandle2 = pyxrt.kernel(d, uuid, "bandwidth2", pyxrt.kernel.shared)

    output_bo1, output_buf1 = getInputOutputBuffer(d, khandle1, 0, False)
    output_bo2, output_buf2 = getInputOutputBuffer(d, khandle1, 0, False)
    output_bo3, output_buf3 = getInputOutputBuffer(d, khandle1, 0, False)
    output_bo4, output_buf4 = getInputOutputBuffer(d, khandle2, 0, False)
    output_bo5, output_buf5 = getInputOutputBuffer(d, khandle2, 0, False)
    output_bo6, output_buf6 = getInputOutputBuffer(d, khandle2, 0, False)
    input_bo1, input_buf1 = getInputOutputBuffer(d, khandle1, 1, True)
    input_bo2, input_buf2 = getInputOutputBuffer(d, khandle1, 1, True)
    input_bo3, input_buf3 = getInputOutputBuffer(d, khandle1, 1, True)
    input_bo4, input_buf4 = getInputOutputBuffer(d, khandle2, 1, True)
    input_bo5, input_buf5 = getInputOutputBuffer(d, khandle2, 1, True)
    input_bo6, input_buf6 = getInputOutputBuffer(d, khandle2, 1, True)

    TYPESIZE = 512
    threshold = 40000
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
            rhandle1 = khandle1(output_bo1, input_bo1, output_bo2, input_bo2, output_bo3, input_bo3, beats, reps)
            rhandle2 = khandle2(output_bo4, input_bo4, output_bo5, input_bo5, output_bo6, input_bo6, beats, reps)
            rhandle1.wait()
            rhandle2.wait()
            end = current_micro_time()

            usduration = end-start
            limit = beats*int(TYPESIZE / 8)
            output_bo1.sync(pyxrt.xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, limit, 0)
            output_bo2.sync(pyxrt.xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, limit, 0)
            output_bo3.sync(pyxrt.xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, limit, 0)
            output_bo4.sync(pyxrt.xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, limit, 0)
            output_bo5.sync(pyxrt.xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, limit, 0)
            output_bo6.sync(pyxrt.xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, limit, 0)

            failed = (input_buf1[:limit] != output_buf1[:limit])
            if (failed):
                break
            failed = (input_buf2[:limit] != output_buf2[:limit])
            if (failed):
                break
            failed = (input_buf3[:limit] != output_buf3[:limit])
            if (failed):
                break
            failed = (input_buf4[:limit] != output_buf4[:limit])
            if (failed):
                break
            failed = (input_buf5[:limit] != output_buf5[:limit])
            if (failed):
                break
            failed = (input_buf6[:limit] != output_buf6[:limit])
            if (failed):
                break
            # print("Reps = %d, Beats = %d, Duration = %lf us" %(reps, beats, usduration)) # for debug

            if usduration < fiveseconds:
                reps = reps*2

        dnsduration.append(usduration)
        dsduration.append(dnsduration[test]/1000000.0)
        dbytes.append(reps*beats*int(TYPESIZE / 8))
        dmbytes.append(dbytes[test]/(1024 * 1024))
        bpersec.append(6.0*dbytes[test]/dsduration[test])
        mbpersec.append(2.0*bpersec[test]/(1024 * 1024))
        throughput.append(mbpersec[test])
        print("Test %d, Throughput: %d MB/s" %(test, throughput[test]))
        beats = beats*4
        test+=1

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
        runKernel(opt)
        print("PASSED TEST")
        sys.exit(0)

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

if __name__ == "__main__":
    main(sys.argv)
