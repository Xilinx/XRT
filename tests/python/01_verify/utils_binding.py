import getopt, sys, struct, mmap
from ctypes import *
# import source files
sys.path.append('../../../src/python/')
from xclbin_binding import *
from xrt_binding import *
from ert_binding import *
from cffi import FFI

XHELLO_HELLO_CONTROL_ADDR_AP_CTRL              = 0x00
XHELLO_HELLO_CONTROL_ADDR_GIE                  = 0x04
XHELLO_HELLO_CONTROL_ADDR_IER                  = 0x08
XHELLO_HELLO_CONTROL_ADDR_ISR                  = 0x0c
XHELLO_HELLO_CONTROL_ADDR_GROUP_ID_X_DATA      = 0x10
XHELLO_HELLO_CONTROL_BITS_GROUP_ID_X_DATA      = 32
XHELLO_HELLO_CONTROL_ADDR_GROUP_ID_Y_DATA      = 0x18
XHELLO_HELLO_CONTROL_BITS_GROUP_ID_Y_DATA      = 32
XHELLO_HELLO_CONTROL_ADDR_GROUP_ID_Z_DATA      = 0x20
XHELLO_HELLO_CONTROL_BITS_GROUP_ID_Z_DATA      = 32
XHELLO_HELLO_CONTROL_ADDR_GLOBAL_OFFSET_X_DATA = 0x28
XHELLO_HELLO_CONTROL_BITS_GLOBAL_OFFSET_X_DATA = 32
XHELLO_HELLO_CONTROL_ADDR_GLOBAL_OFFSET_Y_DATA = 0x30
XHELLO_HELLO_CONTROL_BITS_GLOBAL_OFFSET_Y_DATA = 32
XHELLO_HELLO_CONTROL_ADDR_GLOBAL_OFFSET_Z_DATA = 0x38
XHELLO_HELLO_CONTROL_BITS_GLOBAL_OFFSET_Z_DATA = 32
XHELLO_HELLO_CONTROL_ADDR_ACCESS1_DATA         = 0x40
XHELLO_HELLO_CONTROL_BITS_ACCESS1_DATA         = 64


class Options(object):
    def __init__(self):
        self.DATA_SIZE = 1024
        self.sharedLibrary = "None"
        self.bitstreamFile = None
        self.halLogFile = ""
        self.alignment = 128
        self.option_index = 0
        self.index = 0
        self.cu_index = 0
        self.verbose = False
        self.handle = xclDeviceHandle
        self.first_mem = -1
        self.cu_base_addr = -1
        self.ert = False
        #n_elements?

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
        print("HAL driver = " + self.sharedLibrary)
        print("Host buffer alignment "+str(self.alignment) + " bytes")
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

    print( "xclOpen args", opt.index, opt.halLogFile, xclVerbosityLevel.XCL_INFO )
    #opt.handle = xclOpen(opt.index, opt.halLogFile, xclVerbosityLevel.XCL_INFO)
    opt.handle = xclOpen(0, "", xclVerbosityLevel.XCL_INFO)
    print( "xclOpen handle:", opt.handle )
    #opt.handle = xclOpen(opt.index, opt.halLogFile, xclVerbosityLevel.XCL_INFO)
    if xclGetDeviceInfo2(opt.handle, byref(deviceInfo)):
        print("Error 2")
        return -1

    print("DSA = %s") % deviceInfo.mName
    print("Index = %s") % opt.index
    print("PCIe = GEN%s" + " x %s") % (deviceInfo.mPCIeLinkSpeed, deviceInfo.mPCIeLinkWidth)
    print("OCL Frequency = %s MHz") % deviceInfo.mOCLFrequency[0]
    print("DDR Bank = %s") % deviceInfo.mDDRBankCount
    print("Device Temp = %s C") % deviceInfo.mOnChipTemp
    print("MIG Calibration = %s") % deviceInfo.mMigCalib

    if not opt.bitstreamFile or not len(opt.bitstreamFile):
        print(opt.bitstreamFile)
        return 0

    if xclLockDevice(opt.handle):
        print("Cannot unlock device")
        sys.exit()

    tempFileName = opt.bitstreamFile

    with open(tempFileName, "rb") as f:
        header = f.read(7)
        if header != "xclbin2":
            print("Invalid Bitsream")
            sys.exit()
        f.seek(0)
        blob = f.read()
        #if not xclLoadXclBin(opt.handle, blob):
        #    print("Bitsream download failed")
        xclLoadXclBin(opt.handle, blob)
        print("Finished downloading bitstream %s") % opt.bitstreamFile

        f.seek(0)
        top = f.read()
        ip = wrap_get_axlf_section(top, AXLF_SECTION_KIND.IP_LAYOUT)

        f.seek(ip.contents.m_sectionOffset)
        count = int(f.read(1).encode("hex"))

        if opt.cu_index > count:
            print("Can't determine cu base address")
            sys.exit()

        f.seek(ip.contents.m_sectionOffset + sizeof(c_int32) + sizeof(c_int32))
        # unpack sequence: '=1I1I1Q64s' = 1 uint32 1 uint32 1 uint32 1 uint64 64 char
        struct_fmt = '=1I1I1Q64s'
        struct_len = struct.calcsize(struct_fmt)
        struct_unpack = struct.Struct(struct_fmt).unpack_from

        for i in range(count):
            s = struct_unpack(f.read(struct_len))
            if s[0] == 1:
                opt.cu_base_addr = s[2]
                print("base address %s") % hex(s[2])

        topo = wrap_get_axlf_section(top, AXLF_SECTION_KIND.MEM_TOPOLOGY)
        f.seek(topo.contents.m_sectionOffset)
        topo_count = int(f.read(1).encode("hex"))

        f.seek(topo.contents.m_sectionOffset + sizeof(c_int32) + sizeof(c_int32))
        # unpack sequence: '=1b1b6b1Q1Q16s' = 1 byte 1 byte 6 bytes 1 uint64 1 uint64 6 char
        struct_fmt = '=1b1b6b1Q1Q16s'
        struct_len = struct.calcsize(struct_fmt)
        struct_unpack = struct.Struct(struct_fmt).unpack_from

        for i in range(topo_count):
            s = struct_unpack(f.read(struct_len))

            if s[1] == 1:
                opt.first_mem = i
                break

    return 0

def validate( bo ):
    ecmd = ert_configure_cmd.from_buffer( bo )
    print( ecmd.m_uert.m_cmd_struct.opcode )
    print( ecmd.m_uert.m_cmd_struct.count )
    print( ecmd.m_uert.m_cmd_struct.state )
    return 0

def validate2( bo ):
    ffi = FFI()
    #i_ecmd = ffi.cast( "int", bo )
    buf = ffi.buffer( bo, sizeof(ert_configure_cmd) )
    ecmd = ert_configure_cmd.from_buffer( buf )
    print( ecmd.m_uert.m_cmd_struct.opcode )
    print( ecmd.m_uert.m_cmd_struct.count )
    print( ecmd.m_uert.m_cmd_struct.state )
    return 0


def runKernel(opt):
    ffi = FFI() # create the FFI obj
    boHandle = xclAllocBO(opt.handle, opt.DATA_SIZE, xclBOKind.XCL_BO_DEVICE_RAM, opt.first_mem)
    bo1 = xclMapBO(opt.handle, boHandle, True)
    read_fp = ffi.cast( "FILE *", bo1 )

    print( "before memset" )
    #memset( addressof( c_int(bo1) ), 0, 1024 )
    print( "after memset" )

    if xclSyncBO(opt.handle, boHandle, xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE, opt.DATA_SIZE, 0):
        return 1

    # test write hello, read hello from BO
    # fp = ffi.cast( "FILE *", bo1 )
    # teststr = "hello"
    # ffi.memmove( fp, teststr, len(teststr) )
    # ret = xclSyncBO(opt.handle, boHandle, xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE, opt.DATA_SIZE, 0)
    # # xbutil mem --read -a 0x500000 -i 20 -o f.out , will show "hello"
    # read_fp = ffi.cast( "FILE *", bo1 )
    # comparestr = "HELLO"
    # rd_buf = ffi.buffer( read_fp, len(teststr) ) 
    # ret = xclSyncBO(opt.handle, boHandle, xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, opt.DATA_SIZE, 0)
    # print( rd_buf )

    p = xclBOProperties()
    bodevAddr = p.paddr if not(xclGetBOProperties(opt.handle, boHandle, p)) else -1

    if bodevAddr is -1:
        return 1

    # Allocate the exec_bo
    execHandle = xclAllocBO(opt.handle, opt.DATA_SIZE, xclBOKind.XCL_BO_SHARED_VIRTUAL, (1 << 31))
    print( "xclAllocBO : ", execHandle )
    execData = xclMapBO(opt.handle, execHandle, True) # returns mmap()
    c_f = ffi.cast( "FILE *", execData )
    print( "type execData: ", type(execData) )
    print( "type c_f: ", type(c_f) )
    if execData is ffi.NULL:
        print( "execData is NULL" )
    print("Construct the exe buf cmd to configure FPGA")

    ecmd = ert_configure_cmd()
    ecmd.m_uert.m_cmd_struct.state = 1#ERT_CMD_STATE_NEW
    ecmd.m_uert.m_cmd_struct.opcode = 2#ERT_CONFIGURE

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

    sz = sizeof(ert_configure_cmd)
    ffi.memmove( c_f, ecmd, sz )
    validate( ecmd )
    validate2( c_f )

    print("Send the exec command and configure FPGA (ERT)")

    # Send the command.
    ret = xclExecBuf(opt.handle, execHandle)
    print( "xclExecBuf: ", ret )
    if ret:  # -22
        print("Unable to issue xclExecBuf")
        return 1

    print("Wait until the command finish")

    #while xclExecWait(opt.handle, 1000)) !=0
    while xclExecWait(opt.handle, 1000) != 0:
        print( "." )

    print("Construct the exec command to run the kernel on FPGA")
    # L144

    # construct the exec buffer cmd to start the kernel
    start_cmd = ert_start_kernel_cmd()
    rsz = (XHELLO_HELLO_CONTROL_ADDR_ACCESS1_DATA/4+1) +1# regmap array size
    # how to grow start_cmd so data[] is larger than 1 Byte.
    new_data = ((start_cmd.data._type_)*rsz)()
    start_cmd.m_uert.m_start_cmd_struct.state = 1#ERT_CMD_STATE_NEW
    start_cmd.m_uert.m_start_cmd_struct.opcode = 0#ERT_START_CU
    start_cmd.m_uert.m_start_cmd_struct.count = 1 + rsz
    start_cmd.cu_mask = 0x1

    #import pdb; pdb.set_trace() # breakpoint

    new_data[XHELLO_HELLO_CONTROL_ADDR_AP_CTRL] = 0x0
    new_data[XHELLO_HELLO_CONTROL_ADDR_ACCESS1_DATA/4] = bodevAddr
    new_data[XHELLO_HELLO_CONTROL_ADDR_ACCESS1_DATA/4 + 1] = (bodevAddr >> 32) & 0xFFFFFFFF
    #for i in range( 18 ):
    #    new_data[ i ] = i
    #new_data[17]=50

    #sz_start_minus_data = sizeof( start_cmd ) - sizeof( start_cmd.data )
    #print( "size: sz_start_minus_data: ", sz_start_minus_data )
    len_start_cmd = sizeof( start_cmd )
    ffi.memmove( c_f, start_cmd, 2*sizeof(c_uint32)) # send start_cmd minus data[], which is one c_uint32
    #ffi.memmove( c_f, start_cmd, 2 )#sz_start_minus_data ) # send start_cmd minus data[], which is one c_uint32

    # hokey way to move the pointer of c_f
    tmp_buf = ffi.buffer( c_f, 2*sizeof(c_uint32)+(len(new_data)*sizeof(c_uint32)) ) # alloc buffer size of entire command
    #tmp_buf = ffi.buffer( c_f, rsz )#sz_start_minus_data + rsz ) # alloc buffer size of entire command
    data_ptr = ffi.from_buffer( tmp_buf )
    ffi.memmove( data_ptr + 2*sizeof(c_uint32), new_data, len( new_data )*sizeof(c_uint32) )
    #ffi.memmove( data_ptr + 2, new_data, rsz )

    ret = xclExecBuf(opt.handle, execHandle)
    print( "xclExecBuf: ", ret )
    if ret:  # -22
        print("Unable to issue xclExecBuf")
        return 1

    print("Wait until the command finish")

    while xclExecWait(opt.handle, 1) != 0:
        print( "." )


    # read status or state of scheduler
    
    

    # get the output xclSyncBO
    if xclSyncBO(opt.handle, boHandle, xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE, opt.DATA_SIZE, 0):
        return 1

    #import pdb; pdb.set_trace() # breakpoint
    rd_buf = ffi.buffer( read_fp, len("Hello World") )
    print( "rd_buf: ", rd_buf[:] )


    return 0


