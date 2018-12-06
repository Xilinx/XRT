from ctypes import *
import sys
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
        print("\n<-------------DEBUG------------>\n")
        f.seek(0)
        top = f.read()
        ip = wrap_get_axlf_section(top, AXLF_SECTION_KIND.IP_LAYOUT)
        print("ip.contents.m_sectionOffset: %d") % ip.contents.m_sectionOffset  # correct
        layout = ip_layout(8 + ip.contents.m_sectionOffset)  # size of header = 8 not sure about this
        print("Layout->m_count: %d (Expected 5)") % layout.m_count  # should give 5

        if opt.cu_index > layout.m_count:
            print("Can't determine cu base address")
            sys.exit()

        # cur_index = 0
        # for i in range(layout.m_count):
        #     if layout.m_ip_data[i].m_type != 1:
        #         continue
        #     if cur_index == opt.cu_index:
        #         cu_base_addr = layout.m_ip_data[i].m_base_address
        #         print("base address %d") % cu_base_addr # sould be 25165824 or 1800000

        topo = wrap_get_axlf_section(top, AXLF_SECTION_KIND.MEM_TOPOLOGY)
        print("topo.contents.m_sectionOffset: %d") % topo.contents.m_sectionOffset  # correct
        topology = mem_topology(8 + topo.contents.m_sectionOffset)  # size of header = 8
        print("topology->m_count: %d (Expected 7)") % topology.m_count
        #
        #
        # for i in range(topology.m_count):
        #     if topology.m_mem_data[i].m_used:
        #         first_used_mem = i
        #         break
        print("\n<-------------DEBUG END------------>\n")

    # <----------HARDCODED VALUES---------->
    opt.first_mem = 1
    opt.cu_base_addr = 25165824  # this isn't used anywhere in the main function
    print("base address %i") % opt.cu_base_addr

    return 0


