/*******************************************************************************
 *
 * Xilinx DMA IP Core Linux Driver
 * Copyright(c) 2017 Xilinx, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "LICENSE".
 *
 ******************************************************************************/
#ifndef __LIBQDMA_EXPORT_API_H__
#define __LIBQDMA_EXPORT_API_H__

#include <linux/types.h>
#include <linux/interrupt.h>
#include "libqdma_config.h"

/**
 * DOC: Xilinx QDMA IP Core Library Interface Definitions
 *
 * Header file "libqdma_export.h" defines the data structures and function
 * prototypes exported by the QDMA Core libary.
 */

#define QDMA_FUNC_ID_INVALID	(QDMA_PF_MAX + QDMA_VF_MAX)

/*
 * QDMA Error codes
 */
typedef enum {
	QDMA_OPERATION_SUCCESSFUL		= 0,	/* QDMA driver API operation successful         */
	QDMA_ERR_PCI_DEVICE_NOT_FOUND		= -1,	/* QDMA PCI device not found on the PCIe bus    */
	QDMA_ERR_PCI_DEVICE_ALREADY_ATTACHED	= -2,	/* QDMA PCI device already attached             */
	QDMA_ERR_PCI_DEVICE_ENABLE_FAILED	= -3,	/* Failed to enable the QDMA PCIe device        */
	QDMA_ERR_PCI_DEVICE_INIT_FAILED		= -4,	/* Failed to initialize the QDMA PCIe device    */
	QDMA_ERR_INVALID_INPUT_PARAM		= -5,	/* Invalid input parameter given to QDMA API    */
	QDMA_ERR_INVALID_PCI_DEV		= -6,	/* Invalid PCIe device                          */
	QDMA_ERR_INVALID_QIDX			= -7,	/* Invalid Queue ID provided as input           */
	QDMA_ERR_INVALID_DESCQ_STATE		= -8,	/* Invalid descriptor queue state               */
	QDMA_ERR_INVALID_DIRECTION		= -9,	/* Invalid descriptor direction provided        */
	QDMA_ERR_DESCQ_SETUP_FAILED		= -10,	/* Failed to setup the descriptor queue         */
	QDMA_ERR_DESCQ_FULL			= -11,	/* Descriptor queue is full                     */
	QDMA_ERR_DESCQ_IDX_ALREADY_ADDED	= -12,	/* Descriptor queue index is already added      */
	QDMA_ERR_QUEUE_ALREADY_CONFIGURED	= -13,	/* Queue is already configured                  */
	QDMA_ERR_OUT_OF_MEMORY			= -14,	/* Out of memory                                */
	QDMA_ERR_INVALID_QDMA_DEVICE	= -15,	/* Invalid QDMA device, QDMA device is not yet created   */
	QDMA_ERR_INTERFACE_NOT_ENABLED_IN_DEVICE = -16, /* The ST or MM or Both interface not enabled in the device */
} qdma_error_codes;

struct pci_dev;

/**
 * DOC: libqdma Initialization and Cleanup
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

/**
 * libqdma_init()       initialize the QDMA core library
 *
 * Return: 0 success, < 0 in the case error
 */
int libqdma_init(void);

/**
 * libqdma_exit()       cleanup the QDMA core library before exiting
 *
 * Return: none
 */
void libqdma_exit(void);

/**
 * DOC: qdma device management
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

/**
 * qdma_dev_conf defines the per-device qdma property.
 *
 * NOTE: if any of the max requested is less than supported, the value will
 *       be updated
 */
enum intr_ring_size_sel {
	INTR_RING_SZ_4KB = 0,	/* 0 */
	INTR_RING_SZ_8KB,	/* 1 */
	INTR_RING_SZ_12KB,	/* 2 */
	INTR_RING_SZ_16KB,	/* 3 */
	INTR_RING_SZ_20KB,	/* 4 */
	INTR_RING_SZ_24KB,	/* 5 */
	INTR_RING_SZ_28KB,	/* 6 */
	INTR_RING_SZ_32KB,	/* 7 */
};


#define QDMA_DEV_NAME_MAXLEN	32
#define QDMA_DEV_MSIX_VEC_MAX	8
struct qdma_dev_conf {
	struct pci_dev *pdev;

	unsigned short qsets_max; /* max. of queue pairs */
	unsigned short rsvd2;

	u8 poll_mode:1;		/* poll or interrupt */
	u8 intr_agg:1;		/* poll_mode=0, enable intrrupt aggregation */
	u8 zerolen_dma:1;	/* zero length DMA allowed */
	u8 master_pf:1;		/* master pf: for control of CSR settings */
	u8 rsvd1:4;

	u8 vf_max;		/* PF only: max # VFs to be enabled */
	u8 intr_rngsz;		/* intr_agg=1, intr_ring_size_sel */

	/*
	 * interrupt:
	 * - MSI-X only
	 * max of QDMA_DEV_MSIX_VEC_MAX per function, 32 in Everest
 	 * - 1 vector is reserved for user interrupt
 	 * - 1 vector is reserved mailbox
	 * - 1 vector on pf0 is reserved for error interrupt
	 * - the remaining vectors will be used for queues
	 */

	/* max. of vectors used for queues. libqdma update w/ actual # */
	u8 msix_qvec_max;

	unsigned long uld;	/* upper layer data, i.e. callback data */

	/* user interrupt, if null, default libqdma handler is used */
	void (*fp_user_isr_handler)(unsigned long dev_hndl, unsigned long uld);

	/*
	 * example flow of ST C2H:
	 * a. interrupt fires 
	 * b. Hard IRQ: libqdma isr top -> dev->fp_q_isr_top_dev ->
	 *	isr_top_qproc && Q->fp_descq_isr_top
	 * c. Soft IRQ:
	 *	irq handler
	 *	qdma_queue_service_bh() ->
	 *	if rx: Q->fp_descq_rx_packet() called for each packet
	 *	qdma_queue_cmpl_ctrl(set=true) to update h/w and re-enable
	 *	interrupt
	 */

	/* Q interrupt top, per-device addtional handling code */
	void (*fp_q_isr_top_dev)(unsigned long dev_hndl, unsigned uld);

	/* filled in by libqdma */
	char name[QDMA_DEV_NAME_MAXLEN]; /* an unique string identify the dev.
					    current format: qdma<pf|vf><idx> */
	u8 idx;				/* device index */
	char bar_num_config;		/* dma config bar #, < 0 not present */
	char bar_num_user;		/* user bar, PF only */
	char rsvd;
	unsigned int qsets_base;
};

/**
 * qdma_device_open - read the pci bars and configure the fpga
 *	should be called from probe()
 * 	NOTE:
 *		user interrupt will not enabled until qdma_user_isr_enable()
 *		is called
 * @mod_name: the module name, used for request_irq
 * @conf: device configuration
 * @dev_hndl: a opaque handle (for libqdma to identify the device)
 * returns
 *	QDMA_OPERATION_SUCCESSFUL success, < 0 in case of error
 */
int qdma_device_open(const char *mod_name, struct qdma_dev_conf *conf,
				unsigned long *dev_hndl);

/**
 * qdma_device_close - prepare fpga for removal: disable all interrupts (users
 * and qdma) and release all resources
 *	should called from remove()
 * @pdev: ptr to struct pci_dev
 * @dev_hndl: dev_hndl retured from qdma_device_open()
 */
void qdma_device_close(struct pci_dev *pdev, unsigned long dev_hndl);

/**
 * qdma_device_offline - offline the fpga
 * qdma_device_online - online the fpga, re-initialze
 * qdma_device_flr_set - start pre-flr processing
 * qdma_device_flr_check - check if pre-flr processing completed
 * @pdev: ptr to struct pci_dev
 * @dev_hndl: dev_hndl retured from qdma_device_open()
 * returns:	0 - completed, < 0, error
 */
void qdma_device_offline(struct pci_dev *pdev, unsigned long dev_hndl);
int qdma_device_online(struct pci_dev *pdev, unsigned long dev_hndl);
int qdma_device_flr_quirk_set(struct pci_dev *pdev, unsigned long dev_hndl);
int qdma_device_flr_quirk_check(struct pci_dev *pdev, unsigned long dev_hndl);
/**
 * qdma_device_get_config - retrieve the current device configuration
 * @dev_hndl: dev_hndl retured from qdma_device_open()
 * @ebuf, ebuflen: error message buffer, can be NULL/0 (i.e., optional)
 */
struct qdma_dev_conf *qdma_device_get_config(unsigned long dev_hndl,
				char *ebuf, int ebuflen);

/*
 * qdma_device_sriov_config - configure sriov
 * @pdev: ptr to struct pci_dev
 * @dev_hndl: dev_hndl retured from qdma_device_open()
 * @num_vfs: # of VFs to be instantiated
 */
int qdma_device_sriov_config(struct pci_dev *pdev, unsigned long dev_hndl,
				int num_vfs);

/*
 * qdma_device_read_config_register - read dma config. register
 * @dev_hndl: dev_hndl retured from qdma_device_open()
 * @reg_addr: register address
 * return value of the register
 */
unsigned int qdma_device_read_config_register(unsigned long dev_hndl,
					unsigned int reg_addr);

/*
 * qdma_device_write_config_register - write dma config. register
 * @dev_hndl: dev_hndl retured from qdma_device_open()
 * @reg_addr: register address
 * @value: register value to be writen
 */
void qdma_device_write_config_register(unsigned long dev_hndl,
					unsigned int reg_addr, u32 value);

/*
 * qdma_glbal_csr_get - retrieve the global csr settings
 * qdma_glbal_csr_set - set the global csr values
 *	NOTE: for set, libqdma will enforce the access control
 *
 * @dev_hndl: hndl retured from qdma_device_open()
 * @csr_conf: data structures to hold the csr values
 * returns
 *	0: success
 *	<0: error
 */
#define QDMA_GLOBAL_CSR_ARRAY_SZ        16
struct global_csr_conf {
	unsigned int ring_sz[QDMA_GLOBAL_CSR_ARRAY_SZ];
			/* descriptor ring size, ie. queue depth */
	unsigned int c2h_timer_cnt[QDMA_GLOBAL_CSR_ARRAY_SZ];
	unsigned int c2h_cnt_th[QDMA_GLOBAL_CSR_ARRAY_SZ];
	unsigned int c2h_buf_sz[QDMA_GLOBAL_CSR_ARRAY_SZ];
	unsigned int wb_acc;
};

int qdma_global_csr_get(unsigned long dev_handle, struct global_csr_conf *csr);

int qdma_global_csr_set(unsigned long dev_handle, struct global_csr_conf *csr);


/**
 * qdma_queue_conf defines the per-dma Q property.
 *
 * NOTE: if any of the max requested is less than supported, the value will
 *	be updated
 */
enum desc_sz_t {
	DESC_SZ_8B = 0,
	DESC_SZ_16B,
	DESC_SZ_32B,
	DESC_SZ_RSV
};

enum tigger_mode_t {
	TRIG_MODE_DISABLE,	/* 0 */
	TRIG_MODE_ANY,		/* 1 */
	TRIG_MODE_TIMER,	/* 2 */
	TRIG_MODE_COUNTER,	/* 3 */
	TRIG_MODE_COMBO,	/* 4 */
	TRIG_MODE_USER,		/* 5 */
};

struct qdma_sw_sg {
	struct qdma_sw_sg *next;

	struct page *pg;
	unsigned int offset;
	unsigned int len;
	dma_addr_t dma_addr;
};

#define QDMA_QUEUE_NAME_MAXLEN	32
#define QDMA_QUEUE_IDX_INVALID	0xFFFF
#define QDMA_QUEUE_VEC_INVALID	0xFF	/* msix_vec_idx */
struct qdma_queue_conf {
	unsigned short qidx;	/* 0xFFFF: libqdma choose the queue idx
				   0 ~ (qdma_dev_conf.qsets_max - 1)
				   the calling function choose the queue idx */

	/* config flags: byte #1 */
	u8 st:1;
	u8 c2h:1;
	u8 pipe:1;		/* SDx only: inter-kernel communcation pipe */
        u8 irq_en:1;		/* poll or interrupt */

	/*
	 * descriptor ring
	 */
        u8 desc_rng_sz_idx:4;	/* global_csr_conf.ringsz[N] */

	/* config flags: byte #2 */
        u8 wbk_en:1;		/* writeback enable, disabled for ST C2H */
        u8 wbk_acc_en:1;        /* sw context.wbi_acc_en */
        u8 wbk_pend_chk:1;      /* sw context.wbi_chk */
        u8 bypass:1;		/* send descriptor to bypass out */

	u8 pfetch_en:1;         /* descriptor prefetch enable control */
	u8 fetch_credit:1;      /* sw context.frcd_en[32] */
        u8 st_pkt_mode:1;	/* SDx only: ST packet mode (i.e., with TLAST
				   to identify the packet boundary) */
        u8 c2h_use_fl:1;	/* c2h use pre-alloc free list */

	/* config flags: byte #3 */
        u8 c2h_buf_sz_idx:4;	/* global_csr_conf.c2h_buf_sz[N] */

	/* 
	 * ST C2H Completion/Writeback ring
	 */

        u8 cmpl_rng_sz_idx:4;	/* global_csr_conf.ringsz[N] */

	/* config flags: byte #4 */
        u8 cmpl_desc_sz:2;	/* C2H ST wrb + immediate data, desc_sz_t */
        u8 cmpl_stat_en:1;	/* enable status desc. for WRB */
	u8 cmpl_udd_en:1;	/* C2H Completion entry user-defined data */

        u8 cmpl_timer_idx:4;	/* global_csr_conf.c2h_timer_cnt[N] */

	/* config flags: byte #5 */
	u8 cmpl_cnt_th_idx:4;	/* global_csr_conf.c2h_cnt_th[N] */
	u8 cmpl_trig_mode:3;	/* tigger_mode_t */
	u8 cmpl_en_intr:1;	/* enable interrupt for WRB */

	u8 rsvd;

	/*
	 * TODO: for Platform streaming DSA
	 */
	/* only if pipe = 1 */
	u8 cdh_max;			/* max 16. CDH length per packet */
	u8 gl_max;			/* <= 7, max # gather buf. per packet */
	u32 pipe_flow_id;
	u32 pipe_route_id;
	u32 pipe_stm_qdepth;		/* depth at STM */
	u32 pipe_stm_max_pkt_sz;	/* <= 64K-1, hw STM buffer size */

	/* user provided per-Q irq handler */
	unsigned long quld;		/* set by user for per Q data */

	/* TBA: Q interrupt top, per-queue addtional handling code
	 * for example, network rx: napi_schedule(&Q->napi) */
	void (*fp_descq_isr_top)(unsigned long qhndl, unsigned long quld);
	/*
	 * optional rx packet handler:
	 *	 called from irq BH (i.e.qdma_queue_service_bh())
	 * - udd: user defined data in the completion entry
	 * - sgcnt / sgl: packet data in scatter-gather list
	 *   NOTE: a. do NOT modify any field of sgl
	 *	   b. if zero copy, do a get_page() to prevent page freeing
	 *	   c. do loop through the sgl with sg->next and stop
	 *	      at sgcnt. the last sg may not have sg->next = NULL
	 * returns:
	 *	- 0 to allow libqdma free/re-task the sgl
	 *	- < 0, libqdma will keep the packet for processing again
	 *
	 * A single packet could contain:
	 * in the case of c2h_udd_en = 1:
	 * - udd only (udd valid, sgcnt = 0, sgl = NULL), or
	 * - udd + packdet data 
	 * in the case of c2h_udd_en = 0:
	 * - packet data (udd = NULL, sgcnt > 0 and sgl valid)
	 */
	int (*fp_descq_c2h_packet)(unsigned long qhndl, unsigned long quld,
				unsigned int len, unsigned int sgcnt,
				struct qdma_sw_sg *sgl, void *udd);

	/* fill in by libqdma */
	char name[QDMA_QUEUE_NAME_MAXLEN];
	unsigned int rngsz;
	unsigned int rngsz_wrb;
	unsigned int c2h_bufsz;
};

/*
 * qdma_queue_add - add a queue
 * qdma_queue_start - start a queue (i.e, online, ready for dma)
 * qdma_queue_stop - stop a queue (i.e., offline, NOT ready for dma)
 * qdma_queue_remove - remove/delete a queue
 *
 * @dev_hndl: dev_hndl retured from qdma_device_open()
 * @cnt: # of queues to be configured
 * @ebuf, ebuflen: error message buffer, can be NULL/0 (i.e., optional)
 * @qhndl: list of unsigned long values that are the opaque qhndl

 * return < 0 in case of error
 */
int qdma_queue_add(unsigned long dev_hndl, struct qdma_queue_conf *qconf,
			unsigned long *qhndl, char *ebuf, int ebuflen);
int qdma_queue_start(unsigned long dev_hndl, unsigned long qhndl,
                     char *buf, int buflen);
int qdma_queue_stop(unsigned long dev_hndl, unsigned long qhndl, char *buf,
				int buflen);
int qdma_queue_remove(unsigned long dev_hndl, unsigned long qhndl, char *buf,
				int buflen);

/**
 * queue helper/debug functions
 */

/*
 * qdma_queue_get_config - retrieve the configuration of a queue
 * qdma_queue_list - display all configured queues in a string buffer
 * qdma_queue_dump - display a queue's state in a string buffer
 * qdma_queue_dump_desc - display a queue's descriptor ring from index start
 * 				~ end in a string buffer
 * qdma_queue_dump_wrb -  display a queue's descriptor ring from index start
 * 				~ end in a string buffer
 *
 * @dev_hndl: dev_hndl retured from qdma_device_open()
 * @qhndl: an opaque queue handle of type unsigned long
 * @buf, buflen: message buffer
 *
 * return 	on error: < 0
 * 		on success: if optional message buffer used then strlen of buf,
 * 			    otherwise QDMA_OPERATION_SUCCESSFUL
 */
struct qdma_queue_conf *qdma_queue_get_config(unsigned long dev_hndl,
				unsigned long qhndl, char *buf, int buflen);
int qdma_queue_list(unsigned long dev_hndl, char *buf, int buflen);
int qdma_queue_dump(unsigned long dev_hndl, unsigned long qhndl, char *buf,
				int buflen);
int qdma_queue_dump_desc(unsigned long dev_hndl, unsigned long qhndl,
				unsigned int start, unsigned int end, char *buf,
				int buflen);
int qdma_queue_dump_wrb(unsigned long dev_hndl, unsigned long qhndl,
				unsigned int start, unsigned int end, char *buf,
				int buflen);
#ifdef ERR_DEBUG
int qdma_queue_set_err_indcution(unsigned long dev_hndl, unsigned long id,
				 u64 err_sel, u64 err_mask, char *buf,
				 int buflen);
#endif

/*
 * qdma_request_submit - submit a scatter-gather list of data for dma operation
 *                      (for both read and write)
 * @dev_hndl: dev_hndl retured from qdma_device_open()
 * @qhndl: list of unsigned long values that are the opaque qhndl
 * return # of bytes transfered or
 *	 < 0 in case of error
 */

#define QDMA_REQ_OPAQUE_SIZE 	128
#define QDMA_UDD_MAXLEN		32
struct qdma_request {
	/* private to the dma driver, do NOT touch */
	unsigned char opaque[QDMA_REQ_OPAQUE_SIZE];

	/* filled in by the calling function */
	unsigned long uld_data;		/* for the calling function */
	int (*fp_done)(struct qdma_request *, unsigned int bytes_done, int err);
					/* set fp_done for non-blocking mode */
	unsigned int timeout_ms;	/* timeout in mili-seconds,
					   0 - no timeout */
	unsigned int count;		/* total data size */

	u64 ep_addr;			/* MM only, DDR/BRAM memory addr */
	u8 write:1;			/* write: if write to the device */
	u8 dma_mapped:1;		/* if sgt is already dma mapped */
	u8 udd_len; 			/* user defined data present */
	unsigned short sgcnt;		/* # of scatter-gather entries < 64K */
	struct qdma_sw_sg *sgl;		/* scatter-gather list of data bufs */
	u8 udd[QDMA_UDD_MAXLEN];	/* udd data */
};

ssize_t qdma_request_submit(unsigned long dev_hndl, unsigned long qhndl,
			struct qdma_request *req);

/*
 * qdma_queue_c2h_peek - peek a receive (c2h) queue
 *
 * @dev_hndl: hndl retured from qdma_device_open()
 * @qhndl: list of unsigned long values that are the opaque qhndl
 *
 * filled in by libqdma:
 * @udd_len: # of udd received
 * @pkt_len: # of packets received
 * @data_len: # of bytes of packet data received
 *
 * return # of packets received in the Q
 * return 0 if success
 *      < 0 in case of error
 */
int qdma_queue_c2h_peek(unsigned long dev_hndl, unsigned long qhndl,
			unsigned int *udd_cnt, unsigned int *pkt_cnt,
			unsigned int *data_len);

/*
 * qdma_queue_avail_desc - query of # of free descriptor
 *
 * @dev_hndl: hndl retured from qdma_device_open()
 * @qhndl: hndl retured from qdma_queue_add()
 *
 * return # of available desc in the queue
 *	 < 0 in case of error
 */
int qdma_queue_avail_desc(unsigned long dev_hndl, unsigned long qhndl);

/*
 * packet/streaming interfaces
 */

/*
 * qdma_queue_cmpl_ctrl - read/set the c2h Q's completion control
 *
 * @dev_hndl: hndl retured from qdma_device_open()
 * @qhndl: hndl retured from qdma_queue_add()
 * @cctrl: completion control
 * @set: read or set
 *
 * return 0 if success, < 0 otherwise
 */
struct qdma_cmpl_ctrl {
	u8 cnt_th_idx:4;	/* global_csr_conf.c2h_cnt_th[N] */
	u8 timer_idx:4;		/* global_csr_conf.c2h_timer_cnt[N] */

	u8 trigger_mode:3;	/* tigger_mode_t */
	u8 en_stat_desc:1;	/* enable status desc. for WRB */
	u8 cmpl_en_intr:1;	/* enable interrupt for WRB */
};

int qdma_queue_cmpl_ctrl(unsigned long dev_hndl, unsigned long qhndl,
				struct qdma_cmpl_ctrl *cctrl, bool set);

/*
 * qdma_read_packet - read rcv'ed data (ST C2H dma operation)
 *
 * @dev_hndl: hndl retured from qdma_device_open()
 * @qhndl: hndl retured from qdma_queue_add()
 * @req: pointer to the list of packet data
 * @cctrl: completion control, if no change is desired, set it to NULL
 *
 * return # of packet read
 */
int qdma_queue_packet_read(unsigned long dev_hndl, unsigned long qhndl,
			struct qdma_request *req, struct qdma_cmpl_ctrl *cctrl);

/*
 * qdma_queue_write_packet - submit data for h2c dma operation
 *
 * @dev_hndl: hndl retured from qdma_device_open()
 * @qhndl: hndl retured from qdma_queue_add()
 * @flag: TBD, write control flag
 * @req: pointer to the list of packet data
 * @callback_parm: parameter passed into fp_dma_cmpl()
 *
 * return # of desc posted
 *	 < 0 in case of error
 */
int qdma_queue_packet_write(unsigned long dev_hndl, unsigned long qhndl,
			struct qdma_request *req);

/*
 * qdma_queue_service - service the queue
 *	in the case of irq handler is registered by the user, the user should
 *	call qdma_queue_service() in its interrupt handler to service the queue
 *
 * @dev_hndl: hndl retured from qdma_device_open()
 * @qhndl: hndl retured from qdma_queue_add()
 */
void qdma_queue_service(unsigned long dev_hndl, unsigned long qhndl);


/*
 * qdma_intr_ring_dump - display the interrupt ring info of a vector
 *
 * @dev_hndl: dev_hndl retured from qdma_device_open()
 * @vector_idx: vector number
 * @start_idx: interrupt start idx
 * @vector_idx: interrupt end idx
 * @buf, buflen: message buffer
 *
 * return < 0 in case of error
 */
int qdma_intr_ring_dump(unsigned long dev_hndl, unsigned int vector_idx,
			int start_idx, int end_idx, char *buf, int buflen);

#endif
