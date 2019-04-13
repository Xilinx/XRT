/**
 * Copyright (C) 2019 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *		 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#ifndef _XCL_MB_PROTOCOL_H_
#define _XCL_MB_PROTOCOL_H_

/*
 * This header file contains mailbox protocol b/w mgmt and user pfs.
 * - Any changes made here should maintain backward compatibility.
 * - If it's not possible, new OP code should be added and version number should
 *   be bumped up.
 * - Support for old OP code should never be removed.
 */
#define MB_PROTOCOL_VER	0U

/*
 * UUID_SZ should ALWAYS have the same number 
 * as the MACRO UUID_SIZE defined in linux/uuid.h
 */
#define UUID_SZ		16
/**
 *	mailbox_req OPCODE
 */
enum mailbox_request {
	MAILBOX_REQ_UNKNOWN =		0,
	MAILBOX_REQ_TEST_READY =	1,
	MAILBOX_REQ_TEST_READ =		2,
	MAILBOX_REQ_LOCK_BITSTREAM =	3,
	MAILBOX_REQ_UNLOCK_BITSTREAM =	4,
	MAILBOX_REQ_HOT_RESET =		5,
	MAILBOX_REQ_FIREWALL =		6,
	MAILBOX_REQ_LOAD_XCLBIN_KADDR =	7,
	MAILBOX_REQ_LOAD_XCLBIN =	8,
	MAILBOX_REQ_RECLOCK =		9,
	MAILBOX_REQ_PEER_DATA =		10,
	MAILBOX_REQ_USER_PROBE =	11,
	MAILBOX_REQ_MGMT_STATE =	12,
	/* Version 0 OP code ends */
};

/**
 *	MAILBOX_REQ_LOCK_BITSTREAM &
 *	MAILBOX_REQ_UNLOCK_BITSTREAM payload type
 */

struct mailbox_req_bitstream_lock {
	uint64_t reserved;
	u8 uuid[UUID_SZ];
};


enum group_kind {
	SENSOR = 0,
	ICAP,
	MGMT,
};

struct xcl_sensor {
	u64 vol_12v_pex;
	u64 vol_12v_aux;
	u64 cur_12v_pex;
	u64 cur_12v_aux;
	u64 vol_3v3_pex;
	u64 vol_3v3_aux;
	u64 ddr_vpp_btm;
	u64 sys_5v5;
	u64 top_1v2;
	u64 vol_1v8;
	u64 vol_0v85;
	u64 ddr_vpp_top;
	u64 mgt0v9avcc;
	u64 vol_12v_sw;
	u64 mgtavtt;
	u64 vcc1v2_btm;
	u64 fpga_temp;
	u64 fan_temp;
	u64 fan_rpm;
	u64 dimm_temp0;
	u64 dimm_temp1;
	u64 dimm_temp2;
	u64 dimm_temp3;
	u64 vccint_vol;
	u64 vccint_curr;
	u64 se98_temp0;
	u64 se98_temp1;
	u64 se98_temp2;
	u64 cage_temp0;
	u64 cage_temp1;
	u64 cage_temp2;
	u64 cage_temp3;
};

struct xcl_hwicap {
	u64 freq_0;
	u64 freq_1;
	u64 freq_2;
	u64 freq_3;
	u64 freq_cntr_0;
	u64 freq_cntr_1;
	u64 freq_cntr_2;
	u64 freq_cntr_3;
	u64 idcode;
	u8 uuid[UUID_SZ];
};

struct xcl_common {
	u64 mig_calib;
};

/**
 *	MAILBOX_REQ_PEER_DATA payload type
 */
struct mailbox_subdev_peer {
	enum group_kind kind;
	size_t size;
};

/**
 *	MAILBOX_REQ_USER_PROBE payload type
 */
struct mailbox_conn {
	uint64_t kaddr;
	uint64_t paddr;
	uint32_t crc32;
	uint32_t version;
	uint64_t sec_id;
};

/**
 *	MAILBOX_REQ_USER_PROBE response payload type
 */
#define	MB_COMM_ID_LEN		256
#define MB_CONN_CONNECTED	(1UL << 0)
#define MB_CONN_SAME_DOMAIN	(1UL << 1)
struct mailbox_conn_resp {
	uint32_t version;
	uint32_t reserved;
	uint64_t conn_flags;
	uint64_t chan_switch;
	char comm_id[MB_COMM_ID_LEN];
};

/**
 *	MAILBOX_REQ_MGMT_STATE payload type
 */
#define	MB_STATE_ONLINE		(1UL << 0)
#define	MB_STATE_OFFLINE	(1UL << 1)
struct mailbox_peer_state {
	uint64_t state_flags;
};

/**
 *	MAILBOX_REQ_LOAD_XCLBIN_KADDR payload type
 */
struct mailbox_bitstream_kaddr {
	uint64_t addr;
};

/**
 *	MAILBOX_REQ_RECLOCK payload type
 */
struct mailbox_clock_freqscaling {
	unsigned int region;
	unsigned short target_freqs[4];
};

/**
 *	mailbox_req header
 *	req:				opcode
 *	data_len:			payload size
 *	flags:				reserved
 *	data:				payload
 */
struct mailbox_req {
	enum mailbox_request req;
	uint32_t data_len;
	uint64_t flags;
	char data[0];
};
/**
 *  BITMAP meanings of flags field in struct mailbox_req
 */
#define MB_REQ_FLAG_RESPONSE	(1 << 0)
#define MB_REQ_FLAG_REQUEST	(1 << 1)
#define MB_REQ_FLAG_RECV_REQ	(1 << 2)

#endif /* _XCL_MB_PROTOCOL_H_ */



