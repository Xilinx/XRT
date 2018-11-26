from ctypes import *
from enum import *

libc = CDLL("../../../build/Debug/opt/xilinx/xrt/lib/libxrt_core.so")


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


# def get_axlf_section(top, kind):
#  libc.get_axlf_section.restype = POINTER(axlf_section_header)
#  libc.get_axlf_scetion.argtypes = [POINTER(axlf), c_int]#axlf_section_kind]
#  return libc.get_axlf_section(top, kind)
    
    
# print("get_axlf_section %s") %get_axlf_section("kernel.xclbin",1)
