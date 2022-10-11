#!/usr/bin/python3

#
# SPDX-License-Identifier: Apache-2.0
#
# Copyright (C) 2019-2021 Xilinx, Inc
#

import ctypes.util
import sys
import time
import math

# Following found in PYTHONPATH setup by XRT
from xrt_binding import *
from ert_binding import *
import pyxrt

# utils_binding.py
sys.path.append('../')
from utils_binding import *

current_micro_time = lambda: int(round(time.time() * 1000000))

DATASIZE = int(1024*1024*16)    #16 MB

def getThreshold(devhdl):
    threshold = 40000
    name = devhdl.get_info(pyxrt.xrt_info_device.name);

    if "qdma" in name or "qep" in name:
        threshold = 30000
    if "gen3x4" in name or "_u26z_" in name:
        threshold = 20000
    if "u2x4" in name or "U2x4" in name or "u2_gen3x4" in name:
        threshold = 10000
    if "_u25_" in name or "_u30_" in name: # so that it doesn't set theshold for u250
        threshold = 9000
    return threshold

def getInputOutputBuffer(devhdl, krnlhdl, argno, isInput):
    bo = pyxrt.bo(devhdl, DATASIZE, pyxrt.bo.normal, krnlhdl.group_id(argno))
    buf = bo.map();

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
    output_bo2, output_buf2 = getInputOutputBuffer(d, khandle2, 0, False)
    input_bo1, input_buf1 = getInputOutputBuffer(d, khandle1, 1, True)
    input_bo2, input_buf2 = getInputOutputBuffer(d, khandle2, 1, True)

    TYPESIZE = 512
    threshold = getThreshold(d)
    beats = 16

    #lists
    dnsduration = []
    dsduration  = []
    dbytes      = []
    dmbytes     = []
    bpersec     = []
    mbpersec    = []

    #run tests with burst length 1 beat to DATASIZE
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
            rhandle1 = khandle1(output_bo1, input_bo1, beats, reps)
            rhandle2 = khandle2(output_bo2, input_bo2, beats, reps)
            rhandle1.wait()
            rhandle2.wait()
            end = current_micro_time()

            usduration = end - start
            limit = beats * int(TYPESIZE / 8)
            output_bo1.sync(pyxrt.xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, limit, 0)
            output_bo2.sync(pyxrt.xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, limit, 0)

            failed = (input_buf1[:limit] != output_buf1[:limit]);
            if (failed):
                break

            failed = (input_buf2[:limit] != output_buf2[:limit]);
            if (failed):
                break
            # print("Reps = %d, Beats = %d, Duration = %lf us" %(reps, beats, usduration)) # for debug

            if usduration < fiveseconds:
                reps = reps*2

        dnsduration.append(usduration)
        dsduration.append(dnsduration[test]/1000000.0)
        dbytes.append(reps*beats*int(TYPESIZE / 8))
        dmbytes.append(dbytes[test]/(1024 * 1024))
        bpersec.append(2.0*dbytes[test]/dsduration[test])
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
