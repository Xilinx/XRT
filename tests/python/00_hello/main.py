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

        boHandle1 = xclAllocBO(opt.handle, opt.DATA_SIZE, xclBOKind.XCL_BO_DEVICE_RAM, opt.first_mem)

        bo1 = xclMapBO(opt.handle, boHandle1, True)
        testVector = "hello\nthis is Xilinx OpenCL memory read write test\n:-)\n"
        ctypes.memset(bo1, 0, opt.DATA_SIZE)

        ctypes.memmove(bo1, testVector, len(testVector))

        print("buffer from device: \n%s") % bo1.contents[:55]

        if xclSyncBO(opt.handle, boHandle1, xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE, opt.DATA_SIZE, 0):
            return 1

        p = xclBOProperties()
        bo1devAddr = p.paddr if not(xclGetBOProperties(opt.handle, boHandle1, p)) else -1

        if bo1devAddr is -1:
            return 1

        # Get the output
        if xclSyncBO(opt.handle, boHandle1, xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, opt.DATA_SIZE, False):
            return 1

        bo2 = xclMapBO(opt.handle, boHandle1, False)

        if bo1.contents[:] != bo2.contents[:]:
            print("FAILED TEST")
            print("Value read back does not match value written")
            return 1

    except Exception as exp:
        print("Exception: ")
        print(exp)  # prints the err
        sys.exit()

    print("PASSED TEST")


if __name__ == "__main__":
    main(sys.argv)
