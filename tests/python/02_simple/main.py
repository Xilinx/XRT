import sys
# import source files
sys.path.append('../../../src/python/')
from xrt_binding import *
sys.path.append('../')
from utils_binding import *

XSIMPLE_CONTROL_ADDR_AP_CTRL = 0x00
XSIMPLE_CONTROL_ADDR_GIE = 0x04
XSIMPLE_CONTROL_ADDR_IER = 0x08
XSIMPLE_CONTROL_ADDR_ISR = 0x0c
XSIMPLE_CONTROL_ADDR_GROUP_ID_X_DATA = 0x10
XSIMPLE_CONTROL_BITS_GROUP_ID_X_DATA = 32
XSIMPLE_CONTROL_ADDR_GROUP_ID_Y_DATA = 0x18
XSIMPLE_CONTROL_BITS_GROUP_ID_Y_DATA = 32
XSIMPLE_CONTROL_ADDR_GROUP_ID_Z_DATA = 0x20
XSIMPLE_CONTROL_BITS_GROUP_ID_Z_DATA = 32
XSIMPLE_CONTROL_ADDR_GLOBAL_OFFSET_X_DATA = 0x28
XSIMPLE_CONTROL_BITS_GLOBAL_OFFSET_X_DATA = 32
XSIMPLE_CONTROL_ADDR_GLOBAL_OFFSET_Y_DATA = 0x30
XSIMPLE_CONTROL_BITS_GLOBAL_OFFSET_Y_DATA = 32
XSIMPLE_CONTROL_ADDR_GLOBAL_OFFSET_Z_DATA = 0x38
XSIMPLE_CONTROL_BITS_GLOBAL_OFFSET_Z_DATA = 32
XSIMPLE_CONTROL_ADDR_S1_DATA = 0x40
XSIMPLE_CONTROL_BITS_S1_DATA = 64
XSIMPLE_CONTROL_ADDR_S2_DATA = 0x4c
XSIMPLE_CONTROL_BITS_S2_DATA = 64
XSIMPLE_CONTROL_ADDR_FOO_DATA = 0x58
XSIMPLE_CONTROL_BITS_FOO_DATA = 32


def runKernel(opt):
    count = 1024
    DATA_SIZE = ctypes.sizeof(ctypes.c_int64) * count

    boHandle1 = xclAllocBO(opt.handle, DATA_SIZE, xclBOKind.XCL_BO_DEVICE_RAM, opt.first_mem)
    boHandle2 = xclAllocBO(opt.handle, DATA_SIZE, xclBOKind.XCL_BO_DEVICE_RAM, opt.first_mem)

    bo1 = xclMapBO(opt.handle, boHandle1, True, 'int')
    bo2 = xclMapBO(opt.handle, boHandle2, True, 'int')

    ctypes.memset(bo1, 0, opt.DATA_SIZE)
    ctypes.memset(bo2, 0, opt.DATA_SIZE)

    # bo1
    bo1_arr = [0X586C0C6C for _ in range(count)]
    arr = (ctypes.c_int * len(bo1_arr))(*bo1_arr)
    ctypes.memmove(bo1, arr, count*5)

    #bo2
    bo2_arr = [i*i for i in range(count)]
    arr = (ctypes.c_int * len(bo2_arr))(*bo2_arr)
    ctypes.memmove(bo2, arr, count*5)

    # bufReference
    bufReference = [i * i+i*16 for i in range(count)]

    if xclSyncBO(opt.handle, boHandle1, xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE, DATA_SIZE, 0):
        return 1

    if xclSyncBO(opt.handle, boHandle2, xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE, DATA_SIZE, 0):
        return 1

    p = xclBOProperties()
    bo1devAddr = p.paddr if not (xclGetBOProperties(opt.handle, boHandle1, p)) else -1
    bo2devAddr = p.paddr if not (xclGetBOProperties(opt.handle, boHandle2, p)) else -1

    if bo1devAddr is -1 or bo2devAddr is -1:
        return 1

    # Allocate the exec_bo
    execHandle = xclAllocBO(opt.handle, DATA_SIZE, xclBOKind.XCL_BO_SHARED_VIRTUAL, (1 << 31))
    execData = xclMapBO(opt.handle, execHandle, True, 'int', 32)  # required buffer size = 128

    if execData is None:
        print("execData is NULL")
    print("Construct the exe buf cmd to configure FPGA")
    xclOpenContext(opt.handle, opt.xuuid, 0, True)
    ecmd = ert_configure_cmd.from_buffer(execData.contents)
    ecmd.m_uert.m_cmd_struct.state = 1  # ERT_CMD_STATE_NEW
    ecmd.m_uert.m_cmd_struct.opcode = 2  # ERT_CONFIGURE

    ecmd.slot_size = opt.DATA_SIZE
    ecmd.num_cus = 1
    ecmd.cu_shift = 16
    ecmd.cu_base_addr = opt.cu_base_addr

    ecmd.m_features.ert = opt.ert
    if opt.ert:
        ecmd.m_features.cu_dma = 1
        ecmd.m_features.cu_isr = 1

    # CU -> base address mapping
    ecmd.data[0] = opt.cu_base_addr
    ecmd.m_uert.m_cmd_struct.count = 5 + ecmd.num_cus

    print("Send the exec command and configure FPGA (ERT)")

    # Send the command.
    if xclExecBuf(opt.handle, execHandle):
        print("Unable to issue xclExecBuf")
        return 1

    print("Wait until the command finish")

    while xclExecWait(opt.handle, 1000) == 0:
        print(".")


    print("Construct the exec command to run the kernel on FPGA")
    print("Due to the 1D OpenCL group size, the kernel must be launched %d times") % count

    # construct the exec buffer cmd to start the kernel
    for id in range(count):
        start_cmd = ert_start_kernel_cmd.from_buffer(execData.contents)
        rsz = XSIMPLE_CONTROL_ADDR_FOO_DATA/4 + 2  # regmap array size
        ctypes.memset(execData.contents, 0, ctypes.sizeof(ert_start_kernel_cmd) + rsz*4)
        start_cmd.m_uert.m_start_cmd_struct.state = 1  # ERT_CMD_STATE_NEW
        start_cmd.m_uert.m_start_cmd_struct.opcode = 0  # ERT_START_CU
        start_cmd.m_uert.m_start_cmd_struct.count = 1 + rsz
        start_cmd.cu_mask = 0x1

        # Prepare kernel reg map
        new_data = (ctypes.c_uint32 * rsz).from_buffer(execData.contents, 8)
        new_data[XSIMPLE_CONTROL_ADDR_AP_CTRL] = 0x0
        new_data[XSIMPLE_CONTROL_ADDR_GROUP_ID_X_DATA/4] = id
        new_data[XSIMPLE_CONTROL_ADDR_S1_DATA / 4] = bo1devAddr & 0xFFFFFFFF  # output
        new_data[XSIMPLE_CONTROL_ADDR_S2_DATA / 4] = bo2devAddr & 0xFFFFFFFF
        new_data[XSIMPLE_CONTROL_ADDR_S1_DATA / 4 + 1] = (bo1devAddr >> 32) & 0xFFFFFFFF  # output
        new_data[XSIMPLE_CONTROL_ADDR_S2_DATA / 4 + 1] = (bo2devAddr >> 32) & 0xFFFFFFFF  # input
        new_data[XSIMPLE_CONTROL_ADDR_FOO_DATA/4] = 0x10  # foo

        if xclExecBuf(opt.handle, execHandle):
            print("Unable to issue xclExecBuf")
            return 1

        print("Wait until the command finish")

        while xclExecWait(opt.handle, 100) == 0:
            print("reentering wait... \n")


    if start_cmd.m_uert.m_start_cmd_struct.state != 4:
        print("configure command failed")
        return 1

    # get the output xclSyncBO
    print("Get the output data from the device")
    if xclSyncBO(opt.handle, boHandle1, xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, DATA_SIZE, 0):
        return 1

    xclCloseContext(opt.handle, opt.xuuid, 0)
    xclFreeBO(opt.handle, execHandle)
    xclFreeBO(opt.handle, boHandle1)
    xclFreeBO(opt.handle, boHandle2)

    print("RESULT: ")
    if bufReference[:count] != bo1[:count]:
        print("FAILED TEST")
        print("Value read back does not match value written")
        sys.exit()


def main(args):
    opt = Options()
    Options.getOptions(opt, args)

    try:
        if initXRT(opt):
            xclClose(opt.handle)
            return 1
        if opt.first_mem < 0:
            xclClose(opt.handle)
            return 1
        if runKernel(opt):
            xclClose(opt.handle)
            return 1

    except Exception as exp:
        print("Exception: ")
        print(exp)  # prints the err
        print("FAILED TEST")
        sys.exit()

    print("PASSED TEST")


if __name__ == "__main__":
    main(sys.argv)