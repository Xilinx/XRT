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
import errno

# Following found in PYTHONPATH setup by XRT
import pyxrt

# utils_binding.py
sys.path.append('../')
from utils_binding import *

current_micro_time = lambda: int(round(time.time() * 1000000))

DATASIZE = int(1024*1024*2)    #2 MB

def getThreshold(devHandle):
    threshold = 30000
    name = devHandle.get_info(pyxrt.xrt_info_device.name)

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
    bo = pyxrt.bo(devhdl, DATASIZE, pyxrt.bo.host_only, krnlhdl.group_id(argno))
    buf = bo.map()

    for i in range(DATASIZE):
        buf[i] = i%256 if isInput else 0

    bo.sync(pyxrt.xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE, DATASIZE, 0)
    return bo, buf

def runKernel(opt):
    d = pyxrt.device(opt.index)
    xbin = pyxrt.xclbin(opt.bitstreamFile)
    uuid = d.load_xclbin(xbin)

    try:
        khandle3 = pyxrt.kernel(d, uuid, "bandwidth3", pyxrt.kernel.shared)
    except Exception as e:
        return errno.EOPNOTSUPP

    output_bo3, output_buf3 = getInputOutputBuffer(d, khandle3, 0, False)
    input_bo3, input_buf3 = getInputOutputBuffer(d, khandle3, 1, True)

    TYPESIZE = 512
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
            rhandle3 = khandle3(output_bo3, input_bo3, beats, reps)
            rhandle3.wait()
            end = current_micro_time()

            usduration = end - start
            limit = beats * int(TYPESIZE / 8)
            output_bo3.sync(pyxrt.xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, limit, 0)
            failed = (input_buf3[:limit] != output_buf3[:limit])
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


def main(args):
    opt = Options()
    b_file = "hostmemory.xclbin"
    Options.getOptions(opt, args, b_file)

    try:
        if (runKernel(opt) == errno.EOPNOTSUPP):
            print("NOT SUPPORTED TEST")
            sys.exit(errno.EOPNOTSUPP)
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
