import getopt
import sys
sys.path.append('../../../src/python/')
from xclhal2_binding import *
from utils_binding import *
import argparse


class Options(object):

    DATA_SIZE = 1024
    sharedLibrary = "None"
    bitstreamFile = None
    halLogFile = ""
    alignment = 128
    option_index = 0
    index = 0
    cu_index = 0
    verbose = False
    handle = xclDeviceHandle
    first_mem = -1

    def getOptions(self, argv):
        try:
            opts, args = getopt.getopt(argv[1:], "k:l:a:c:d:vhe", ["bitstream=", "hal_logfile=", "alignment=",
                                                                   "cu_index=", "device=", "verbose", "help", "ert"])
        except getopt.GetoptError:
            print(self.printHelp())
            sys.exit(2)

        for o, arg in opts:
            if o in ("--bitstream", "-k"):
                self.bitstreamFile = arg
            elif o in ("--hal_logfile", "-l"):
                self.halLogFile = arg
            elif o in ("--alignment", "-a"):
                self.alignment = int(arg)
            elif o in ("--cu_index", "-c"):
                self.cu_index = int(arg)
            elif o in ("--device", "-d"):
                self.index = int(arg)
            elif o in ("--help", "-h"):
                print(self.printHelp())
            elif o == "-v":
                self.verbose = True
            elif o in ("-e", "--ert"):
                print('ert')
            else:
                assert False, "unhandled option"

        if self.bitstreamFile is None:
            print("FAILED TEST" + "\n" + "No bitstream specified")
            sys.exit()
        if self.halLogFile:
            print("Using " + self.halLogFile + " as HAL driver logfile")
        print("HAL driver = " + self.sharedLibrary)
        print("Host buffer alignment "+str(self.alignment) + " bytes")
        print("Compiled kernel = " + self.bitstreamFile)

    def printHelp(self):
        print("usage: %s [options] -k <bitstream>")
        print("  -k <bitstream>")
        print("  -l <hal_logfile>")
        print("  -a <alignment>")
        print("  -d <device_index>")
        print("  -c <cu_index>")
        print("  -v")
        print("  -h")
        print("")
        print("  [--ert] enable embedded runtime (default: false)")
        print("")
        print("* If HAL driver is not specified, application will try to find the HAL driver")
        print("  using XILINX_OPENCL and XCL_PLATFORM environment variables")
        print("* Bitstream is required")
        print("* HAL logfile is optional but useful for capturing messages from HAL driver")


def main(args):
    opt = Options()
    print(Options.getOptions(opt, args))
    try:
        handle = xclDeviceHandle()
        cu_base_addr = 0
        if initXRT(opt.bitstreamFile, opt.index, opt.halLogFile, handle, opt.cu_index,
                       cu_base_addr, opt.first_mem):
            return 1

        print(opt.first_mem)
        if opt.first_mem < 0:
            print("shouldn't go in here")
            return 1

        boHandle1 = xclAllocBO(handle, opt.DATA_SIZE, xclBOKind.XCL_BO_DEVICE_RAM, opt.first_mem)
        boHandle2 = xclAllocBO(handle, opt.DATA_SIZE, xclBOKind.XCL_BO_DEVICE_RAM, opt.first_mem)

        bo1 = xclMapBO(handle, boHandle1, True)
        print("<--------------bo1---------->")
        print(bo1)
        # memset(bo1, 0, DATA_SIZE)
        testVector = "hello\nthis is Xilinx OpenCL memory read write test\n:-)\n"
        bo1 = testVector

        # if xclSyncBO(handle, boHandle1, xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE, DATA_SIZE, 0):
        #     print("s")
        #     return 1

        p = xclBOProperties()
        bo2devAddr = p.pddr if not(xclGetBOProperties(handle, boHandle2, p)) else -1
        bo1devAddr = p.pddr if not(xclGetBOProperties(handle, boHandle1, p)) else -1

        # if bo2devAddr is -1 or bo1devAddr is -1:
        #     return 1

        # Allocate the exec_bo unsigned
        execHandle = xclAllocBO(handle, DATA_SIZE, xclBOKind.XCL_BO_SHARED_VIRTUAL, 1)

        # Get the output

        if xclSyncBO(handle, boHandle1, xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, DATA_SIZE, False):
            bo2 = xclMapBO(handle, boHandle1, False)
            # return 1
            if len(bo1) == len(bo2) and all(x == y for x, y in zip(bo1,bo2)):
                print("FAILED TEST")
                print("Value read back does not match value written")
                # return 1
            munmap(bo1, DATA_SIZE)
            munmap(bo2, DATA_SIZE)
            xclFreeBO(handle, boHandle1)
            xclFreeBO(handle, boHandle2)
            xclFreeBO(handle, execHandle)

    except Exception as exp:
        print("Exception: ")
        print(exp)  # prints the err
        sys.exit()

    print("PASSED TEST")


if __name__ == "__main__":
    main(sys.argv)
