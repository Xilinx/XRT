import sys, struct
import numpy as np
# import source files
sys.path.append('../../../src/python/')
from xrt_binding import *
sys.path.append('../')
from utils_binding import *
from cffi import FFI

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
    DATA_SIZE = sizeof(c_int64) * count

    ffi = FFI()  # create the FFI obj
    boHandle1 = xclAllocBO(opt.handle, DATA_SIZE, xclBOKind.XCL_BO_DEVICE_RAM, opt.first_mem)
    boHandle2 = xclAllocBO(opt.handle, DATA_SIZE, xclBOKind.XCL_BO_DEVICE_RAM, opt.first_mem)

    bo1 = xclMapBO(opt.handle, boHandle1, True)
    bo2 = xclMapBO(opt.handle, boHandle2, True)

    bo1_fp = ffi.cast("FILE *", bo1)
    bo2_fp = ffi.cast("FILE *", bo2)

    # bo1
    bo1_arr = np.array([0x586C0C6C for _ in range(count)])
    ffi.memmove(bo1_fp, ffi.from_buffer(bo1_arr), count*5)

    # bo2
    int_arr = np.array([i * i for i in range(count)])
    bo2_arr = np.array(map(str, int_arr.astype(int)))

    ffi.memmove(bo2_fp, ffi.from_buffer(bo2_arr), count*7)

    # bufReference
    int_arr_2 = np.array([i * i+i*16 for i in range(count)])
    str_arr = np.array(map(str, int_arr_2))
    buf = ffi.from_buffer(str_arr)
    bufReference = ''.join(buf)

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
    execData = xclMapBO(opt.handle, execHandle, True)  # returns mmap()
    c_f = ffi.cast("FILE *", execData)

    if execData is ffi.NULL:
        print("execData is NULL")

    print("Construct the exe buf cmd to configure FPGA")

    ecmd = ert_configure_cmd()
    ecmd.m_uert.m_cmd_struct.state = 1  # ERT_CMD_STATE_NEW
    ecmd.m_uert.m_cmd_struct.opcode = 2  # ERT_CONFIGURE

    ecmd.slot_size = 1024
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

    sz = sizeof(ert_configure_cmd)
    ffi.memmove(c_f, ecmd, sz)
    print("Send the exec command and configure FPGA (ERT)")

    # Send the command.
    if xclExecBuf(opt.handle, execHandle):
        print("Unable to issue xclExecBuf")
        return 1

    print("Wait until the command finish")

    while xclExecWait(opt.handle, 1000) != 0:
        print(".")

    print("Construct the exec command to run the kernel on FPGA")
    print("Due to the 1D OpenCL group size, the kernel must be launched %d times") % count

    # construct the exec buffer cmd to start the kernel
    for id in range(count):
        start_cmd = ert_start_kernel_cmd()
        rsz = XSIMPLE_CONTROL_ADDR_FOO_DATA/4 + 2  # regmap array size
        new_data = ((start_cmd.data._type_) * rsz)()
        start_cmd.m_uert.m_start_cmd_struct.state = 1  # ERT_CMD_STATE_NEW
        start_cmd.m_uert.m_start_cmd_struct.opcode = 0  # ERT_START_CU
        start_cmd.m_uert.m_start_cmd_struct.count = 1 + rsz
        start_cmd.cu_mask = 0x1

        new_data[XSIMPLE_CONTROL_ADDR_AP_CTRL] = 0x0
        new_data[XSIMPLE_CONTROL_ADDR_GROUP_ID_X_DATA/4] = id
        new_data[XSIMPLE_CONTROL_ADDR_S1_DATA / 4] = bo1devAddr & 0xFFFFFFFF  # output
        new_data[XSIMPLE_CONTROL_ADDR_S2_DATA / 4] = bo2devAddr & 0xFFFFFFFF  # input
        new_data[XSIMPLE_CONTROL_ADDR_FOO_DATA/4] = 0x10  # foo
        ffi.memmove(c_f, start_cmd, 2 * sizeof(c_uint32))

        tmp_buf = ffi.buffer(c_f, 2 * sizeof(c_uint32) + (len(new_data) * sizeof(c_uint32)))
        data_ptr = ffi.from_buffer(tmp_buf)
        ffi.memmove(data_ptr + 2 * sizeof(c_uint32), new_data, len(new_data) * sizeof(c_uint32))

        if xclExecBuf(opt.handle, execHandle):
            print("Unable to issue xclExecBuf")
            return 1

        print("Wait until the command finish")

        while xclExecWait(opt.handle, 100) != 0:
            print("reentering wait... \n")

    # get the output xclSyncBO
    print("Get the output data from the device")
    if xclSyncBO(opt.handle, boHandle1, xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, DATA_SIZE, 0):
        return 1
    rd_buf = ffi.buffer(bo1_fp, count*7)
    print("RESULT: ")
    # print(rd_buf[:] + "\n")

    if bufReference != rd_buf[:]:
        print("FAILED TEST")
        print("Value read back does not match value written")
        sys.exit()


def main(args):
    opt = Options()
    Options.getOptions(opt, args)

    try:
        if initXRT(opt):
            return 1
        if opt.first_mem < 0:
            return 1
        if runKernel(opt):
            return 1

    except Exception as exp:
        print("Exception: ")
        print(exp)  # prints the err
        print("FAILED TEST")
        sys.exit()

    print("PASSED TEST")


if __name__ == "__main__":
    main(sys.argv)