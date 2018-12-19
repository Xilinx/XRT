import sys
# import source files
sys.path.append('../../../src/python/')
sys.path.append('../')
from xrt_binding import *
from utils_binding import *
from cffi import FFI


def main(args):
    opt = Options()
    Options.getOptions(opt, args)
    try:
        if initXRT(opt):
            return 1
        if opt.first_mem < 0:
            return 1

        boHandle1 = xclAllocBO(opt.handle, opt.DATA_SIZE, xclBOKind.XCL_BO_DEVICE_RAM, opt.first_mem)

        ffi = FFI()
        bo1 = xclMapBO(opt.handle, boHandle1, True)
        testVector = "hello\nthis is Xilinx OpenCL memory read write test\n:-)\n"
        bo1_p = ffi.cast("FILE *", bo1)

        ffi.memmove(bo1_p, testVector, len(testVector))

        bo1_buf = ffi.buffer(bo1_p, len(testVector))
        print("buffer from device: ", bo1_buf[:])

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
        bo2_p = ffi.cast("FILE *", bo2)
        bo2_buf = ffi.buffer(bo2_p, len(testVector))

        if bo1_buf[:] != bo2_buf[:]:
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
