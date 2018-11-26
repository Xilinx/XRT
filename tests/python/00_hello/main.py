import getopt
import sys
sys.path.append('../../../src/python/')
from xclhal2_binding import *
from utils_binding import *


def main():
    try:
        opts, args = getopt.getopt(sys.argv[1:], "k:l:a:c:d:vhe", ["bitstream=", "hal_logfile=", "alignment=",
                                                                   "cu_index=", "device=", "verbose", "help", "ert"])
    except getopt.GetoptError:
        print(printHelp())
        sys.exit(2)
        
    sharedLibrary = "None"
    bitstreamFile = None
    halLogFile = ""
    alignment = 128
    option_index = 0
    index = 0
    cu_index = 0
    verbose = False

    for o, arg in opts:
        if o in ("--bitstream", "-k"):
            bitstreamFile = arg
        elif o in ("--hal_logfile", "-l"):
            halLogFile = arg
        elif o in ("--alignment", "-a"):
            alignment = int(arg)
        elif o in ("--cu_index", "-c"):
            cu_index = int(arg)
        elif o in ("--device", "-d"):
            index = int(arg)
        elif o in ("--help", "-h"):
            print(printHelp())
        elif o == "-v":
            verbose = True
        elif o in ("-e", "--ert"):
            print('ert')
        else:
            assert False, "unhandled option"

    if bitstreamFile is None:
        print("FAILED TEST"+ "\n" + "No bitstream specified")
        sys.exit()
    if halLogFile:
        print("Using " + halLogFile + " as HAL driver logfile")
    print("HAL driver = " + sharedLibrary)
    print("Host buffer alignment "+str(alignment)+ " bytes")
    print("Compiled kernel = " + bitstreamFile)

    try:
        handle = xclDeviceHandle
        cu_base_addr = 0
        first_mem = -1
        if initXRT(bitstreamFile, index, halLogFile, handle, cu_index, cu_base_addr, first_mem):
            return 1
        if first_mem < 0:
            return 1

        boHandle1 = xclAllocBO(handle, DATA_SIZE, XCL_BO_DEVICE_RAM.name, first_mem)
        boHandle2 = xclAllocBO(handle, DATA_SIZE, XCL_BO_DEVICE_RAM.name, first_mem)
        bo1 = xclMapBO(handle, boHandle1, True)
        # memset(bo1, 0, DATA_SIZE)
        testVector = "hello\nthis is Xilinx OpenCL memory read write test\n:-)\n"
        bol = testVector #??
        # strcpy(bo1, testVector.c_str())

        if xclSyncBO(handle, boHandle1, xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE , DATA_SIZE,0):
            return 1

        p = xclBOProperties
        bo2devAddr =p.pddr if not(xclGetBOProperties(handle, boHandle2, p)) else -1
        bo1devAddr = p.pddr if not(xclGetBOProperties(handle, boHandle1, p)) else -1

        if bo2devAddr is -1 or bo1devAddr is -1:
            return 1

        # Allocate the exec_bo unsigned
        execHandle = xclAllocBO(handle, DATA_SIZE, xclBOKind.XCL_BO_SHARED_VIRTUAL.value, 1)

        # Get the output

        if xclSyncBO(handle, boHandle1, xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE.value, DATA_SIZE, False):
            bo2 = xclMapBO(handle, boHandle1, False)
            # return 1
            if len(bo1) == len(bo2) and all(x == y for x, y in zip(bo1,bo2)):
                print("FAILED TEST")
                print("Value read back does not match value written")
                # return 1

            # munmap(bo1, DATA_SIZE)
            # munmap(bo2, DATA_SIZE)
            # xclFreeBO(handle, boHandle1)
            # xclFreeBO(handle, boHandle2)
            # xclFreeBO(handle, execHandle)

    except Exception as exp:
        print("Exception: ")
        print(exp)  # prints the err
        sys.exit()

    print("PASSED TEST")


def printHelp():
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


if __name__ == "__main__":
    main()
