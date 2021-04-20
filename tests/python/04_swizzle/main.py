#!/usr/bin/python3

#
# SPDX-License-Identifier: Apache-2.0
#
# Copyright (C) 2019-2021 Xilinx, Inc
#

import sys
import traceback
import numpy
# found in PYTHONPATH
import pyxrt

sys.path.append('../') # utils_binding.py
from utils_binding import *

def runKernel(opt):
    result = 0
    d = pyxrt.device(opt.index)
    uuid = d.load_xclbin(opt.bitstreamFile)
    # Instantiate vectorswizzle
    swizzle = pyxrt.kernel(d, uuid, "vectorswizzle")

    elem_num = 4096
    size = ctypes.sizeof(ctypes.c_int) * elem_num

    obj = pyxrt.bo(d, size, pyxrt.bo.normal, swizzle.group_id(0))
    buf = numpy.asarray(obj.map())

    # Compute golden values
    reference = []

    for idx in range(elem_num):
        remainder = idx % 4
        buf[idx] = idx
        if remainder == 0:
            reference.append(idx+2)
        if remainder == 1:
            reference.append(idx+2)
        if remainder == 2:
            reference.append(idx-2)
        if remainder == 3:
            reference.append(idx-2)


    obj.sync(pyxrt.xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE, size, 0)

    # Create a run object without starting kernel
    run = pyxrt.run(swizzle);

    global_dim = [size // 4, 0]; # int4 vector count global range
    local_dim = [16, 0]; # int4 vector count global range
    group_size = global_dim[0] // local_dim[0];

    # Run swizzle with 16 (local[0]) elements at a time
    # Each element is an int4 (sizeof(int) * 4 bytes)
    # Create sub buffer to offset kernel argument in parent buffer
    local_size_bytes = local_dim[0] * ctypes.sizeof(ctypes.c_int) * 4;
    for id in range(group_size):
        subobj = pyxrt.bo(obj, local_size_bytes, local_size_bytes * id)
        run.set_arg(0, subobj)
        run.start()
        state = run.state()
        state = run.wait(5)

    obj.sync(pyxrt.xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, size, 0)

    print("Compare the FPGA results with golden data")
    for idx in range(elem_num):
        assert(buf[idx] == reference[idx])

    return 0

def main(args):
    opt = Options()
    Options.getOptions(opt, args)

    try:
        runKernel(opt)
        print("PASSED TEST")
        return 0

    except Exception as exp:
        print(exp)  # prints the err
        traceback.print_exc(file=sys.stdout)
        print("FAILED TEST")
        return 1

if __name__ == "__main__":
    result = main(sys.argv)
    sys.exit(result)
