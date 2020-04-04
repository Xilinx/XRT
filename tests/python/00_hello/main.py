import sys
sys.path.append('../') # utils_binding.py
from xrt_binding import *
from utils_binding import *

def main(args):
    opt = Options()
    Options.getOptions(opt, args)
    try:
        if initXRT(opt):
            return 1
        if opt.first_mem < 0:
            return 1

        boHandle1 = xclAllocBO(opt.handle, opt.DATA_SIZE, 0, opt.first_mem)
        bo1 = xclMapBO(opt.handle, boHandle1, True)

        testVector = "hello\nthis is Xilinx OpenCL memory read write test\n:-)\n"
        ctypes.memset(bo1, 0, opt.DATA_SIZE)
        ctypes.memmove(bo1, testVector, len(testVector))

        xclSyncBO(opt.handle, boHandle1, xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE, opt.DATA_SIZE, 0)

        p = xclBOProperties()
        bo1devAddr = p.paddr if not(xclGetBOProperties(opt.handle, boHandle1, p)) else -1

        if bo1devAddr is -1:
            return 1

        # Clear our shadow buffer on host
        ctypes.memset(bo1, 0, opt.DATA_SIZE)

        # Move the buffer from device back to the shadow buffer on host
        xclSyncBO(opt.handle, boHandle1, xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, opt.DATA_SIZE, False)

        assert(bo1.contents[:len(testVector)] == testVector[:])

    except OSError as e:
        print("FAILED TEST")
        print(e)
        sys.exit(e.errno)
    except Exception as f:
        print("FAILED TEST")
        print(f)
        sys.exit(1)

    print("PASSED TEST")

if __name__ == "__main__":
    main(sys.argv)
