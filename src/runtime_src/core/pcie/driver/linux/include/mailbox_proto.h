/*
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

#ifndef __KERNEL__
#include <stdint.h>
#endif

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
 * enum mailbox_request - List of all mailbox request OPCODE. Some OP code
 *                        requires arguments, which is defined as corresponding
 *                        data structures below. Response to the request usually
 *                        is a int32_t containing the error code. Some responses
 *                        are more complicated and require a data structure,
 *                        which is also defined below in this file.
 * @MAILBOX_REQ_UNKNOWN: invalid OP code
 * @MAILBOX_REQ_TEST_READY: test msg is ready (post only, internal test only)
 * @MAILBOX_REQ_TEST_READ: fetch test msg from peer (internal test only)
 * @MAILBOX_REQ_LOCK_BITSTREAM: lock down xclbin on mgmt pf
 * @MAILBOX_REQ_UNLOCK_BITSTREAM: unlock xclbin on mgmt pf
 * @MAILBOX_REQ_HOT_RESET: request mgmt pf driver to reset the board
 * @MAILBOX_REQ_FIREWALL: firewall trip detected on mgmt pf (post only)
 * @MAILBOX_REQ_LOAD_XCLBIN_KADDR: download xclbin (pointed to by a pointer)
 * @MAILBOX_REQ_LOAD_XCLBIN: download xclbin (bitstream is in payload)
 * @MAILBOX_REQ_RECLOCK: set clock frequency
 * @MAILBOX_REQ_PEER_DATA: read specified data from peer
 * @MAILBOX_REQ_USER_PROBE: for user pf to probe the peer mgmt pf
 * @MAILBOX_REQ_MGMT_STATE: for mgmt pf to notify user pf of its state change
 *                          (post only)
 * @MAILBOX_REQ_CHG_SHELL: shell change is required on mgmt pf (post only)
 * @MAILBOX_REQ_PROGRAM_SHELL: request mgmt pf driver to reprogram shell
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
	MAILBOX_REQ_CHG_SHELL =		13,
	MAILBOX_REQ_PROGRAM_SHELL =	14,
	/* Version 0 OP code ends */
};

/**
 * struct mailbox_req_bitstream_lock - MAILBOX_REQ_LOCK_BITSTREAM and
 * 				       MAILBOX_REQ_UNLOCK_BITSTREAM payload type
 * @uuid: uuid of the xclbin
 */
struct mailbox_req_bitstream_lock {
	uint64_t reserved;
	uint8_t uuid[UUID_SZ];
};

/**
 * enum group_kind - Groups of data that can be fetched from mgmt side
 * @SENSOR: all kinds of sensor readings
 * @ICAP: ICAP IP related information
 * @MGMT: mgmt side specific information
 */
enum group_kind {
	SENSOR = 0,
	ICAP,
	MGMT,
	MIG_ECC,
	FIREWALL,
	DNA,
	SUBDEV,
};

/**
 * struct xcl_sensor - Data structure used to fetch SENSOR group
 */
struct xcl_sensor {
	uint64_t vol_12v_pex;
	uint64_t vol_12v_aux;
	uint64_t cur_12v_pex;
	uint64_t cur_12v_aux;
	uint64_t vol_3v3_pex;
	uint64_t vol_3v3_aux;
	uint64_t ddr_vpp_btm;
	uint64_t sys_5v5;
	uint64_t top_1v2;
	uint64_t vol_1v8;
	uint64_t vol_0v85;
	uint64_t ddr_vpp_top;
	uint64_t mgt0v9avcc;
	uint64_t vol_12v_sw;
	uint64_t mgtavtt;
	uint64_t vcc1v2_btm;
	uint64_t fpga_temp;
	uint64_t fan_temp;
	uint64_t fan_rpm;
	uint64_t dimm_temp0;
	uint64_t dimm_temp1;
	uint64_t dimm_temp2;
	uint64_t dimm_temp3;
	uint64_t vccint_vol;
	uint64_t vccint_curr;
	uint64_t se98_temp0;
	uint64_t se98_temp1;
	uint64_t se98_temp2;
	uint64_t cage_temp0;
	uint64_t cage_temp1;
	uint64_t cage_temp2;
	uint64_t cage_temp3;
	uint64_t hbm_temp0;
};

/**
 * struct xcl_hwicap - Data structure used to fetch ICAP group
 */
struct xcl_hwicap {
	uint64_t freq_0;
	uint64_t freq_1;
	uint64_t freq_2;
	uint64_t freq_3;
	uint64_t freq_cntr_0;
	uint64_t freq_cntr_1;
	uint64_t freq_cntr_2;
	uint64_t freq_cntr_3;
	uint64_t idcode;
	uint8_t uuid[UUID_SZ];
};

/**
 * struct xcl_common - Data structure used to fetch MGMT group
 */
struct xcl_common {
	uint64_t mig_calib;
};


/**
 * struct xcl_mig_ecc -  Data structure used to fetch MIG_ECC group
 */
struct xcl_mig_ecc {
	uint64_t mem_type;
	uint64_t mem_idx;
	uint64_t ecc_enabled;
	uint64_t ecc_status;
	uint64_t ecc_ce_cnt;
	uint64_t ecc_ue_cnt;
	uint64_t ecc_ce_ffa;
	uint64_t ecc_ue_ffa;
};

/**
 * struct xcl_firewall -  Data structure used to fetch FIREWALL group
 */
struct xcl_firewall {
	uint64_t max_level;
	uint64_t curr_status;
	uint64_t curr_level;
	uint64_t err_detected_status;
	uint64_t err_detected_level;
	uint64_t err_detected_time;
};


/**
 * struct xcl_dna -  Data structure used to fetch DNA group
 */
struct xcl_dna {
	uint64_t status;
	uint32_t dna[4];
	uint64_t capability;
	uint64_t dna_version;
	uint64_t revision;
};
/**
 * Data structure used to fetch SUBDEV group
 */
struct xcl_subdev {
	uint32_t ver;
	int32_t rtncode;
	uint64_t checksum;
	size_t size;
	size_t offset;
	uint64_t data[1];
};
/**
 * struct mailbox_subdev_peer - MAILBOX_REQ_PEER_DATA payload type
 * @kind: data group
 * @size: buffer size for receiving response
 */
struct mailbox_subdev_peer {
	enum group_kind kind;
	size_t size;
	uint64_t entries;
	size_t offset;
};

/**
 * struct mailbox_conn - MAILBOX_REQ_USER_PROBE payload type
 * @kaddr: KVA of the verification data buffer
 * @paddr: physical addresss of the verification data buffer
 * @crc32: CRC value of the verification data buffer
 * @version: protocol version supported by peer
 */
struct mailbox_conn {
	uint64_t kaddr;
	uint64_t paddr;
	uint32_t crc32;
	uint32_t version;
};

#define	COMM_ID_SIZE		2048
#define MB_PEER_READY		(1UL << 0)
#define MB_PEER_SAME_DOMAIN	(1UL << 1)
/**
 * struct mailbox_conn_resp - MAILBOX_REQ_USER_PROBE response payload type
 * @version: protocol version should be used
 * @conn_flags: connection status
 * @chan_switch: bitmap to indicate SW / HW channel for each OP code msg
 * @comm_id: user defined cookie
 */
struct mailbox_conn_resp {
	uint32_t version;
	uint32_t reserved;
	uint64_t conn_flags;
	uint64_t chan_switch;
	char comm_id[COMM_ID_SIZE];
};

#define	MB_STATE_ONLINE		(1UL << 0)
#define	MB_STATE_OFFLINE	(1UL << 1)
/**
 * struct mailbox_peer_state - MAILBOX_REQ_MGMT_STATE payload type
 * @state_flags: peer state flags
 */
struct mailbox_peer_state {
	uint64_t state_flags;
};

/**
 * struct mailbox_bitstream_kaddr - MAILBOX_REQ_LOAD_XCLBIN_KADDR payload type
 * @addr: pointer to xclbin body
 */
struct mailbox_bitstream_kaddr {
	uint64_t addr;
};

/**
 * struct mailbox_clock_freqscaling - MAILBOX_REQ_RECLOCK payload type
 * @region: region of clock
 * @target_freqs: array of target clock frequencies (max clocks: 16)
 */
struct mailbox_clock_freqscaling {
	unsigned int region;
	unsigned short target_freqs[16];
};

#define MB_REQ_FLAG_RESPONSE	(1 << 0)
#define MB_REQ_FLAG_REQUEST	(1 << 1)
#define MB_REQ_FLAG_RECV_REQ	(1 << 2)
/**
 * struct mailbox_req - mailbox request message header
 * @req: opcode
 * @flags: flags of this message
 * @data: payload of variable length
 */
struct mailbox_req {
	enum mailbox_request req;
	uint64_t flags;
	char data[0];
};

/**
 * struct sw_chan - mailbox software channel message metadata. This defines the
 *                  interface between daemons (MPD and MSD) and mailbox's
 *                  read or write callbacks. A mailbox message (either a request
 *                  or response) is wrapped by this data structure as payload.
 *                  A sw_chan is passed between mailbox driver and daemon via
 *                  read / write driver callbacks. And it is also passed between
 *                  MPD and MSD via vendor defined interface (TCP socket, etc).
 * @sz: payload size
 * @flags: flags of this message as in struct mailbox_req
 * @id: message ID
 * @data: payload (struct mailbox_req or response data matching the request)
 */
struct sw_chan {
	uint64_t sz;
	uint64_t flags;
	uint64_t id;
	char data[0]; /* variable length of payload */
};


#endif /* _XCL_MB_PROTOCOL_H_ */
