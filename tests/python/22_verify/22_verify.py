import sys
import uuid
from xrt_binding import * # found in PYTHONPATH
from ert_binding import * # found in PYTHONPATH
sys.path.append('../') # utils_binding.py
from utils_binding import *

XHELLO_HELLO_CONTROL_ADDR_AP_CTRL = 0x00
XHELLO_HELLO_CONTROL_ADDR_GIE = 0x04
XHELLO_HELLO_CONTROL_ADDR_IER = 0x08
XHELLO_HELLO_CONTROL_ADDR_ISR = 0x0c
XHELLO_HELLO_CONTROL_ADDR_GROUP_ID_X_DATA = 0x10
XHELLO_HELLO_CONTROL_BITS_GROUP_ID_X_DATA = 32
XHELLO_HELLO_CONTROL_ADDR_GROUP_ID_Y_DATA = 0x18
XHELLO_HELLO_CONTROL_BITS_GROUP_ID_Y_DATA = 32
XHELLO_HELLO_CONTROL_ADDR_GROUP_ID_Z_DATA = 0x20
XHELLO_HELLO_CONTROL_BITS_GROUP_ID_Z_DATA = 32
XHELLO_HELLO_CONTROL_ADDR_GLOBAL_OFFSET_X_DATA = 0x28
XHELLO_HELLO_CONTROL_BITS_GLOBAL_OFFSET_X_DATA = 32
XHELLO_HELLO_CONTROL_ADDR_GLOBAL_OFFSET_Y_DATA = 0x30
XHELLO_HELLO_CONTROL_BITS_GLOBAL_OFFSET_Y_DATA = 32
XHELLO_HELLO_CONTROL_ADDR_GLOBAL_OFFSET_Z_DATA = 0x38
XHELLO_HELLO_CONTROL_BITS_GLOBAL_OFFSET_Z_DATA = 32
XHELLO_HELLO_CONTROL_ADDR_ACCESS1_DATA = 0x40
XHELLO_HELLO_CONTROL_BITS_ACCESS1_DATA = 64


def runKernel(opt):
    xclOpenContext(opt.handle, opt.xuuid, 0, True)

    boHandle = xclAllocBO(opt.handle, opt.DATA_SIZE, 0, opt.first_mem)
    bo = xclMapBO(opt.handle, boHandle, True)
    ctypes.memset(bo, 0, opt.DATA_SIZE)

    xclSyncBO(opt.handle, boHandle, xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE, opt.DATA_SIZE, 0)

    print("Original string = [%s]\n" % bo.contents[:].decode("utf-8"))


    # Allocate the exec_bo
    execHandle = xclAllocBO(opt.handle, opt.DATA_SIZE, 0, (1 << 31))
    execData = xclMapBO(opt.handle, execHandle, True)  # returns mmap()

    print("Construct the exec command to run the kernel on FPGA")

    p = xclBOProperties()
    xclGetBOProperties(opt.handle, boHandle, p)
    # construct the exec buffer cmd to start the kernel
    start_cmd = ert_start_kernel_cmd.from_buffer(execData.contents)
    rsz = int((XHELLO_HELLO_CONTROL_ADDR_ACCESS1_DATA / 4 + 1) + 1)  # regmap array size
    ctypes.memset(execData.contents, 0, ctypes.sizeof(ert_start_kernel_cmd) + rsz*4)
    start_cmd.m_uert.m_start_cmd_struct.state = 1  # ERT_CMD_STATE_NEW
    start_cmd.m_uert.m_start_cmd_struct.opcode = 0  # ERT_START_CU
    start_cmd.m_uert.m_start_cmd_struct.count = 1 + rsz
    start_cmd.cu_mask = 0x1

    # Prepare kernel reg map
    new_data = (ctypes.c_uint32 * rsz).from_buffer(execData.contents, 8)
    new_data[XHELLO_HELLO_CONTROL_ADDR_AP_CTRL] = 0x0
    new_data[int(XHELLO_HELLO_CONTROL_ADDR_ACCESS1_DATA / 4)] = p.paddr
    new_data[int(XHELLO_HELLO_CONTROL_ADDR_ACCESS1_DATA / 4 + 1)] = (p.paddr >> 32) & 0xFFFFFFFF

    xclExecBuf(opt.handle, execHandle)
    print("Kernel start command issued through xclExecBuf : start_kernel")
    print("Now wait until the kernel finish")

    print("Wait until the command finish")
    while start_cmd.m_uert.m_start_cmd_struct.state < ert_cmd_state.ERT_CMD_STATE_COMPLETED:
        while xclExecWait(opt.handle, 100) == 0:
            print(".")

    print("Get the output data from the device")
    xclSyncBO(opt.handle, boHandle, xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, opt.DATA_SIZE, 0)
    result = bo.contents[:len("Hello World")]
    print("Result string = [%s]\n" % result.decode("utf-8"))

    xclCloseContext(opt.handle, opt.xuuid, 0)
    xclUnmapBO(opt.handle, boHandle, bo)
    xclFreeBO(opt.handle, execHandle)
    xclFreeBO(opt.handle, boHandle)

    assert(result == "Hello World")

def main(args):
    opt = Options()
    Options.getOptions(opt, args)

    try:
        initXRT(opt)
        assert (opt.first_mem >= 0), "Incorrect memory configuration"

        runKernel(opt)
        print("PASSED TEST")

    except OSError as o:
        print(o)
        print("FAILED TEST")
        sys.exit(o.errno)
    except AssertionError as a:
        print(a)
        print("FAILED TEST")
        sys.exit(1)
    except Exception as e:
        print(e)
        print("FAILED TEST")
        sys.exit(1)
    finally:
        xclClose(opt.handle)

if __name__ == "__main__":
    main(sys.argv)
