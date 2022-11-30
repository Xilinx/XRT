##
 # Copyright (C) 2018-2020 Xilinx, Inc
 # Helper routines for Python based XRT tests
 #
 # Licensed under the Apache License, Version 2.0 (the "License"). You may
 # not use this file except in compliance with the License. A copy of the
 # License is located at
 #
 #     http://www.apache.org/licenses/LICENSE-2.0
 #
 # Unless required by applicable law or agreed to in writing, software
 # distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 # WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 # License for the specific language governing permissions and limitations
 # under the License.
##
import sys
import getopt
import struct
import ctypes
import uuid
# XRT modules imported from PYTHONPATH
from xclbin_binding import *
from xrt_binding import *
from ert_binding import *


class Options(object):
    def __init__(self):
        self.DATA_SIZE = 1024
        self.sharedLibrary = None
        self.bitstreamFile = None
        self.halLogFile = None
        self.alignment = 4096
        self.option_index = 0
        self.index = None
        self.cu_index = 0
        self.s_flag = False
        self.verbose = False
        self.handle = None
        self.xcl_handle = None
        self.first_mem = -1
        self.cu_base_addr = -1
        self.xuuid = uuid.uuid4()
        self.kernels = []

    def getOptions(self, argv, b_file):
        try:
            opts, args = getopt.getopt(argv[1:], "k:l:a:c:d:svhe", ["bitstream=", "hal_logfile=", "alignment=",
                                                                   "cu_index=", "device=", "supported", "verbose", "help", "ert"])
        except getopt.GetoptError:
            print(self.printHelp())
            sys.exit(2)

        for o, arg in opts:
            if o in ("--bitstream", "-k"):
                self.bitstreamFile = arg
            elif o in ("--hal_logfile", "-l"):
                self.halLogFile = arg
            elif o in ("--alignment", "-a"):
                print("-a/--alignment switch is not supported")
            elif o in ("--cu_index", "-c"):
                self.cu_index = int(arg)
            elif o in ("--supported", "-s"):
                self.s_flag = True
            elif o in ("--device", "-d"):
                self.index = arg
            elif o in ("--help", "-h"):
                print(self.printHelp())
            elif o == "-v":
                self.verbose = True
            elif o in ("-e", "--ert"):
                print("-e/--ert switch is not supported")
            else:
                assert False, "unhandled option"

        if self.bitstreamFile is None:
            raise RuntimeError("No bitstream specified")

        if self.halLogFile:
            print("Log files are not supported on command line, Please use xrt.ini to specify logging configuration")
        print("Host buffer alignment " + str(self.alignment) + " bytes")
        print("Compiled kernel = " + self.bitstreamFile)

        if(os.path.isfile(self.bitstreamFile)):
            tempfile = self.bitstreamFile
        else:
            tempfile = os.path.join(self.bitstreamFile, b_file)
        if self.s_flag:
            if os.path.isfile(tempfile):
                print("TEST SUPPORTED")
                sys.exit()
            else :
                print("TEST NOT SUPPORTED")
                sys.exit(1)

    def printHelp(self):
        print("usage: %s [options] -k <bitstream>")
        print("  -k <bitstream>")
        print("  -d <device_index>")
        print("  -c <cu_index>")
        print("  -s <test_support>")
        print("  -v")
        print("  -h")
        print("")
        print("* Bitstream is required")

def initXRT(opt):
    deviceInfo = xclDeviceInfo2()

    opt.handle = xrtDeviceOpenByBDF(opt.index)
    if opt.handle is None:
            raise RuntimeError("Invalid device BDF")

    opt.xcl_handle = xrtDeviceToXclDevice(opt.handle)

    xclGetDeviceInfo2(opt.xcl_handle, ctypes.byref(deviceInfo))

    if sys.version_info[0] == 3:
        print("Shell = %s" % deviceInfo.mName)
        print("Index = %s" % opt.index)
        print("PCIe = GEN%d x %d" % (deviceInfo.mPCIeLinkSpeed, deviceInfo.mPCIeLinkWidth))
        print("OCL Frequency = (%d, %d) MHz" % (deviceInfo.mOCLFrequency[0], deviceInfo.mOCLFrequency[1]))
        print("DDR Bank = %d" % deviceInfo.mDDRBankCount)
        print("Device Temp = %d C" % deviceInfo.mOnChipTemp)
        print("MIG Calibration = %s" % deviceInfo.mMigCalib)
    else:
        print("Shell = %s") % deviceInfo.mName
        print("Index = %s") % opt.index
        print("PCIe = GEN%s" + " x %s") % (deviceInfo.mPCIeLinkSpeed, deviceInfo.mPCIeLinkWidth)
        print("OCL Frequency = %s MHz") % deviceInfo.mOCLFrequency[0]
        print("DDR Bank = %d") % deviceInfo.mDDRBankCount
        print("Device Temp = %d C") % deviceInfo.mOnChipTemp
        print("MIG Calibration = %s") % deviceInfo.mMigCalib

    tempFileName = opt.bitstreamFile

    with open(tempFileName, "rb") as f:
        data = bytearray(os.path.getsize(tempFileName))
        f.readinto(data)
        f.close()
        blob = (ctypes.c_char * len(data)).from_buffer(data)
        xbinary = axlf.from_buffer(data)
        if xbinary.m_magic.decode("utf-8") != "xclbin2":
            raise RuntimeError("Invalid Bitsream")

        xclLoadXclBin(opt.xcl_handle, blob)
        print("Finished downloading bitstream %s" % opt.bitstreamFile)

        myuuid = memoryview(xbinary.m_header.u2.uuid)[:]
        opt.xuuid = uuid.UUID(bytes=myuuid.tobytes())
        head = wrap_get_axlf_section(blob, AXLF_SECTION_KIND.IP_LAYOUT)
        layout = ip_layout.from_buffer(data, head.contents.m_sectionOffset)

        if opt.cu_index > layout.m_count:
            raise RuntimeError("Can't determine cu base address")

        ip = (ip_data * layout.m_count).from_buffer(data, head.contents.m_sectionOffset + 8)

        for i in range(layout.m_count):
            if (ip[i].m_type != 1):
                continue
            opt.cu_base_addr = ip[i].ip_u1.m_base_address
            opt.kernels.append(ctypes.cast(ip[i].m_name, ctypes.c_char_p).value)
            print("CU[%d] %s @0x%x" % (i, opt.kernels[-1], opt.cu_base_addr))

        head = wrap_get_axlf_section(blob, AXLF_SECTION_KIND.MEM_TOPOLOGY)
        topo = mem_topology.from_buffer(data, head.contents.m_sectionOffset)
        mem = (mem_data * topo.m_count).from_buffer(data, head.contents.m_sectionOffset + 8)

        for i in range(topo.m_count):
            print("[%d] %s @0x%x" % (i, ctypes.cast(mem[i].m_tag, ctypes.c_char_p).value, mem[i].mem_u2.m_base_address))
            if (mem[i].m_used == 0):
                continue
            opt.first_mem = i
            break

    return 0
