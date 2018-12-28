##
 # Copyright (C) 2018 Xilinx, Inc
 # Author(s): Ryan Radjabi
 #            Shivangi Agarwal
 #            Sonal Santan
 # ctypes based Python binding for XRT
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
import os
import ctypes
import enum
from xclbin_binding import *

libc = ctypes.CDLL(os.environ['XILINX_XRT'] + "/lib/libxrt_core.so")

xclDeviceHandle = ctypes.c_void_p

class xclDeviceInfo2(ctypes.Structure):
    # "_fields_" is a required keyword
    _fields_ = [
     ("mMagic", ctypes.c_uint),
     ("mName", ctypes.c_char*256),
     ("mHALMajorVersion", ctypes.c_ushort),
     ("mHALMinorVersion", ctypes.c_ushort),
     ("mVendorId", ctypes.c_ushort),
     ("mDeviceId", ctypes.c_ushort),
     ("mSubsystemId", ctypes.c_ushort),
     ("mSubsystemVendorId", ctypes.c_ushort),
     ("mDeviceVersion", ctypes.c_ushort),
     ("mDDRSize", ctypes.c_size_t),
     ("mDataAlignment", ctypes.c_size_t),
     ("mDDRFreeSize", ctypes.c_size_t),
     ("mMinTransferSize", ctypes.c_size_t),
     ("mDDRBankCount", ctypes.c_ushort),
     ("mOCLFrequency", ctypes.c_ushort*4),
     ("mPCIeLinkWidth", ctypes.c_ushort),
     ("mPCIeLinkSpeed", ctypes.c_ushort),
     ("mDMAThreads", ctypes.c_ushort),
     ("mOnChipTemp", ctypes.c_short),
     ("mFanTemp", ctypes.c_short),
     ("mVInt", ctypes.c_ushort),
     ("mVAux", ctypes.c_ushort),
     ("mVBram", ctypes.c_ushort),
     ("mCurrent", ctypes.c_float),
     ("mNumClocks", ctypes.c_ushort),
     ("mFanSpeed", ctypes.c_ushort),
     ("mMigCalib", ctypes.c_bool),
     ("mXMCVersion", ctypes.c_ulonglong),
     ("mMBVersion", ctypes.c_ulonglong),
     ("m12VPex", ctypes.c_short),
     ("m12VAux", ctypes.c_short),
     ("mPexCurr", ctypes.c_ulonglong),
     ("mAuxCurr", ctypes.c_ulonglong),
     ("mFanRpm", ctypes.c_ushort),
     ("mDimmTemp", ctypes.c_ushort*4),
     ("mSE98Temp", ctypes.c_ushort*4),
     ("m3v3Pex", ctypes.c_ushort),
     ("m3v3Aux", ctypes.c_ushort),
     ("mDDRVppBottom",ctypes.c_ushort),
     ("mDDRVppTop", ctypes.c_ushort),
     ("mSys5v5", ctypes.c_ushort),
     ("m1v2Top", ctypes.c_ushort),
     ("m1v8Top", ctypes.c_ushort),
     ("m0v85", ctypes.c_ushort),
     ("mMgt0v9", ctypes.c_ushort),
     ("m12vSW", ctypes.c_ushort),
     ("mMgtVtt", ctypes.c_ushort),
     ("m1v2Bottom", ctypes.c_ushort),
     ("mDriverVersion, ", ctypes.c_ulonglong),
     ("mPciSlot", ctypes.c_uint),
     ("mIsXPR", ctypes.c_bool),
     ("mTimeStamp", ctypes.c_ulonglong),
     ("mFpga", ctypes.c_char*256),
     ("mPCIeLinkWidthMax", ctypes.c_ushort),
     ("mPCIeLinkSpeedMax", ctypes.c_ushort),
     ("mVccIntVol", ctypes.c_ushort),
     ("mVccIntCurr", ctypes.c_ushort),
     ("mNumCDMA", ctypes.c_ushort)
    ]


class xclMemoryDomains(enum.Enum):
    XCL_MEM_HOST_RAM = 0
    XCL_MEM_DEVICE_RAM = 1
    XCL_MEM_DEVICE_BRAM = 2
    XCL_MEM_SVM = 3
    XCL_MEM_CMA = 4
    XCL_MEM_DEVICE_REG = 5


class xclDDRFlags (enum.Enum):
    XCL_DEVICE_RAM_BANK0 = 0
    XCL_DEVICE_RAM_BANK1 = 2
    XCL_DEVICE_RAM_BANK2 = 4
    XCL_DEVICE_RAM_BANK3 = 8


class xclBOKind (enum.Enum):
    XCL_BO_SHARED_VIRTUAL = 0
    XCL_BO_SHARED_PHYSICAL = 1
    XCL_BO_MIRRORED_VIRTUAL = 2
    XCL_BO_DEVICE_RAM = 3
    XCL_BO_DEVICE_BRAM = 4
    XCL_BO_DEVICE_PREALLOCATED_BRAM = 5


class xclBOSyncDirection (enum.Enum):
    XCL_BO_SYNC_BO_TO_DEVICE = 0
    XCL_BO_SYNC_BO_FROM_DEVICE = 1


class xclAddressSpace (enum.Enum):
    XCL_ADDR_SPACE_DEVICE_FLAT = 0     # Absolute address space
    XCL_ADDR_SPACE_DEVICE_RAM = 1      # Address space for the DDR memory
    XCL_ADDR_KERNEL_CTRL = 2           # Address space for the OCL Region control port
    XCL_ADDR_SPACE_DEVICE_PERFMON = 3  # Address space for the Performance monitors
    XCL_ADDR_SPACE_DEVICE_CHECKER = 5  # Address space for protocol checker
    XCL_ADDR_SPACE_MAX = 8


class xclVerbosityLevel (enum.Enum):
    XCL_QUIET = 0
    XCL_INFO = 1
    XCL_WARN = 2
    XCL_ERROR = 3


class xclResetKind (enum.Enum):
    XCL_RESET_KERNEL = 0
    XCL_RESET_FULL = 1
    XCL_USER_RESET = 2


class xclDeviceUsage (ctypes.Structure):
    _fields_ = [
     ("h2c", ctypes.c_size_t*8),
     ("c2h", ctypes.c_size_t*8),
     ("ddeMemUsed", ctypes.c_size_t*8),
     ("ddrBOAllocated", ctypes.c_uint *8),
     ("totalContents", ctypes.c_uint),
     ("xclbinId", ctypes.c_ulonglong),
     ("dma_channel_cnt", ctypes.c_uint),
     ("mm_channel_cnt", ctypes.c_uint),
     ("memSize", ctypes.c_ulonglong*8)
    ]


class xclBOProperties (ctypes.Structure):
    _fields_ = [
     ("handle", ctypes.c_uint),
     ("flags" , ctypes.c_uint),
     ("size", ctypes.c_ulonglong),
     ("paddr", ctypes.c_ulonglong),
     ("domain", ctypes.c_uint),
    ]


def xclProbe():
    """
    xclProbe() - Enumerate devices found in the system
    :return: count of devices found
    """
    return libc.xclProbe()


def xclVersion():
    """
    :return: the version number. 1 => Hal1 ; 2 => Hal2
    """
    return libc.xclVersion()


def xclOpen(deviceIndex, logFileName, level):
    """
    xclOpen(): Open a device and obtain its handle

    :param deviceIndex: (unsigned int) Slot number of device 0 for first device, 1 for the second device...
    :param logFileName: (const char pointer) Log file to use for optional logging
    :param level: (int) Severity level of messages to log
    :return: device handle
    """
    libc.xclOpen.restype = ctypes.POINTER(xclDeviceHandle)
    libc.xclOpen.argtypes = [ctypes.c_uint, ctypes.c_char_p, ctypes.c_int]
    return libc.xclOpen(deviceIndex, logFileName, level.value)


def xclClose(handle):
    """
    xclClose(): Close an opened device

    :param handle: (xclDeviceHandle) device handle
    :return: None
    """
    libc.xclClose.restype = None
    libc.xclClose.argtype = xclDeviceHandle
    libc.xclClose(handle)


def xclResetDevice(handle, kind):
    """
    xclResetDevice() - Reset a device or its CL
    :param handle: Device handle
    :param kind: Reset kind
    :return: 0 on success or appropriate error number
    """
    libc.xclResetDevice.restype = ctypes.c_int
    libc.xclResetDevice.argtypes = [xclDeviceHandle, ctypes.c_int]
    libc.xclResetDevice(handle, kind)


def xclGetDeviceInfo2 (handle, info):
    """
    xclGetDeviceInfo2() - Obtain various bits of information from the device

    :param handle: (xclDeviceHandle) device handle
    :param info: (xclDeviceInfo pointer) Information record
    :return: 0 on success or appropriate error number
    """

    libc.xclGetDeviceInfo2.restype = ctypes.c_int
    libc.xclGetDeviceInfo2.argtypes = [xclDeviceHandle, ctypes.POINTER(xclDeviceInfo2)]
    return libc.xclGetDeviceInfo2(handle, info)


def xclGetUsageInfo (handle, info):
    """
    xclGetUsageInfo() - Obtain usage information from the device
    :param handle: Device handle
    :param info: Information record
    :return: 0 on success or appropriate error number
    """
    libc.xclGetUsageInfo.restype = ctypes.c_int
    libc.xclGetUsageInfo.argtypes = [xclDeviceHandle, ctypes.POINTER(xclDeviceInfo2)]
    return libc.xclGetUsageInfo(handle, info)


def xclGetErrorStatus(handle, info):
    """
    xclGetErrorStatus() - Obtain error information from the device
    :param handle: Device handle
    :param info: Information record
    :return: 0 on success or appropriate error number
    """
    libc.xclGetErrorStatus.restype = ctypes.c_int
    libc.xclGetErrorStatus.argtypes = [xclDeviceHandle, ctypes.POINTER(xclDeviceInfo2)]
    return libc.xclGetErrorStatus(handle, info)


def xclLoadXclBin(handle, buf):
    """
    Download FPGA image (xclbin) to the device

    :param handle: (xclDeviceHandle) device handle
    :param buf: (void pointer) Pointer to device image (xclbin) in memory
    :return: 0 on success or appropriate error number

    Download FPGA image (AXLF) to the device. The PR bitstream is encapsulated inside
    xclbin as a section. xclbin may also contains other sections which are suitably
    handled by the driver
    """
    libc.xclLoadXclBin.restype = ctypes.c_int
    libc.xclLoadXclBin.argtypes = [xclDeviceHandle, ctypes.c_void_p]
    return libc.xclLoadXclBin(handle, buf)


def xclGetSectionInfo(handle, info, size, kind, index):
    """
    xclGetSectionInfo() - Get Information from sysfs about the downloaded xclbin sections
    :param handle: Device handle
    :param info: Pointer to preallocated memory which will store the return value.
    :param size: Pointer to preallocated memory which will store the return size.
    :param kind: axlf_section_kind for which info is being queried
    :param index: The (sub)section index for the "kind" type.
    :return: 0 on success or appropriate error number
    """
    libc.xclGetSectionInfo.restype = ctypes.c_int
    libc.xclGetSectionInfo.argtypes = [xclDeviceHandle, ctypes.POINTER(xclDeviceInfo2), ctypes.POINTER(sizeof(xclDeviceInfo2)),
                                       ctypes.c_int, ctypes.c_int]
    return libc.xclGetSectionInfo(handle, info, size, kind, index)


def xclReClock2(handle, region, targetFreqMHz):
    """
    xclReClock2() - Configure PR region frequencies
    :param handle: Device handle
    :param region: PR region (always 0)
    :param targetFreqMHz: Array of target frequencies in order for the Clock Wizards driving the PR region
    :return: 0 on success or appropriate error number
    """
    libc.xclReClock2.restype = ctypes.c_int
    libc.xclReClock2.argtypes = [xclDeviceHandle, ctypes.c_uint, ctypes.c_uint]
    return libc.xclReClock2(handle, region, targetFreqMHz)


def xclLockDevice(handle):
    """
    Get exclusive ownership of the device

    :param handle: (xclDeviceHandle) device handle
    :return: 0 on success or appropriate error number

    The lock is necessary before performing buffer migration, register access or bitstream downloads
    """
    libc.xclLockDevice.restype = ctypes.c_int
    libc.xclLockDevice.argtype = xclDeviceHandle
    return libc.xclLockDevice(handle)


def xclUnlockDevice(handle):
    """
    xclUnlockDevice() - Release exclusive ownership of the device

    :param handle: (xclDeviceHandle) device handle
    :return: 0 on success or appropriate error number
    """
    libc.xclUnlockDevice.restype = ctypes.c_int
    libc.xclUnlockDevice.argtype = xclDeviceHandle
    return libc.xclUnlockDevice(handle)


def xclOpenContext(handle, xclbinId, ipIndex, shared):
    """
    xclOpenContext() - Create shared/exclusive context on compute units
    :param handle: Device handle
    :param xclbinId: UUID of the xclbin image running on the device
    :param ipIndex: IP/CU index in the IP LAYOUT array
    :param shared: Shared access or exclusive access
    :return: 0 on success or appropriate error number

    The context is necessary before submitting execution jobs using xclExecBuf(). Contexts may be
    exclusive or shared. Allocation of exclusive contexts on a compute unit would succeed
    only if another client has not already setup up a context on that compute unit. Shared
    contexts can be concurrently allocated by many processes on the same compute units.
    """
    libc.xclOpenContext.restype = ctypes.c_int
    libc.xclOpenContext.argtypes = [xclDeviceHandle, ctypes.c_char_p, ctypes.c_uint, ctypes.c_bool]
    return libc.xclOpenContext(handle, xclbinId.bytes, ipIndex, shared)


def xclCloseContext(handle, xclbinId, ipIndex):
    """
    xclCloseContext() - Close previously opened context
    :param handle: Device handle
    :param xclbinId: UUID of the xclbin image running on the device
    :param ipIndex: IP/CU index in the IP LAYOUT array
    :return: 0 on success or appropriate error number

    Close a previously allocated shared/exclusive context for a compute unit.
    """
    libc.xclCloseContext.restype = ctypes.c_int
    libc.xclCloseContext.argtypes = [xclDeviceHandle, ctypes.c_char_p, ctypes.c_uint]
    return libc.xclCloseContext(handle, xclbinId.bytes, ipIndex)


def xclUpgradeFirmware(handle, fileName):
    """
    Update the device BPI PROM with new image
    :param handle: Device handle
    :param fileName:
    :return: 0 on success or appropriate error number
    """
    libc.xclUpgradeFirmware.restype = ctypes.c_int
    libc.xclUpgradeFirmware.argtypes = [xclDeviceHandle, ctypes.c_void_p]
    return libc.xclUpgradeFirmware(handle, fileName)


def xclUpgradeFirmware2(handle, file1, file2):
    """
    Update the device BPI PROM with new image with clearing bitstream
    :param handle: Device handle
    :param fileName:
    :return: 0 on success or appropriate error number
    """
    libc.xclUpgradeFirmware2.restype = ctypes.c_int
    libc.xclUpgradeFirmware2.argtypes = [xclDeviceHandle, ctypes.c_void_p, ctypes.c_void_p]
    return libc.xclUpgradeFirmware2(handle, file1, file2)


def xclAllocBO(handle, size, domain, flags):
    """
    Allocate a BO of requested size with appropriate flags

    :param handle: (xclDeviceHandle) device handle
    :param size: (size_t) Size of buffer
    :param domain: (xclBOKind) Memory domain
    :param flags: (unsigned int) Specify bank information, etc
    :return: BO handle
    """
    libc.xclAllocBO.restype = ctypes.c_uint
    libc.xclAllocBO.argtypes = [xclDeviceHandle, ctypes.c_size_t, ctypes.c_int, ctypes.c_uint]
    return libc.xclAllocBO(handle, size, domain.value, flags)

def xclFreeBO(handle, boHandle):
    """
    Free a previously allocated BO

    :param handle: device handle
    :param boHandle: BO handle
    """
    libc.xclFreeBO.restype = None
    libc.xclFreeBO.argtypes = [xclDeviceHandle, ctypes.c_uint]
    libc.xclFreeBO(handle, boHandle)

def xclMapBO(handle, boHandle, write):
    """
    Memory map BO into user's address space

    :param handle: (xclDeviceHandle) device handle
    :param boHandle: (unsigned int) BO handle
    :param write: (boolean) READ only or READ/WRITE mapping
    :return: (void pointer) Memory mapped buffer

    Map the contents of the buffer object into host memory
    To unmap the buffer call POSIX unmap() on mapped void * pointer returned from xclMapBO
    """
    prop = xclBOProperties()
    xclGetBOProperties(handle, boHandle, prop)
    libc.xclMapBO.restype = ctypes.POINTER(ctypes.c_char * prop.size)
    libc.xclMapBO.argtypes = [xclDeviceHandle, ctypes.c_uint, ctypes.c_bool]
    ptr = libc.xclMapBO(handle, boHandle, write)
    return ptr


def xclSyncBO(handle, boHandle, direction, size, offset):
    """
    Synchronize buffer contents in requested direction

    :param handle: (xclDeviceHandle) device handle
    :param boHandle: (unsigned int) BO handle
    :param direction: (xclBOSyncDirection) To device or from device
    :param size: (size_t) Size of data to synchronize
    :param offset: (size_t) Offset within the BO
    :return: 0 on success or standard errno
    """
    libc.xclSyncBO.restype = ctypes.c_uint
    libc.xclSyncBO.argtypes = [xclDeviceHandle, ctypes.c_uint, ctypes.c_int, ctypes.c_size_t, ctypes.c_size_t]
    return libc.xclSyncBO(handle, boHandle, direction.value, size, offset)


def xclGetBOProperties(handle, boHandle, properties):
    """
    Obtain xclBOProperties struct for a BO

    :param handle: (xclDeviceHandle) device handle
    :param boHandle: (unsigned int) BO handle
    :param properties: BO properties struct pointer
    :return: 0 on success
    """
    libc.xclGetBOProperties.restype = ctypes.c_int
    libc.xclGetBOProperties.argtypes = [xclDeviceHandle, ctypes.c_uint, ctypes.POINTER(xclBOProperties)]
    return libc.xclGetBOProperties(handle, boHandle, properties)


def xclExecBuf(handle, cmdBO):
    """
    xclExecBuf() - Submit an execution request to the embedded (or software) scheduler
    :param handle: Device handle
    :param cmdBO: BO handle containing command packet
    :return: 0 or standard error number

    Submit an exec buffer for execution. The exec buffer layout is defined by struct ert_packet
    which is defined in file *ert.h*. The BO should been allocated with DRM_XOCL_BO_EXECBUF flag.
    """
    libc.xclExecBuf.restype = ctypes.c_int
    libc.xclExecBuf.argtypes = [xclDeviceHandle, ctypes.c_uint]
    return libc.xclExecBuf(handle, cmdBO)


def xclExecWait(handle, timeoutMilliSec):
    """
    xclExecWait() - Wait for one or more execution events on the device
    :param handle: Device handle
    :param timeoutMilliSec: How long to wait for
    :return:  Same code as poll system call

    Wait for notification from the hardware. The function essentially calls "poll" system
    call on the driver file handle. The return value has same semantics as poll system call.
    If return value is > 0 caller should check the status of submitted exec buffers
    """
    libc.xclExecWait.restype = ctypes.c_size_t
    libc.xclExecWait.argtypes = [xclDeviceHandle, ctypes.c_int]
    return libc.xclExecWait(handle, timeoutMilliSec)

def xclReadBO(handle, boHandle, dst, size, skip):
    libc.xclReadBO.restype = ctypes.c_int
    libc.xclReadBO.argtypes = [xclDeviceHandle, ctypes.c_uint, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_size_t]
    return libc.xclReadBO(handle, boHandle, dst, size, skip)
