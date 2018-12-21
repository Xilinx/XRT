from ctypes import *
import sys, getopt, struct
# import source files
sys.path.append('../../../src/python/')
from xclbin_binding import *
from xrt_binding import *
from ert_binding import *


class Options(object):
    def __init__(self):
        self.DATA_SIZE = 1024
        self.sharedLibrary = "None"
        self.bitstreamFile = None
        self.halLogFile = ""
        self.alignment = 128
        self.option_index = 0
        self.index = 0
        self.cu_index = 0
        self.verbose = False
        self.handle = xclDeviceHandle
        self.first_mem = -1
        self.cu_base_addr = -1
        self.ert = False

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
                self.ert = bool(arg)
            else:
                assert False, "unhandled option"

        if self.bitstreamFile is None:
            print("FAILED TEST" + "\n" + "No bitstream specified")
            sys.exit()

        if self.halLogFile:
            print("Using " + self.halLogFile + " as HAL driver logfile")
        print("HAL driver = " + self.sharedLibrary)
        print("Host buffer alignment " + str(self.alignment) + " bytes")
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


def initXRT(opt):
    deviceInfo = xclDeviceInfo2()
    if opt.index >= xclProbe():
        print("Error")
        return -1

    opt.handle = xclOpen(opt.index, opt.halLogFile, xclVerbosityLevel.XCL_INFO)

    if xclGetDeviceInfo2(opt.handle, byref(deviceInfo)):
        print("Error 2")
        return -1

    print("DSA = %s") % deviceInfo.mName
    print("Index = %s") % opt.index
    print("PCIe = GEN%s" + " x %s") % (deviceInfo.mPCIeLinkSpeed, deviceInfo.mPCIeLinkWidth)
    print("OCL Frequency = %s MHz") % deviceInfo.mOCLFrequency[0]
    print("DDR Bank = %s") % deviceInfo.mDDRBankCount
    print("Device Temp = %s C") % deviceInfo.mOnChipTemp
    print("MIG Calibration = %s") % deviceInfo.mMigCalib

    if not opt.bitstreamFile or not len(opt.bitstreamFile):
        print(opt.bitstreamFile)
        return 0

    if xclLockDevice(opt.handle):
        print("Cannot unlock device")
        sys.exit()

    tempFileName = opt.bitstreamFile

    with open(tempFileName, "rb") as f:
        header = f.read(7)
        if header != "xclbin2":
            print("Invalid Bitsream")
            sys.exit()
        f.seek(0)
        blob = f.read()

        if xclLoadXclBin(opt.handle, blob):
            print("Bitsream download failed")

        xclLoadXclBin(opt.handle, blob)
        print("Finished downloading bitstream %s") % opt.bitstreamFile

        f.seek(0)
        top = f.read()
        ip = wrap_get_axlf_section(top, AXLF_SECTION_KIND.IP_LAYOUT)

        f.seek(ip.contents.m_sectionOffset)
        count = int(f.read(1).encode("hex"))

        if opt.cu_index > count:
            print("Can't determine cu base address")
            sys.exit()

        f.seek(ip.contents.m_sectionOffset + sizeof(c_int32) + sizeof(c_int32))
        # unpack sequence: '=1I1I1Q64s' = 1 uint32 1 uint32 1 uint32 1 uint64 64 char
        struct_fmt = '=1I1I1Q64s'
        struct_len = struct.calcsize(struct_fmt)
        struct_unpack = struct.Struct(struct_fmt).unpack_from

        for i in range(count):
            s = struct_unpack(f.read(struct_len))
            if s[0] == 1:
                opt.cu_base_addr = s[2]
                print("base address %s") % hex(s[2])

        topo = wrap_get_axlf_section(top, AXLF_SECTION_KIND.MEM_TOPOLOGY)
        f.seek(topo.contents.m_sectionOffset)
        topo_count = int(f.read(1).encode("hex"))

        f.seek(topo.contents.m_sectionOffset + sizeof(c_int32) + sizeof(c_int32))
        # unpack sequence: '=1b1b6b1Q1Q16s' = 1 byte 1 byte 6 bytes 1 uint64 1 uint64 6 char
        struct_fmt = '=1b1b6b1Q1Q16s'
        struct_len = struct.calcsize(struct_fmt)
        struct_unpack = struct.Struct(struct_fmt).unpack_from

        for i in range(topo_count):
            s = struct_unpack(f.read(struct_len))

            if s[1] == 1:
                opt.first_mem = i
                break

    return 0
