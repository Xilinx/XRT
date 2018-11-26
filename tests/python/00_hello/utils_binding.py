from ctypes import *
import sys
sys.path.append('../../../src/python/')
from xclbin_binding import *
from xclhal2_binding import *

libc = CDLL("../../../build/Debug/opt/xilinx/xrt/lib/libxrt_core.so")


def initXRT(bit, deviceIndex, halLog, handle, cu_index, cu_base_addr, first_mem_used):
    deviceInfo = xclDeviceInfo2()
    if deviceIndex >= xclProbe():
        print("Error")
        return -1
    
    handle = xclOpen(deviceIndex, halLog, xclVerbosityLevel.XCL_INFO)

    if xclGetDeviceInfo2(handle, byref(deviceInfo)):
        print("Error 2")
        return -1

    print("DSA = %s") % deviceInfo.mName
    print("Index = %s") % deviceIndex
    print("PCIe = GEN%s"+ " x %s") %(deviceInfo.mPCIeLinkSpeed, deviceInfo.mPCIeLinkWidth)
    print("OCL Frequency = %s MHz") % deviceInfo.mOCLFrequency[0]
    print("DDR Bank = %s") % deviceInfo.mDDRBankCount
    print("Device Temp = %s C") % deviceInfo.mOnChipTemp
    print("MIG Calibration = %s") % deviceInfo.mMigCalibs

    cu_base_addr = 0xffffffffffffffff  # long
    if not bit or not len(bit):
        return 0
    
    if xclLockDevice(handle):
        print("Cannot unlock device")
        sys.exit()
    
    # tempFileName = bit[:]
    # print(tempFileName)

    with open(tempFileName, "rb") as f:
        if f.read(8) == "xclbin2":
            print("Invalid Bitsream")
            sys.exit()
        f.seek(0)

        if xclLoadXclBin(handle, f.read()):
            print("Bitsream download failed")

        print("Finished downloading bitstream %s") % bit
        # 83
    return 0


