import sys
import uuid
from xrt_binding import * # found in PYTHONPATH
from ert_binding import * # found in PYTHONPATH
sys.path.append('../') # utils_binding.py
from utils_binding import *

def runKernel(opt):

    khandle = xrtPLKernelOpen(opt.handle, opt.xuuid, "hello:hello_1")

    boHandle1 = xclAllocBO(opt.handle, opt.DATA_SIZE, 0, opt.first_mem)
    bo1 = xclMapBO(opt.handle, boHandle1, True)
    ctypes.memset(bo1, 0, opt.DATA_SIZE)

    boHandle2 = xclAllocBO(opt.handle, opt.DATA_SIZE, 0, opt.first_mem)
    bo2 = xclMapBO(opt.handle, boHandle2, True)
    ctypes.memset(bo2, 0, opt.DATA_SIZE)

    xclSyncBO(opt.handle, boHandle1, xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE, opt.DATA_SIZE, 0)
    xclSyncBO(opt.handle, boHandle2, xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE, opt.DATA_SIZE, 0)

    print("Original string = [%s]" % bo1.contents[:64].decode("utf-8"))
    print("Original string = [%s]" % bo2.contents[:64].decode("utf-8"))

    print("Issue kernel start requests using xrtKernelRun()")
    rhandle1 = xrtKernelRun(khandle, boHandle1)
    rhandle2 = xrtKernelRun(khandle, boHandle2)

    print("Now wait for the kernels to finish using xrtRunWait()")
    xrtRunWait(rhandle1)
    xrtRunWait(rhandle2)

    print("Get the output data produced by the 2 kernel runs from the device")
    xclSyncBO(opt.handle, boHandle1, xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, opt.DATA_SIZE, 0)
    xclSyncBO(opt.handle, boHandle2, xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, opt.DATA_SIZE, 0)
    result1 = bo1.contents[:len("Hello World")]
    result2 = bo2.contents[:len("Hello World")]
    print("Result string = [%s]" % result1.decode("utf-8"))
    print("Result string = [%s]" % result2.decode("utf-8"))

    assert(result1 == "Hello World"), "Incorrect output from kernel"
    assert(result2 == "Hello World"), "Incorrect output from kernel"

    xrtRunClose(rhandle2)
    xrtRunClose(rhandle1)
    xrtKernelClose(khandle)
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
