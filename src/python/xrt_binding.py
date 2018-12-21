from ctypes import *
from enum import *
from xclbin_binding import *
import os
libc = CDLL(os.environ['XILINX_XRT'] + "/lib/libxrt_core.so")

xclDeviceHandle = c_void_p


class xclDeviceInfo2(Structure):
    # "_fields_" is a required keyword
    _fields_ = [
     ("mMagic", c_uint),
     ("mName", c_char*256),
     ("mHALMajorVersion", c_ushort),
     ("mHALMinorVersion", c_ushort),
     ("mVendorId", c_ushort),
     ("mDeviceId", c_ushort),
     ("mSubsystemId", c_ushort),
     ("mSubsystemVendorId", c_ushort),
     ("mDeviceVersion", c_ushort),
     ("mDDRSize", c_size_t),
     ("mDataAlignment", c_size_t),
     ("mDDRFreeSize", c_size_t),
     ("mMinTransferSize", c_size_t),
     ("mDDRBankCount", c_ushort),
     ("mOCLFrequency", c_ushort*4),
     ("mPCIeLinkWidth", c_ushort),
     ("mPCIeLinkSpeed", c_ushort),
     ("mDMAThreads", c_ushort),
     ("mOnChipTemp", c_short),
     ("mFanTemp", c_short),
     ("mVInt", c_ushort),
     ("mVAux", c_ushort),
     ("mVBram", c_ushort),
     ("mCurrent", c_float),
     ("mNumClocks", c_ushort),
     ("mFanSpeed", c_ushort),
     ("mMigCalib", c_bool),
     ("mXMCVersion", c_ulonglong),
     ("mMBVersion", c_ulonglong),
     ("m12VPex", c_short),
     ("m12VAux", c_short),
     ("mPexCurr", c_ulonglong),
     ("mAuxCurr", c_ulonglong),
     ("mFanRpm", c_ushort),
     ("mDimmTemp", c_ushort*4),
     ("mSE98Temp", c_ushort*4),
     ("m3v3Pex", c_ushort),
     ("m3v3Aux", c_ushort),
     ("mDDRVppBottom",c_ushort),
     ("mDDRVppTop", c_ushort),
     ("mSys5v5", c_ushort),
     ("m1v2Top", c_ushort),
     ("m1v8Top", c_ushort),
     ("m0v85", c_ushort),
     ("mMgt0v9", c_ushort),
     ("m12vSW", c_ushort),
     ("mMgtVtt", c_ushort),
     ("m1v2Bottom", c_ushort),
     ("mDriverVersion, ", c_ulonglong),
     ("mPciSlot", c_uint),
     ("mIsXPR", c_bool),
     ("mTimeStamp", c_ulonglong),
     ("mFpga", c_char*256),
     ("mPCIeLinkWidthMax", c_ushort),
     ("mPCIeLinkSpeedMax", c_ushort),
     ("mVccIntVol", c_ushort),
     ("mVccIntCurr", c_ushort),
     ("mNumCDMA", c_ushort)
    ]


class xclMemoryDomains(Enum):
    XCL_MEM_HOST_RAM = 0
    XCL_MEM_DEVICE_RAM = 1
    XCL_MEM_DEVICE_BRAM = 2
    XCL_MEM_SVM = 3
    XCL_MEM_CMA = 4
    XCL_MEM_DEVICE_REG = 5


class xclDDRFlags (Enum):
    XCL_DEVICE_RAM_BANK0 = 0
    XCL_DEVICE_RAM_BANK1 = 2
    XCL_DEVICE_RAM_BANK2 = 4
    XCL_DEVICE_RAM_BANK3 = 8


class xclBOKind (Enum):
    XCL_BO_SHARED_VIRTUAL = 0
    XCL_BO_SHARED_PHYSICAL = 1
    XCL_BO_MIRRORED_VIRTUAL = 2
    XCL_BO_DEVICE_RAM = 3
    XCL_BO_DEVICE_BRAM = 4
    XCL_BO_DEVICE_PREALLOCATED_BRAM = 5


class xclBOSyncDirection (Enum):
    XCL_BO_SYNC_BO_TO_DEVICE = 0
    XCL_BO_SYNC_BO_FROM_DEVICE = 1


class xclAddressSpace (Enum):
    XCL_ADDR_SPACE_DEVICE_FLAT = 0     # Absolute address space
    XCL_ADDR_SPACE_DEVICE_RAM = 1      # Address space for the DDR memory
    XCL_ADDR_KERNEL_CTRL = 2           # Address space for the OCL Region control port
    XCL_ADDR_SPACE_DEVICE_PERFMON = 3  # Address space for the Performance monitors
    XCL_ADDR_SPACE_DEVICE_CHECKER = 5  # Address space for protocol checker
    XCL_ADDR_SPACE_MAX = 8


class xclVerbosityLevel (Enum):
    XCL_QUIET = 0
    XCL_INFO = 1
    XCL_WARN = 2
    XCL_ERROR = 3


class xclResetKind (Enum):
    XCL_RESET_KERNEL = 0
    XCL_RESET_FULL = 1
    XCL_USER_RESET = 2


class xclDeviceUsage (Structure):
    _fields_ = [
     ("h2c", c_size_t*8),
     ("c2h", c_size_t*8),
     ("ddeMemUsed", c_size_t*8),
     ("ddrBOAllocated", c_uint *8),
     ("totalContents", c_uint),
     ("xclbinId", c_ulonglong),
     ("dma_channel_cnt", c_uint),
     ("mm_channel_cnt", c_uint),
     ("memSize", c_ulonglong*8)
    ]


class xclBOProperties (Structure):
    _fields_ = [
     ("handle", c_uint),
     ("flags" , c_uint),
     ("size", c_ulonglong),
     ("paddr", c_ulonglong),
     ("domain", c_uint),
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
    libc.xclOpen.restype = POINTER(xclDeviceHandle)
    libc.xclOpen.argtypes = [c_uint, c_char_p, c_int]
    return libc.xclOpen(deviceIndex, logFileName, level)


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
    libc.xclResetDevice.restype = c_int
    libc.xclResetDevice.argtypes = [xclDeviceHandle, c_int]
    libc.xclResetDevice(handle, kind)


def xclGetDeviceInfo2 (handle, info):
    """
    xclGetDeviceInfo2() - Obtain various bits of information from the device

    :param handle: (xclDeviceHandle) device handle
    :param info: (xclDeviceInfo pointer) Information record
    :return: 0 on success or appropriate error number
    """

    libc.xclGetDeviceInfo2.restype = c_int
    libc.xclGetDeviceInfo2.argtypes = [xclDeviceHandle, POINTER(xclDeviceInfo2)]
    return libc.xclGetDeviceInfo2(handle, info)


def xclGetUsageInfo (handle, info):
    """
    xclGetUsageInfo() - Obtain usage information from the device
    :param handle: Device handle
    :param info: Information record
    :return: 0 on success or appropriate error number
    """
    libc.xclGetUsageInfo.restype = c_int
    libc.xclGetUsageInfo.argtypes = [xclDeviceHandle, POINTER(xclDeviceInfo2)]
    return libc.xclGetUsageInfo(handle, info)


def xclGetErrorStatus(handle, info):
    """
    xclGetErrorStatus() - Obtain error information from the device
    :param handle: Device handle
    :param info: Information record
    :return: 0 on success or appropriate error number
    """
    libc.xclGetErrorStatus.restype = c_int
    libc.xclGetErrorStatus.argtypes = [xclDeviceHandle, POINTER(xclDeviceInfo2)]
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
    libc.xclLoadXclBin.restype = c_int
    libc.xclLoadXclBin.argtypes = [xclDeviceHandle, c_void_p]
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
    libc.xclGetSectionInfo.restype = c_int
    libc.xclGetSectionInfo.argtypes = [xclDeviceHandle, POINTER(xclDeviceInfo2), POINTER(sizeof(xclDeviceInfo2)),
                                       c_int, c_int]
    return libc.xclGetSectionInfo(handle, info, size, kind, index)


def xclReClock2(handle, region, targetFreqMHz):
    """
    xclReClock2() - Configure PR region frequencies
    :param handle: Device handle
    :param region: PR region (always 0)
    :param targetFreqMHz: Array of target frequencies in order for the Clock Wizards driving the PR region
    :return: 0 on success or appropriate error number
    """
    libc.xclReClock2.restype = c_int
    libc.xclReClock2.argtypes = [xclDeviceHandle, c_uint, c_uint]
    return libc.xclReClock2(handle, region, targetFreqMHz)


def xclLockDevice(handle):
    """
    Get exclusive ownership of the device

    :param handle: (xclDeviceHandle) device handle
    :return: 0 on success or appropriate error number

    The lock is necessary before performing buffer migration, register access or bitstream downloads
    """
    libc.xclLockDevice.restype = c_int
    libc.xclLockDevice.argtype = xclDeviceHandle
    return libc.xclLockDevice(handle)


def xclUnlockDevice(handle):
    """
    xclUnlockDevice() - Release exclusive ownership of the device

    :param handle: (xclDeviceHandle) device handle
    :return: 0 on success or appropriate error number
    """
    libc.xclUnlockDevice.restype = c_int
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
    libc.xclOpenContext.restype = c_int
    libc.xclOpenContext.argtypes = [xclDeviceHandle, c_uint, c_uint, c_bool]
    return libc.xclOpenContext(handle, xclbinId, ipIndex, shared)


def xclCloseContext(handle, xclbinId, ipIndex):
    """
    xclCloseContext() - Close previously opened context
    :param handle: Device handle
    :param xclbinId: UUID of the xclbin image running on the device
    :param ipIndex: IP/CU index in the IP LAYOUT array
    :return: 0 on success or appropriate error number

    Close a previously allocated shared/exclusive context for a compute unit.
    """
    libc.xclCloseContext.restype = c_int
    libc.xclCloseContext.argtypes = [xclDeviceHandle, c_uint, c_uint]
    return libc.xclCloseContext(handle, xclbinId, ipIndex)


def xclUpgradeFirmware(handle, fileName):
    """
    Update the device BPI PROM with new image
    :param handle: Device handle
    :param fileName:
    :return: 0 on success or appropriate error number
    """
    libc.xclUpgradeFirmware.restype = c_int
    libc.xclUpgradeFirmware.argtypes = [xclDeviceHandle, c_void_p]
    return libc.xclUpgradeFirmware(handle, fileName)


def xclUpgradeFirmware2(handle, file1, file2):
    """
    Update the device BPI PROM with new image with clearing bitstream
    :param handle: Device handle
    :param fileName:
    :return: 0 on success or appropriate error number
    """
    libc.xclUpgradeFirmware2.restype = c_int
    libc.xclUpgradeFirmware2.argtypes = [xclDeviceHandle, c_void_p, c_void_p]
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
    libc.xclAllocBO.restype = c_uint
    libc.xclAllocBO.argtypes = [xclDeviceHandle, c_size_t, c_int, c_uint]
    return libc.xclAllocBO(handle, size, domain, flags)


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
    libc.xclMapBO.restype = c_void_p
    libc.xclMapBO.argtypes = [xclDeviceHandle, c_uint, c_bool]
    return libc.xclMapBO(handle, boHandle, write)


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
    libc.xclSyncBO.restype = c_uint
    libc.xclSyncBO.argtypes = [xclDeviceHandle, c_uint, c_int, c_size_t, c_size_t]
    return libc.xclSyncBO(handle, boHandle, direction, size, offset)


def xclGetBOProperties(handle, boHandle, properties):
    """
    Obtain xclBOProperties struct for a BO

    :param handle: (xclDeviceHandle) device handle
    :param boHandle: (unsigned int) BO handle
    :param properties: BO properties struct pointer
    :return: 0 on success
    """
    libc.xclGetBOProperties.restype = c_int
    libc.xclGetBOProperties.argtypes = [xclDeviceHandle, c_uint, POINTER(xclBOProperties)]
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
    libc.xclExecBuf.restype = c_int
    libc.xclExecBuf.argtypes = [xclDeviceHandle, c_uint]
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
    libc.xclExecWait.restype = c_size_t
    libc.xclExecWait.argtypes = [xclDeviceHandle, c_int]
    return libc.xclExecWait(handle, timeoutMilliSec)

def xclReadBO(handle, boHandle, dst, size, skip):
    libc.xclReadBO.restype = c_int
    libc.xclReadBO.argtypes = [xclDeviceHandle, c_uint, c_void_p, c_size_t, c_size_t]
    return libc.xclReadBO(handle, boHandle, dst, size, skip)

