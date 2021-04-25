#!/usr/bin/python3

#
# SPDX-License-Identifier: Apache-2.0
#
# Copyright (C) 2021 Xilinx, Inc
#
# Sanity test to ensure we can resolve python bindings without executing them
#

import os
import sys

# Following found in PYTHONPATH setup by XRT
import pyxrt as pp
import xrt_binding as xx
import ert_binding as ee

def reflect():
    count = 0;
    print("Begin XRT pybind11 reflection\n")
    l = dir(pp)
    count = len(l);
    print(l)
    print("Begin XRT C binding reflection\n")
    l = dir(xx);
    count += len(l)
    print(l)
    print("Begin XRT/ERT C binding reflection\n")
    l = dir(ee)
    count += len(l)
    print(dir(ee))
    print("End all reflection\n")
    return count

def main(args):

    try:
        count = xx.xclProbe()
        count = reflect()
        xx.xclLogMsg(None, xx.xrtLogMsgLevel.XRT_ERROR, "PYTHON_TEST", "%d", count)
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
