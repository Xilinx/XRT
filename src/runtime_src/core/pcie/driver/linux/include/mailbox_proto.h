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
#define XCL_MB_PROTOCOL_VER	0U

/*
 * UUID_SZ should ALWAYS have the same number
 * as the MACRO UUID_SIZE defined in linux/uuid.h
 */
#define XCL_UUID_SZ		16

/**
 * enum mailbox_request - List of all mailbox request OPCODE. Some OP code
 *                        requires arguments, which is defined as corresponding
 *                        data structures below. Response to the request usually
 *                        is a int32_t containing the error code. Some responses
 *                        are more complicated and require a data structure,
 *                        which is also defined below in this file.
 * @XCL_MAILBOX_REQ_UNKNOWN: invalid OP code
 * @XCL_MAILBOX_REQ_TEST_READY: test msg is ready (post only, internal test only)
 * @XCL_MAILBOX_REQ_TEST_READ: fetch test msg from peer (internal test only)
 * @XCL_MAILBOX_REQ_LOCK_BITSTREAM: lock down xclbin on mgmt pf (not implemented)
 * @XCL_MAILBOX_REQ_UNLOCK_BITSTREAM: unlock xclbin on mgmt pf (not implemented)
 * @XCL_MAILBOX_REQ_HOT_RESET: request mgmt pf driver to reset the board
 * @XCL_MAILBOX_REQ_FIREWALL: firewall trip detected on mgmt pf (post only)
 * @XCL_MAILBOX_REQ_LOAD_XCLBIN_KADDR: download xclbin (pointed to by a pointer)
 * @XCL_MAILBOX_REQ_LOAD_XCLBIN: download xclbin (bitstream is in payload)
 * @XCL_MAILBOX_REQ_RECLOCK: set clock frequency
 * @XCL_MAILBOX_REQ_PEER_DATA: read specified data from peer
 * @XCL_MAILBOX_REQ_USER_PROBE: for user pf to probe the peer mgmt pf
 * @XCL_MAILBOX_REQ_MGMT_STATE: for mgmt pf to notify user pf of its state change
 *                          (post only)
 * @XCL_MAILBOX_REQ_CHG_SHELL: shell change is required on mgmt pf (post only)
 * @XCL_MAILBOX_REQ_PROGRAM_SHELL: request mgmt pf driver to reprogram shell
 */
enum xcl_mailbox_request {
	XCL_MAILBOX_REQ_UNKNOWN =		0,
	XCL_MAILBOX_REQ_TEST_READY =		1,
	XCL_MAILBOX_REQ_TEST_READ =		2,
	XCL_MAILBOX_REQ_LOCK_BITSTREAM =	3,
	XCL_MAILBOX_REQ_UNLOCK_BITSTREAM =	4,
	XCL_MAILBOX_REQ_HOT_RESET =		5,
	XCL_MAILBOX_REQ_FIREWALL =		6,
	XCL_MAILBOX_REQ_LOAD_XCLBIN_KADDR =	7,
	XCL_MAILBOX_REQ_LOAD_XCLBIN =		8,
	XCL_MAILBOX_REQ_RECLOCK =		9,
	XCL_MAILBOX_REQ_PEER_DATA =		10,
	XCL_MAILBOX_REQ_USER_PROBE =		11,
	XCL_MAILBOX_REQ_MGMT_STATE =		12,
	XCL_MAILBOX_REQ_CHG_SHELL =		13,
	XCL_MAILBOX_REQ_PROGRAM_SHELL =		14,
	XCL_MAILBOX_REQ_READ_P2P_BAR_ADDR =	15,
	XCL_MAILBOX_REQ_MAX,
	/* Version 0 OP code ends */
};

/**
 * struct mailbox_req_bitstream_lock - MAILBOX_REQ_LOCK_BITSTREAM and
 * 				       MAILBOX_REQ_UNLOCK_BITSTREAM payload type
 * @reserved: reserved for future use
 * @uuid: uuid of the xclbin
 */
struct xcl_mailbox_req_bitstream_lock {
	uint64_t reserved;
	uint8_t uuid[XCL_UUID_SZ];
};

/**
 * enum group_kind - Groups of data that can be fetched from mgmt side
 * @XCL_SENSOR: all kinds of sensor readings
 * @XCL_ICAP: ICAP IP related information
 * @XCL_BDINFO: Board Info, serial_num, mac_address
 * @XCL_MIG_ECC: ECC statistics
 * @XCL_FIREWALL: AF detected time, status
 * @XCL_DNA: Device DNA information
 * @XCL_SUBDEV: PLP subdevices information
 */
enum xcl_group_kind {
	XCL_SENSOR = 0,
	XCL_ICAP,
	XCL_BDINFO,
	XCL_MIG_ECC,
	XCL_FIREWALL,
	XCL_DNA,
	XCL_SUBDEV,
};

/**
 * struct xcl_board_info - Data structure used to fetch BDINFO group
 */
struct xcl_board_info {
	char	 serial_num[256];
	char	 mac_addr0[32];
	char	 mac_addr1[32];
	char	 mac_addr2[32];
	char	 mac_addr3[32];
	char	 revision[256];
	char	 bd_name[256];
	char	 bmc_ver[256];
	uint32_t max_power;
	uint32_t fan_presence;
	uint32_t config_mode;
	char exp_bmc_ver[256];
	uint32_t mac_contiguous_num;
	char     mac_addr_first[6];
};

/**
 * struct xcl_sensor - Data structure used to fetch SENSOR group
 */
struct xcl_sensor {
	uint32_t vol_12v_pex;
	uint32_t vol_12v_aux;
	uint32_t cur_12v_pex;
	uint32_t cur_12v_aux;
	uint32_t vol_3v3_pex;
	uint32_t vol_3v3_aux;
	uint32_t cur_3v3_aux;
	uint32_t ddr_vpp_btm;
	uint32_t sys_5v5;
	uint32_t top_1v2;
	uint32_t vol_1v8;
	uint32_t vol_0v85;
	uint32_t ddr_vpp_top;
	uint32_t mgt0v9avcc;
	uint32_t vol_12v_sw;
	uint32_t mgtavtt;
	uint32_t vcc1v2_btm;
	uint32_t fpga_temp;
	uint32_t fan_temp;
	uint32_t fan_rpm;
	uint32_t dimm_temp0;
	uint32_t dimm_temp1;
	uint32_t dimm_temp2;
	uint32_t dimm_temp3;
	uint32_t vccint_vol;
	uint32_t vccint_curr;
	uint32_t se98_temp0;
	uint32_t se98_temp1;
	uint32_t se98_temp2;
	uint32_t cage_temp0;
	uint32_t cage_temp1;
	uint32_t cage_temp2;
	uint32_t cage_temp3;
	uint32_t hbm_temp0;
	uint32_t cur_3v3_pex;
	uint32_t cur_0v85;
	uint32_t vol_3v3_vcc;
	uint32_t vol_1v2_hbm;
	uint32_t vol_2v5_vpp;
	uint32_t vccint_bram;
	uint32_t version;
	uint32_t oem_id;
	uint32_t vccint_temp;
	uint32_t vol_12v_aux1;
	uint32_t vol_vcc1v2_i;
	uint32_t vol_v12_in_i;
	uint32_t vol_v12_in_aux0_i;
	uint32_t vol_v12_in_aux1_i;
	uint32_t vol_vccaux;
	uint32_t vol_vccaux_pmc;
	uint32_t vol_vccram;
	uint32_t power_warn;
	uint32_t qspi_status;
	uint32_t heartbeat_count;
	uint64_t heartbeat_err_time;
	uint32_t heartbeat_err_code;
};
/**
 * struct xcl_hwicap - Data structure used to fetch ICAP group
 */
struct xcl_pr_region {
	uint64_t freq_0;
	uint64_t freq_1;
	uint64_t freq_2;
	uint64_t freq_3;
	uint64_t freq_cntr_0;
	uint64_t freq_cntr_1;
	uint64_t freq_cntr_2;
	uint64_t freq_cntr_3;
	uint64_t idcode;
	uint8_t uuid[XCL_UUID_SZ];
	uint64_t mig_calib;
	uint64_t data_retention;
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
	uint64_t size;
	uint64_t offset;
	uint64_t data[1];
};
/**
 * struct mailbox_subdev_peer - MAILBOX_REQ_PEER_DATA payload type
 * @kind: data group
 * @size: buffer size for receiving response
 */
struct xcl_mailbox_subdev_peer {
	enum xcl_group_kind kind;
	uint32_t padding;
	uint64_t size;
	uint64_t entries;
	uint64_t offset;
};

/**
 * struct mailbox_conn - MAILBOX_REQ_USER_PROBE payload type
 * @kaddr: KVA of the verification data buffer
 * @paddr: physical addresss of the verification data buffer
 * @crc32: CRC value of the verification data buffer
 * @version: protocol version supported by peer
 */
struct xcl_mailbox_conn {
	uint64_t kaddr;
	uint64_t paddr;
	uint32_t crc32;
	uint32_t version;
};

#define	XCL_COMM_ID_SIZE		2048
#define XCL_MB_PEER_READY		(1UL << 0)
#define XCL_MB_PEER_SAME_DOMAIN		(1UL << 1)
/**
 * struct mailbox_conn_resp - MAILBOX_REQ_USER_PROBE response payload type
 * @version: protocol version should be used
 * @conn_flags: connection status
 * @chan_switch: bitmap to indicate SW / HW channel for each OP code msg
 * @comm_id: user defined cookie
 */
struct xcl_mailbox_conn_resp {
	uint32_t version;
	uint32_t reserved;
	uint64_t conn_flags;
	uint64_t chan_switch;
	char comm_id[XCL_COMM_ID_SIZE];
	uint64_t chan_disable;
};

#define	XCL_MB_STATE_ONLINE		(1UL << 0)
#define	XCL_MB_STATE_OFFLINE	(1UL << 1)
/**
 * struct mailbox_peer_state - MAILBOX_REQ_MGMT_STATE payload type
 * @state_flags: peer state flags
 */
struct xcl_mailbox_peer_state {
	uint64_t state_flags;
};

/**
 * struct mailbox_bitstream_kaddr - MAILBOX_REQ_LOAD_XCLBIN_KADDR payload type
 * @addr: pointer to xclbin body
 */
struct xcl_mailbox_bitstream_kaddr {
	uint64_t addr;
};

/**
 * struct mailbox_clock_freqscaling - MAILBOX_REQ_RECLOCK payload type
 * @region: region of clock
 * @target_freqs: array of target clock frequencies (max clocks: 16)
 */
struct xcl_mailbox_clock_freqscaling {
	unsigned int region;
	unsigned short target_freqs[16];
};

#define XCL_MB_REQ_FLAG_RESPONSE	(1 << 0)
#define XCL_MB_REQ_FLAG_REQUEST		(1 << 1)
/**
 * struct mailbox_req - mailbox request message header
 * @req: opcode
 * @flags: flags of this message
 * @data: payload of variable length
 */
struct xcl_mailbox_req {
	uint64_t flags;
	enum xcl_mailbox_request req;
	char data[1]; /* variable length of payload */
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
struct xcl_sw_chan {
	uint64_t sz;
	uint64_t flags;
	uint64_t id;
	char data[1]; /* variable length of payload */
};

/**
 * struct mailbox_p2p_bar_addr
 * @bar_addr: p2p bar address
 * @bar_len: p2p bar length
 */
struct xcl_mailbox_p2p_bar_addr {
	uint64_t  p2p_bar_addr;
	uint64_t  p2p_bar_len;
};

#endif /* _XCL_MB_PROTOCOL_H_ */
