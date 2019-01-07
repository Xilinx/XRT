##
 # Copyright (C) 2018 Xilinx, Inc
 # Author(s): Ryan Radjabi
 #            Shivangi Agarwal
 #            Sonal Santan
 # ctypes based Python binding for ert.h data structures
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

class ert_cmd_state ():
    ERT_CMD_STATE_NEW = 1
    ERT_CMD_STATE_QUEUED = 2
    ERT_CMD_STATE_RUNNING = 3
    ERT_CMD_STATE_COMPLETED = 4
    ERT_CMD_STATE_ERROR = 5
    ERT_CMD_STATE_ABORT = 6

class ert_cmd_opcode ():
    ERT_START_CU     = 0
    ERT_START_KERNEL = 0
    ERT_CONFIGURE    = 2
    ERT_STOP         = 3
    ERT_ABORT        = 4
    ERT_WRITE        = 5
    ERT_CU_STAT      = 6

class ert_cmd_struct (ctypes.Structure):
    _fields_ = [
        ("state", ctypes.c_uint32, 4),
        ("unused", ctypes.c_uint32, 8),
        ("count", ctypes.c_uint32, 11),
        ("opcode", ctypes.c_uint32, 5),
        ("type", ctypes.c_uint32, 4)
    ]


class uert (ctypes.Union):
#    _anonymous_ = ("ert_cmd_struct")
    _fields_ = [
        ("m_cmd_struct", ert_cmd_struct),
        ("header", ctypes.c_uint32)
    ]

class ert_configure_features (ctypes.Structure):
        # features
    _fields_ = [
        ("ert", ctypes.c_uint32, 1),
        ("polling", ctypes.c_uint32, 1),
        ("cu_dma", ctypes.c_uint32, 1),
        ("cu_isr", ctypes.c_uint32, 1),
        ("cq_int", ctypes.c_uint32, 1),
        ("cdma", ctypes.c_uint32, 1),
        ("unusedf", ctypes.c_uint32, 25),
        ("dsa52", ctypes.c_uint32, 1),
    ]

class ert_configure_cmd (ctypes.Structure):
#    _anonymous_ = "uert"
    _fields_ = [
        ("m_uert", uert),
        #("m_cmd_struct", ert_cmd_struct),

        # payload
        ("slot_size", ctypes.c_uint32),
        ("num_cus", ctypes.c_uint32),
        ("cu_shift", ctypes.c_uint32),
        ("cu_base_addr", ctypes.c_uint32),

#        # features
#        ("ert", ctypes.c_uint32, 1),
#        ("polling", ctypes.c_uint32, 1),
#        ("cu_dma", ctypes.c_uint32, 1),
#        ("cu_isr", ctypes.c_uint32, 1),
#        ("cq_int", ctypes.c_uint32, 1),
#        ("cdma", ctypes.c_uint32, 1),
#        ("unusedf", ctypes.c_uint32, 25),
#        ("dsa52", ctypes.c_uint32, 1),
        ("m_features", ert_configure_features),

        # cu address map size is num_cus
        ("data", ctypes.c_uint32*1)
    ]

class ert_start_cmd_struct (ctypes.Structure):
    _fields_ = [
        ("state", ctypes.c_uint32, 4),
        ("unused", ctypes.c_uint32, 6),
        ("extra_cu_masks", ctypes.c_uint32, 2),
        ("count", ctypes.c_uint32, 11),
        ("opcode", ctypes.c_uint32, 5),
        ("type", ctypes.c_uint32, 4)
    ]

class u_start_ert (ctypes.Union):
    _fields_ = [
        ("m_start_cmd_struct", ert_start_cmd_struct),
        ("header", ctypes.c_uint32)
    ]

class ert_start_kernel_cmd (ctypes.Structure):
    _fields_ = [
        ("m_uert", u_start_ert),

        # payload
        ("cu_mask", ctypes.c_uint32),
        ("data", ctypes.c_uint32*1)
    ]
