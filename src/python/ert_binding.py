from ctypes import *
import os
libc = CDLL(os.environ['XILINX_XRT'] + "/lib/libxrt_core.so")


# class structe (Structure):
#     _fields = [
#         ("state", c_uint32, 4),
#         ("unused", c_uint32, 8),
#         ("count", c_uint32, 11),
#         ("opcode", c_uint32, 5),
#         ("type", c_uint32, 4)
#     ]


# class uert (Union):
#     #_anonymous_ = ("structe")
#     _fields_ = [
#         ("structe", c_uint32),
#         ("header", c_uint32)
#     ]


class ert_configure_cmd (Structure):
    #_anonymous_ = "uert"
    _fields_ = [
        ("configure_cmd", c_uint32),

        # payload
        ("slot_size", c_uint32),
        ("num_cus", c_uint32),
        ("cu_shift", c_uint32),
        ("cu_base_addr", c_uint32),

        # features
        ("ert", c_uint32, 1),
        ("polling", c_uint32, 1),
        ("cu_dma", c_uint32, 1),
        ("cu_isr", c_uint32, 1),
        ("cq_int", c_uint32, 1),
        ("cdma", c_uint32, 1),
        ("unusedf", c_uint32, 25),
        ("dsa52", c_uint32, 1),

        # cu address map size is num_cus
        ("data", c_uint32*1)
    ]
