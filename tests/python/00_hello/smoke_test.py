from ctypes import *
import sys

sys.path.append('../../../src/python/')
from xclhal2_binding import *


dev_0_info = xclDeviceInfo2()  # Allocate an instance of the struct to store device info
print(type(dev_0_info))

print("xclProbe %s") %xclProbe()
print("xclVersion %d") %xclVersion()
dev_0 = xclOpen(0,"dev_log.txt", 1)
print("xclOpen %s") % dev_0
d = xclGetDeviceInfo2(dev_0, byref(dev_0_info))
print("xclGetDeviceInfo2 %d") % d
print("DSA = %s") % dev_0_info.mMagic
print("OCL Frequency = %s MHz") % dev_0_info.mOCLFrequency[0]

f = open("kernel_u200.xclbin", "rb")
print("xclLoadXclBin %d") %xclLoadXclBin(dev_0, f.read())
print("xclLockDevice %d") %xclLockDevice(dev_0)
boHandle = xclAllocBO(dev_0, 1024, xclBOKind.XCL_BO_DEVICE_RAM, -1)

print("xclAllocBO %d") %boHandle
print("xclMapBO %s") %xclMapBO(dev_0, boHandle, True)
print("xclSyncBO %d") %xclSyncBO(dev_0, boHandle, xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE, 1024, 0)
p = xclBOProperties()
# print("xclGetBOProperties %d") %xclGetBOProperties(dev_0, boHandle, byref(p))
xclClose(dev_0)