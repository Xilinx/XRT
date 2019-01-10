##
 # Copyright (C) 2018 Xilinx, Inc
 # Author(s): Ryan Radjabi
 #            Shivangi Agarwal
 #            Sonal Santan
 # ctypes based Python binding for xclbin.h data structures
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

libc = ctypes.CDLL(os.environ['XILINX_XRT'] + "/lib/libxrt_core.so")


class AXLF_SECTION_KIND ():
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


class XCLBIN_MODE ():
    XCLBIN_FLAT = 1
    XCLBIN_PR = 2
    XCLBIN_TANDEM_STAGE2 = 3
    XCLBIN_TANDEM_STAGE2_WITH_PR = 4
    XCLBIN_HW_EMU = 5
    XCLBIN_SW_EMU = 6
    XCLBIN_MODE_MAX = 7


class axlf_section_header (ctypes.Structure):
    _fields_ = [
        ("m_sectionKind", ctypes.c_uint32),
        ("m_sectionName", ctypes.c_char*16),
        ("m_sectionOffset", ctypes.c_uint64),
        ("m_sectionSize", ctypes.c_uint64)
    ]


class mem_u1 (ctypes.Union):
    _fields_ = [
        ("m_size", ctypes.c_int64),
        ("route_id", ctypes.c_int64)
    ]


class mem_u2 (ctypes.Union):
    _fields_ = [
        ("m_base_address", ctypes.c_int64),
        ("flow_id", ctypes.c_int64)
    ]


class mem_data (ctypes.Structure):
    _anonymous_ = ("mem_u1", "mem_u2")
    _fields_ = [
        ("m_type", ctypes.c_uint8),
        ("m_used", ctypes.c_uint8),
        ("mem_u1", mem_u1),
        ("mem_u2", mem_u2),
        ("m_tag", ctypes.c_char * 16)
    ]


class mem_topology (ctypes.Structure):
    _fields_ = [
        ("m_count", ctypes.c_int32),
        ("m_mem_data", mem_data*1)
    ]


class ip_data (ctypes.Structure):
    _fields_ = [
        ("m_type", ctypes.c_uint32),
        ("properties", ctypes.c_uint32),
        ("m_base_address", ctypes.c_uint64),
        ("m_name", ctypes.c_uint8 * 64)
    ]


class ip_layout (ctypes.Structure):
    _fields_ = [
        ("m_count", ctypes.c_int32),
        ("m_ip_data", ip_data*1)
    ]


class s1 (ctypes.Structure):
    _fields_ = [
        ("m_platformId", ctypes.c_uint64),
        ("m_featureId", ctypes.c_uint64)
    ]


class u1 (ctypes.Union):
    _fields_ = [
        ("rom", s1),
        ("rom_uuid", ctypes.c_ubyte*16)
    ]


class u2 (ctypes.Union):
    _fields_ = [
        ("m_next_axlf", ctypes.c_char*16),
        ("uuid", ctypes.c_ubyte*16)  # uuid_t/xuid_t
    ]


class axlf_header (ctypes.Structure):
    _anonymous_ = ("u1","u2")
    _fields_ = [
        ("m_length", ctypes.c_uint64),
        ("m_timeStamp", ctypes.c_uint64),
        ("m_featureRomTimeStamp", ctypes.c_uint64),
        ("m_versionPatch", ctypes.c_uint16),
        ("m_versionMajor", ctypes.c_uint8),
        ("m_versionMinor", ctypes.c_uint8),
        ("m_mode", ctypes.c_uint32),
        ("u1", u1),
        ("m_platformVBNV", ctypes.c_ubyte*64),
        ("u2", u2),
        ("m_debug_bin", ctypes.c_char*16),
        ("m_numSections", ctypes.c_uint32)
    ]


class axlf (ctypes.Structure):
    _fields_ = [
        ("m_magic", ctypes.c_char*8),
        ("m_cipher", ctypes.c_ubyte*32),
        ("m_keyBlock", ctypes.c_ubyte*256),
        ("m_uniqueId", ctypes.c_uint64),
        ("m_header", axlf_header),
        ("m_sections", axlf_section_header)
    ]


def wrap_get_axlf_section(top, kind):
    libc.wrap_get_axlf_section.restype = ctypes.POINTER(axlf_section_header)
    libc.wrap_get_axlf_section.argtypes = [ctypes.c_void_p, ctypes.c_int]
    return libc.wrap_get_axlf_section(top, kind)
