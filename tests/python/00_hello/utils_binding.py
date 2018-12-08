from ctypes import *
import sys
import struct
sys.path.append('../../../src/python/')
from xclbin_binding import *
from xclhal2_binding import *


libc = CDLL("../../../build/Debug/opt/xilinx/xrt/lib/libxrt_core.so")

handle = xclDeviceHandle


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
        blob = f.read()
        if not xclLoadXclBin(opt.handle, blob):
            print("Bitsream download failed")

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
        struct_fmt = '=1I1I1Q64s'
        struct_len = struct.calcsize(struct_fmt)
        struct_unpack = struct.Struct(struct_fmt).unpack_from

        for i in range(count):
            temp = f.read(struct_len)
            s = struct_unpack(temp)
            if s[0] == 1:
                opt.cu_base_addr = s[2]
                print("base address %s") % hex(s[2])

        topo = wrap_get_axlf_section(top, AXLF_SECTION_KIND.MEM_TOPOLOGY)
        f.seek(topo.contents.m_sectionOffset)
        topo_count = int(f.read(1).encode("hex"))
        #print(topo_count)


        f.seek(topo.contents.m_sectionOffset + sizeof(c_int32)+ sizeof(c_int32))
        struct_fmt = '=1b1b6b1Q1Q16s'
        struct_len = struct.calcsize(struct_fmt)
        #print(struct_len, sizeof(mem_data))
        struct_unpack = struct.Struct(struct_fmt).unpack_from

        for i in range(topo_count):
            temp = f.read(40)
        # for b in range(struct_len):
        #     print(ord(f.read(1)))
            s = struct_unpack(temp)

            if s[1] == 1:
                opt.first_mem = i
                break
    # <----------HARDCODED VALUES---------->
    #opt.first_mem = 1
    #opt.cu_base_addr = 25165824  # this isn't used anywhere in the main function

    return 0


