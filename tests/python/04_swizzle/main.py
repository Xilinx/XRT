import sys
import uuid
from xrt_binding import * # found in PYTHONPATH
sys.path.append('../') # utils_binding.py
from utils_binding import *

XVECTORSWIZZLE_CONTROL_ADDR_AP_CTRL = 0x00
XVECTORSWIZZLE_CONTROL_ADDR_GIE = 0x04
XVECTORSWIZZLE_CONTROL_ADDR_IER = 0x08
XVECTORSWIZZLE_CONTROL_ADDR_ISR = 0x0c
XVECTORSWIZZLE_CONTROL_ADDR_GROUP_ID_X_DATA = 0x10
XVECTORSWIZZLE_CONTROL_BITS_GROUP_ID_X_DATA = 32
XVECTORSWIZZLE_CONTROL_ADDR_GROUP_ID_Y_DATA = 0x18
XVECTORSWIZZLE_CONTROL_BITS_GROUP_ID_Y_DATA = 32
XVECTORSWIZZLE_CONTROL_ADDR_GROUP_ID_Z_DATA = 0x20
XVECTORSWIZZLE_CONTROL_BITS_GROUP_ID_Z_DATA = 32
XVECTORSWIZZLE_CONTROL_ADDR_GLOBAL_OFFSET_X_DATA = 0x28
XVECTORSWIZZLE_CONTROL_BITS_GLOBAL_OFFSET_X_DATA = 32
XVECTORSWIZZLE_CONTROL_ADDR_GLOBAL_OFFSET_Y_DATA = 0x30
XVECTORSWIZZLE_CONTROL_BITS_GLOBAL_OFFSET_Y_DATA = 32
XVECTORSWIZZLE_CONTROL_ADDR_GLOBAL_OFFSET_Z_DATA = 0x38
XVECTORSWIZZLE_CONTROL_BITS_GLOBAL_OFFSET_Z_DATA = 32
XVECTORSWIZZLE_CONTROL_ADDR_A_DATA = 0x40

def runKernel(opt):
    elem_num = 4096
    DATA_SIZE = ctypes.sizeof(ctypes.c_int) * elem_num
    boHandle = xclAllocBO(opt.handle, DATA_SIZE, xclBOKind.XCL_BO_DEVICE_RAM, opt.first_mem)
    bo = xclMapBO(opt.handle, boHandle, True)
    ctypes.memset(bo, 0, DATA_SIZE)
    bo_arr = ctypes.cast(bo, ctypes.POINTER(ctypes.c_int))
    reference = []

    for idx in range(elem_num):
        remainder = idx % 4
        bo_arr[idx] = idx
        if remainder == 0:
            reference.append(idx+2)
        if remainder == 1:
            reference.append(idx+2)
        if remainder == 2:
            reference.append(idx-2)
        if remainder == 3:
            reference.append(idx-2)

    if xclSyncBO(opt.handle, boHandle, xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE, DATA_SIZE, 0):
        return 1

    p = xclBOProperties()
    bodevAddr = p.paddr if not (xclGetBOProperties(opt.handle, boHandle, p)) else -1

    if bodevAddr is -1:
        return 1

    # Allocate the exec_bo
    execHandle = xclAllocBO(opt.handle, DATA_SIZE, xclBOKind.XCL_BO_SHARED_VIRTUAL, (1 << 31))
    execData = xclMapBO(opt.handle, execHandle, True)  # returns mmap()

    print("Construct the exe buf cmd to configure FPGA")

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

    # sz = sizeof(ert_configure_cmd)
    print("Send the exec command and configure FPGA (ERT)")

    # Send the command.
    ret = xclExecBuf(opt.handle, execHandle)

    if ret:
        print("Unable to issue xclExecBuf")
        return 1

    print("Wait until the ERT configure command finish")

    while xclExecWait(opt.handle, 1000) == 0:
        print(".")

    if ecmd.m_uert.m_cmd_struct.state != 4:
        print("ERT configure command failed")
        return 1

    print("Construct the exec command to run the kernel on FPGA")

    xclOpenContext(opt.handle, opt.xuuid, 0, True)
    # construct the exec buffer cmd to start the kernel
    start_cmd = ert_start_kernel_cmd.from_buffer(execData.contents)
    rsz = (XVECTORSWIZZLE_CONTROL_ADDR_A_DATA / 4 + 1) + 1  # regmap array size
    ctypes.memset(execData.contents, 0, ctypes.sizeof(ert_start_kernel_cmd) + rsz*4)
    start_cmd.m_uert.m_start_cmd_struct.state = 1  # ERT_CMD_STATE_NEW
    start_cmd.m_uert.m_start_cmd_struct.opcode = 0  # ERT_START_CU
    start_cmd.m_uert.m_start_cmd_struct.count = 1 + rsz
    start_cmd.cu_mask = 0x1

    # Prepare kernel reg map
    new_data = (ctypes.c_uint32 * rsz).from_buffer(execData.contents, 8)
    new_data[XVECTORSWIZZLE_CONTROL_ADDR_AP_CTRL] = 0x0 # ap_start
    new_data[XVECTORSWIZZLE_CONTROL_ADDR_GROUP_ID_X_DATA/4] = 0x0 # group id
    new_data[XVECTORSWIZZLE_CONTROL_ADDR_A_DATA / 4] = bodevAddr
    new_data[XVECTORSWIZZLE_CONTROL_ADDR_A_DATA / 4 + 1] = (bodevAddr >> 32) & 0xFFFFFFFF # s1 buffer

    global_dim = [DATA_SIZE / 4, 0]; # int4 vector count global range
    local_dim = [16, 0]; # int4 vector count global range
    groupSize = global_dim[0] / local_dim[0];

    if opt.verbose == 1:
        print("Global range: %d " % global_dim[0])
        print("Group size  : %d " % local_dim[0])
        print("Starting kernel...\n")
        

    for id in range(groupSize):
        if opt.verbose == 1:
            print("group id = %d" % id)

        new_data[XVECTORSWIZZLE_CONTROL_ADDR_AP_CTRL] = 0x0 # ap_start
        new_data[XVECTORSWIZZLE_CONTROL_ADDR_GROUP_ID_X_DATA/4] = id # group id

        # Execute the command
        if xclExecBuf(opt.handle, execHandle):
            print("Unable to issue xclExecBuf")
            return 1

        if opt.verbose == 1:
            print("Wait until the command finish")

        while xclExecWait(opt.handle, 1000) == 0:
            print(".")
    
    # get the output xclSyncBO
    print("Get the output data from the device")
    if xclSyncBO(opt.handle, boHandle, xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, DATA_SIZE, 0):
        return 1

    fail = 0
    print("Compare the FPGA results with golden data")
    for idx in range(elem_num):
        if bo_arr[idx] != reference[idx]:
            fail = 1
            print("FAIL: Results mismatch at [%d] fpga= %d, cpu= %d" % (idx, bo_arr[idx], reference[idx]))
            break

    xclCloseContext(opt.handle, opt.xuuid, 0)
    xclFreeBO(opt.handle, execHandle)
    xclFreeBO(opt.handle, boHandle)

    return fail

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
