from ctypes import *
from enum import *
import os
libc = CDLL(os.environ['XILINX_XRT'] + "/lib/libxrt_core.so")


class AXLF_SECTION_KIND (Enum):
    BITSTREAM = 0
    CLEARING_BITSTREAM = 1
    EMBEDDED_METADATA = 2
    FIRMWARE = 3
    DEBUG_DATA = 4
    SCHED_FIRMWARE = 5
    MEM_TOPOLOGY = 6
    CONNECTIVITY = 7
    IP_LAYOUT = 8
    DEBUG_IP_LAYOUT = 9
    DESIGN_CHECK_POINT = 10
    CLOCK_FREQ_TOPOLOGY = 11
    MCS = 12
    BMC = 13
    BUILD_METADATA = 14
    KEYVALUE_METADATA = 15
    USER_METADATA = 16
    DNA_CERTIFICATE = 17


class XCLBIN_MODE (Enum):
    XCLBIN_FLAT = 1
    XCLBIN_PR = 2
    XCLBIN_TANDEM_STAGE2 = 3
    XCLBIN_TANDEM_STAGE2_WITH_PR = 4
    XCLBIN_HW_EMU = 5
    XCLBIN_SW_EMU = 6
    XCLBIN_MODE_MAX = 7


class axlf_section_header (Structure):
    _fields_ = [
        ("m_sectionKind", c_uint32),
        ("m_sectionName", c_char*16),
        ("m_sectionOffset", c_uint64),
        ("m_sectionSize", c_uint64)
    ]


class mem_u1 (Union):
    _fields_ = [
        ("m_size", c_int64),
        ("route_id", c_int64)
    ]


class mem_u2 (Union):
    _fields_ = [
        ("m_base_address", c_int64),
        ("flow_id", c_int64)
    ]


class mem_data (Structure):
    _anonymous_ = ("mem_u1", "mem_u2")
    _fields_ = [
        ("m_type", c_uint8),
        ("m_used", c_uint8),
        ("mem_u1", mem_u1),
        ("mem_u2", mem_u2),
        ("m_tag", c_char * 16)
    ]


class mem_topology (Structure):
    _fields_ = [
        ("m_count", c_int32),
        ("m_mem_data", mem_data*1)
    ]


class ip_data (Structure):
    _fields_ = [
        ("m_type", c_uint32),
        ("properties", c_uint32),
        ("m_base_address", c_uint64),
        ("m_name", c_uint8 * 64)
    ]


class ip_layout (Structure):
    _fields_ = [
        ("m_count", c_int32),
        ("m_ip_data", ip_data*1)
    ]


class s1 (Structure):
    _fields_ = [
        ("m_platformId", c_uint64),
        ("m_featureId", c_uint64)
    ]


class u1 (Union):
    _fields_ = [
        ("rom", s1),
        ("rom_uuid", c_ubyte*16)
    ]


class u2 (Union):
    _fields_ = [
        ("m_next_axlf", c_char*16),
        ("uuid", c_ubyte*16)  # uuid_t/xuid_t
    ]


class axlf_header (Structure):
    _anonymous_ = ("u1","u2")
    _fields_ = [
        ("m_length", c_uint64),
        ("m_timeStamp", c_uint64),
        ("m_featureRomTimeStamp", c_uint64),
        ("m_versionPatch", c_uint16),
        ("m_versionMajor", c_uint8),
        ("m_versionMinor", c_uint8),
        ("m_mode", c_uint32),
        ("u1", u1),
        ("m_platformVBNV", c_ubyte*64),
        ("u2", u2),
        ("m_debug_bin", c_char*16),
        ("m_numSections", c_uint32)
    ]


class axlf (Structure):
    _fields_ = [
        ("m_magic", c_char*8),
        ("m_cipher", c_ubyte*32),
        ("m_keyBlock", c_ubyte*256),
        ("m_uniqueId", c_uint64),
        ("m_header", axlf_header),
        ("m_sections", axlf_section_header)
    ]


def wrap_get_axlf_section(top, kind):
    libc.wrap_get_axlf_section.restype = POINTER(axlf_section_header)
    libc.wrap_get_axlf_section.argtypes = [c_void_p, c_int]
    return libc.wrap_get_axlf_section(top, kind)

