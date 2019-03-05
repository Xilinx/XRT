/**
 * Copyright (C) 2019 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#ifndef _XCL_MB_PROTOCOL_H_
#define _XCL_MB_PROTOCOL_H_

/* OPCODE */
enum mailbox_request {
  MAILBOX_REQ_UNKNOWN = 0,
  MAILBOX_REQ_TEST_READY,
  MAILBOX_REQ_TEST_READ,
  MAILBOX_REQ_LOCK_BITSTREAM,
  MAILBOX_REQ_UNLOCK_BITSTREAM,
  MAILBOX_REQ_HOT_RESET,
  MAILBOX_REQ_FIREWALL,
  MAILBOX_REQ_GPCTL,
  MAILBOX_REQ_LOAD_XCLBIN_KADDR,
  MAILBOX_REQ_LOAD_XCLBIN,
  MAILBOX_REQ_RECLOCK,
  MAILBOX_REQ_PEER_DATA,
  MAILBOX_REQ_CONN_EXPL,
  MAILBOX_REQ_CHAN_SWITCH,
};

enum mb_cmd_type {
  MB_CMD_DEFAULT = 0,
  MB_CMD_LOAD_XCLBIN,
  MB_CMD_RECLOCK,
  MB_CMD_CONN_EXPL,
  MB_CMD_LOAD_XCLBIN_KADDR,
  MB_CMD_READ_FROM_PEER,
};

/* struct of MAILBOX_REQ_LOCK_BITSTREAM &
 *         MAILBOX_REQ_UNLOCK_BITSTREAM
 */
struct mailbox_req_bitstream_lock {
  pid_t pid;
  xuid_t uuid;
};

enum data_kind {
  MIG_CALIB,
  DIMM0_TEMP,
  DIMM1_TEMP,
  DIMM2_TEMP,
  DIMM3_TEMP,
  FPGA_TEMP,
  VCC_BRAM,
  CLOCK_FREQ_0,
  CLOCK_FREQ_1,
  FREQ_COUNTER_0,
  FREQ_COUNTER_1,
  VOL_12V_PEX,
  VOL_12V_AUX,
  CUR_12V_PEX,
  CUR_12V_AUX,
  SE98_TEMP0,
  SE98_TEMP1,
  SE98_TEMP2,
  FAN_TEMP,
  FAN_RPM,
  VOL_3V3_PEX,
  VOL_3V3_AUX,
  VPP_BTM,
  VPP_TOP,
  VOL_5V5_SYS,
  VOL_1V2_TOP,
  VOL_1V2_BTM,
  VOL_1V8,
  VCC_0V9A,
  VOL_12V_SW,
  VTT_MGTA,
  VOL_VCC_INT,
  CUR_VCC_INT,
  IDCODE,
  IPLAYOUT_AXLF,
  MEMTOPO_AXLF,
  CONNECTIVITY_AXLF,
  DEBUG_IPLAYOUT_AXLF,
  PEER_CONN,
  XCLBIN_UUID,
  CHAN_STATE,
  CHAN_SWITCH,
};

/* struct of MAILBOX_REQ_PEER_DATA
 */
struct mailbox_subdev_peer {
    enum data_kind kind;
};

/* struct of MAILBOX_REQ_CONN_EXPL
 *           MAILBOX_REQ_CHAN_SWITCH
 */
struct mailbox_conn{
  uint64_t flag;
  uint64_t kaddr;
  uint64_t paddr;
  uint32_t crc32;
  uint32_t ver;
  uint64_t sec_id;
};


/* struct of MAILBOX_REQ_LOAD_XCLBIN_KADDR
 *
 */
struct mailbox_bitstream_kaddr {
  uint64_t addr;
};

struct mailbox_gpctl {
  enum mb_cmd_type cmd_type;
  uint32_t data_total_len;
  uint64_t priv_data;
  void *data_ptr;
};
/* MAILBOX_REQ_LOAD_XCLBIN */

/*
 * MAILBOX_REQ_RECLOCK
 */
struct mailbox_clock_freqscaling {
  unsigned region;
  unsigned short target_freqs[4];
};

struct mailbox_req {
  enum mailbox_request req;
  uint32_t data_total_len;
  uint64_t flags;
  char data[0];
};

#define MB_PROT_VER_MAJOR 0
#define MB_PROT_VER_MINOR 5
#define MB_PROTOCOL_VER   ((MB_PROT_VER_MAJOR<<8) + MB_PROT_VER_MINOR)

#define MB_PEER_CONNECTED         (0x1 << 0)
#define MB_PEER_SAME_DOM          (0x1 << 1)
#define MB_PEER_SW_CHAN_EN        (0x1 << 2)
#define MB_PEER_SAMEDOM_CONNECTED (MB_PEER_CONNECTED | MB_PEER_SAME_DOM)

#define MB_SW_ENABLE_LOCK         (0x1 << 3)
#define MB_SW_ENABLE_UNLOCK       (0x1 << 4)
#define MB_SW_ENABLE_HOT_RESET    (0x1 << 5)
#define MB_SW_ENABLE_FIREWALL     (0x1 << 6)
#define MB_SW_ENABLE_GPCTL        (0x1 << 7)
#define MB_SW_ENABLE_XCLBIN_KADDR (0x1 << 8)
#define MB_SW_ENABLE_XCLBIN       (0x1 << 9)
#define MB_SW_ENABLE_RECLOCK      (0x1 << 10)
#define MB_SW_ENABLE_PEER_DATA    (0x1 << 11)
#define MB_SW_ENABLE_CONN_EXPL    (0x1 << 12)
#define MB_SW_ENABLE_CHAN_SWITCH  (0x1 << 13)
#endif /* _XCL_MB_PROTOCOL_H_ */


