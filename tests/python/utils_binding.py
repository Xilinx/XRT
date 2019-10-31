##
 # Copyright (C) 2018 Xilinx, Inc
 # Author(s): Ryan Radjabi
 #            Shivangi Agarwal
 #            Sonal Santan
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
        self.index = 0
        self.cu_index = 0
        self.verbose = False
        self.handle = xclDeviceHandle
        self.first_mem = -1
        self.cu_base_addr = -1
        self.ert = False
        self.xuuid = uuid.uuid4()

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

    if xclGetDeviceInfo2(opt.handle, ctypes.byref(deviceInfo)):
        print("Error 2")
        return -1

    if sys.version_info[0] == 3:
        print("Shell = %s" % deviceInfo.mName)
        print("Index = %d" % opt.index)
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

    if not opt.bitstreamFile or not len(opt.bitstreamFile):
        print(opt.bitstreamFile)
        return 0

    if xclLockDevice(opt.handle):
        print("Cannot unlock device")
        sys.exit()

    tempFileName = opt.bitstreamFile

    with open(tempFileName, "rb") as f:
        data = bytearray(os.path.getsize(tempFileName))
        f.readinto(data)
        f.close()
        blob = (ctypes.c_char * len(data)).from_buffer(data)
        xbinary = axlf.from_buffer(data)
        if xbinary.m_magic.decode("utf-8") != "xclbin2":
            print("Invalid Bitsream")
            sys.exit()

        if xclLoadXclBin(opt.handle, blob):
            print("Bitsream download failed")

        xclLoadXclBin(opt.handle, blob)
        print("Finished downloading bitstream %s" % opt.bitstreamFile)
        myuuid = memoryview(xbinary.m_header.u2.uuid)[:]
        opt.xuuid = uuid.UUID(bytes=myuuid.tobytes())
        head = wrap_get_axlf_section(blob, AXLF_SECTION_KIND.IP_LAYOUT)
        layout = ip_layout.from_buffer(data, head.contents.m_sectionOffset)

        if opt.cu_index > layout.m_count:
            print("Can't determine cu base address")
            sys.exit()

        ip = (ip_data * layout.m_count).from_buffer(data, head.contents.m_sectionOffset + 8)

        for i in range(layout.m_count):
            if (ip[i].m_type != 1):
                continue
            opt.cu_base_addr = ip[i].ip_u1.m_base_address
            print("CU[%d] %s @0x%x" % (i, ctypes.cast(ip[i].m_name, ctypes.c_char_p).value, opt.cu_base_addr))

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
