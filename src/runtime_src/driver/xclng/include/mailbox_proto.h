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
#define MB_PROTOCOL_VER	 0U

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
	xuid_t uuid;
};


/**
 *	data_kind
 */

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
};

/**
 *	MAILBOX_REQ_PEER_DATA payload type
 */
struct mailbox_subdev_peer {
	enum data_kind kind;
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

#endif /* _XCL_MB_PROTOCOL_H_ */


