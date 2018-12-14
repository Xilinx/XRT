from ctypes import *
import os
libc = CDLL(os.environ['XILINX_XRT'] + "/lib/libxrt_core.so")


class ert_cmd_struct (Structure):
    _fields_ = [
        ("state", c_uint32, 4),
        ("unused", c_uint32, 8),
        ("count", c_uint32, 11),
        ("opcode", c_uint32, 5),
        ("type", c_uint32, 4)
    ]


class uert (Union):
#    _anonymous_ = ("ert_cmd_struct")
    _fields_ = [
        ("m_cmd_struct", ert_cmd_struct),
        ("header", c_uint32)
    ]

class ert_configure_features (Structure):
        # features
    _fields_ = [
        ("ert", c_uint32, 1),
        ("polling", c_uint32, 1),
        ("cu_dma", c_uint32, 1),
        ("cu_isr", c_uint32, 1),
        ("cq_int", c_uint32, 1),
        ("cdma", c_uint32, 1),
        ("unusedf", c_uint32, 25),
        ("dsa52", c_uint32, 1),
    ]

class ert_configure_cmd (Structure):
#    _anonymous_ = "uert"
    _fields_ = [
        ("m_uert", uert),
        #("m_cmd_struct", ert_cmd_struct),

        # payload
        ("slot_size", c_uint32),
        ("num_cus", c_uint32),
        ("cu_shift", c_uint32),
        ("cu_base_addr", c_uint32),

#        # features
#        ("ert", c_uint32, 1),
#        ("polling", c_uint32, 1),
#        ("cu_dma", c_uint32, 1),
#        ("cu_isr", c_uint32, 1),
#        ("cq_int", c_uint32, 1),
#        ("cdma", c_uint32, 1),
#        ("unusedf", c_uint32, 25),
#        ("dsa52", c_uint32, 1),
        ("m_features", ert_configure_features),

        # cu address map size is num_cus
        ("data", c_uint32*1)
    ]

class ert_start_cmd_struct (Structure):
    _fields_ = [
        ("state", c_uint32, 4),
        ("unused", c_uint32, 6),
        ("extra_cu_masks", c_uint32, 2),
        ("count", c_uint32, 11),
        ("opcode", c_uint32, 5),
        ("type", c_uint32, 4)
    ]

class u_start_ert (Union):
    _fields_ = [
        ("m_start_cmd_struct", ert_start_cmd_struct),
        ("header", c_uint32)
    ]

class ert_start_kernel_cmd (Structure):
    _fields_ = [
        ("m_uert", u_start_ert),

        # payload
        ("cu_mask", c_uint32),
        ("data", c_uint32*1)
    ]

