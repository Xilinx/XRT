/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
 *
 * Authors: Chien-Wei Lan <chienwei@xilinx.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "../xocl_drv.h"
#include "xrt_ert.h"

#define	ERT_MAX_SLOTS		128
#define	CTRL_SLOT		0


#define CQ_STATUS_OFFSET	(ERT_CQ_STATUS_REGISTER_ADDR - ERT_CSR_ADDR)

//#define	SCHED_VERBOSE	1

#ifdef SCHED_VERBOSE
#define	CMDQUEUE_ERR(cmd_queue, fmt, arg...)	\
	xocl_err(cmd_queue->dev, fmt "", ##arg)
#define	CMDQUEUE_WARN(cmd_queue, fmt, arg...)	\
	xocl_warn(cmd_queue->dev, fmt "", ##arg)
#define	CMDQUEUE_INFO(cmd_queue, fmt, arg...)	\
	xocl_info(cmd_queue->dev, fmt "", ##arg)
#define	CMDQUEUE_DBG(cmd_queue, fmt, arg...)	\
	xocl_info(cmd_queue->dev, fmt "", ##arg)

#else
#define	CMDQUEUE_ERR(cmd_queue, fmt, arg...)	\
	xocl_err(cmd_queue->dev, fmt "", ##arg)
#define	CMDQUEUE_WARN(cmd_queue, fmt, arg...)	\
	xocl_warn(cmd_queue->dev, fmt "", ##arg)
#define	CMDQUEUE_INFO(cmd_queue, fmt, arg...)	\
	xocl_info(cmd_queue->dev, fmt "", ##arg)
#define	CMDQUEUE_DBG(cmd_queue, fmt, arg...)
#endif

extern int kds_echo;

static const unsigned int no_index = -1;

struct command_queue {
	struct device		*dev;
	struct platform_device	*pdev;
	void __iomem		*cfg_gpio;
	void __iomem		*cq_base;
	uint64_t		cq_range;
	bool			polling_mode;
	struct ert_queue	queue;

	unsigned int		num_slots;
	unsigned int            slot_size;
	struct list_head	sq;
	uint32_t		sq_num;
	// Bitmap tracks busy(1)/free(0) slots in command_queue
	DECLARE_BITMAP(slot_status, ERT_MAX_SLOTS);

	struct xrt_ert_command	*submit_queue[ERT_MAX_SLOTS];
	void			*ert_handle;
};

static ssize_t
ert_cq_debug(struct file *filp, struct kobject *kobj,
	    struct bin_attribute *attr, char *buf,
	    loff_t offset, size_t count)
{
	struct command_queue *cmd_queue;
	struct device *dev = container_of(kobj, struct device, kobj);
	ssize_t nread = 0;
	size_t size = 0;

	cmd_queue = (struct command_queue *)dev_get_drvdata(dev);
	if (!cmd_queue || !cmd_queue->cq_base)
		return nread;

	size = cmd_queue->cq_range;
	if (offset >= size)
		goto done;

	if (offset + count < size)
		nread = count;
	else
		nread = size - offset;

	xocl_memcpy_fromio(buf, cmd_queue->cq_base + offset, nread);

done:
	return nread;
}

static struct bin_attribute cq_attr = {
	.attr = {
		.name ="cq_debug",
		.mode = 0444
	},
	.read = ert_cq_debug,
	.write = NULL,
	.size = 0
};

static struct bin_attribute *cmd_queue_bin_attrs[] = {
	&cq_attr,
	NULL,
};

static struct attribute_group cmd_queue_attr_group = {
	.bin_attrs = cmd_queue_bin_attrs,
};

/*
 * cmd_opcode() - Command opcode
 *
 * @epkt: Command object
 * Return: Opcode of command
 */
static inline u32
cmd_opcode(struct ert_packet *epkt)
{
	return epkt->opcode;
}

/**
 * mask_idx32() - Slot mask idx index for a given slot_idx
 *
 * @slot_idx: Global [0..127] index of a CQ slot
 * Return: Index of the slot mask containing the slot_idx
 */
static inline unsigned int
mask_idx32(unsigned int idx)
{
	return idx >> 5;
}
/**
 * idx_in_mask32() - Index of command queue slot within the mask that contains it
 *
 * @slot_idx: Global [0..127] index of a CQ slot
 * Return: Index of slot within the mask that contains it
 */
static inline unsigned int
idx_in_mask32(unsigned int idx, unsigned int mask_idx)
{
	return idx - (mask_idx << 5);
}


/**
 * cmd_queue_reset() - Reset Slot bitmap and set ctrl slot to 1
 */

static void cmd_queue_reset(struct command_queue *cmd_queue)
{
	bitmap_zero(cmd_queue->slot_status, ERT_MAX_SLOTS);
	set_bit(0, cmd_queue->slot_status);
}

/* ERT would return some information when notify host. Ex. PS kernel start and
 * get CU stat commands. In this case, need read CQ slot to get return info.
 *
 * TODO:
 * Assume there are 64 PS kernel and 2 nornal CUs. The ERT_CU_STAT command
 * requires more than (64+2)*2*4 = 528 bytes (without consider other info).
 * In this case, the slot size needs to be 1K and maximum 64 CQ slots.
 *
 * In old kds, to avoid buffer overflow, it silently truncate return value.
 * Luckily there is always use 16 slots in old kds.
 * But truncate is definitly not ideal, this should be fixed in new KDS.
 */
static inline void
command_queue_get_return(struct command_queue *cmd_queue, struct xrt_ert_command *ecmd)
{
	u32 slot_addr;
	int slot_size = cmd_queue->slot_size;

	if (!ecmd->response_size)
		return;

	slot_addr = ecmd->handle * slot_size;
	CMDQUEUE_DBG(cmd_queue, "%s %d slot_addr %x\n", __func__, ecmd->response_size, slot_addr);

	xocl_memcpy_fromio(ecmd->response, cmd_queue->cq_base + slot_addr, ecmd->response_size);
}

/**
 * is_special_cmd() - Check this command if it's a special command
 */
static inline bool
is_special_cmd(struct xrt_ert_command *ecmd)
{
	bool ret = false;
	struct ert_packet *epkt = (struct ert_packet *)ecmd->payload;

	switch (cmd_opcode(epkt)) {
	case ERT_EXIT:
	case ERT_CONFIGURE:
	case ERT_SK_CONFIG:
	case ERT_CU_STAT:
	case ERT_CLK_CALIB:
	case ERT_MB_VALIDATE:
	case ERT_ACCESS_TEST_C:
		ret = true;
		break;
	default:
		break;
	}

	return ret;
}

/*
 * release_slot_idx() - Release specified slot idx
 */
static inline void
command_queue_release_slot_idx(struct command_queue *cmd_queue, unsigned int slot_idx)
{
	clear_bit(slot_idx, cmd_queue->slot_status);
}

/**
 * command_queue_release() - Release a slot index for a command
 *
 * Special case for control commands that execute in slot 0.  This
 * slot cannot be marked free ever.
 */
static inline void
command_queue_release(struct xrt_ert_command *ecmd, void *queue_handle)
{
	struct command_queue *cmd_queue = (struct command_queue *)queue_handle;

	if (ecmd->handle == no_index)
		return;

	/* special command always uses slot 0, don't reset bit0*/
	if (!is_special_cmd(ecmd)) {
		CMDQUEUE_DBG(cmd_queue, "ecmd->handle %d\n", ecmd->handle);
		command_queue_release_slot_idx(cmd_queue, ecmd->handle);
	}
	ecmd->handle = no_index;
}

/*
 * acquire_slot_idx() - First available slot index
 */
static inline unsigned int
command_queue_acquire_slot_idx(struct command_queue *cmd_queue)
{
	unsigned int idx = find_first_zero_bit(cmd_queue->slot_status, ERT_MAX_SLOTS);

	if (idx < cmd_queue->num_slots) {
		set_bit(idx, cmd_queue->slot_status);
		return idx;
	}
	return no_index;
}

/**
 * command_queue_acquire() - Acquire a slot index for a command
 *
 * This function makes a special case for control commands which
 * must always dispatch to slot 0, otherwise normal acquisition
 */
static inline int
command_queue_acquire(struct xrt_ert_command *ecmd, void *queue_handle)
{
	struct command_queue *cmd_queue = (struct command_queue *)queue_handle;
	// slot 0 is reserved for ctrl commands
	if (is_special_cmd(ecmd)) {
		set_bit(CTRL_SLOT, cmd_queue->slot_status);
		return (ecmd->handle = CTRL_SLOT);
	}

	return (ecmd->handle = command_queue_acquire_slot_idx(cmd_queue));
}

static inline void
command_queue_complete(struct xrt_ert_command *ecmd, void *queue_handle)
{
	struct command_queue *cmd_queue = (struct command_queue *)queue_handle;

	command_queue_get_return(cmd_queue, ecmd);
	cmd_queue->submit_queue[ecmd->handle] = NULL;
	command_queue_release(ecmd, cmd_queue);
	ecmd->cb.complete(ecmd, cmd_queue->ert_handle);
}

static inline void
command_queue_check_csr(struct command_queue *cmd_queue)
{
	u32 mask = 0;
	u32 slot_idx = 0, section_idx = 0;
	struct xrt_ert_command *ecmd = NULL;
	xdev_handle_t xdev = xocl_get_xdev(cmd_queue->pdev);

	for (section_idx = 0; section_idx < 4; ++section_idx) {
		mask = xocl_intc_ert_read32(xdev, (section_idx<<2));
		if (!mask)
			continue;
		CMDQUEUE_DBG(cmd_queue, "mask 0x%x\n", mask);
		for (slot_idx = 0; slot_idx < 32; mask >>= 1, ++slot_idx) {
			u32 cmd_idx = slot_idx+(section_idx<<5);

			if (!mask)
				break;
			if (!(mask & 0x1))
				continue;
			ecmd = cmd_queue->submit_queue[cmd_idx];
			if (!ecmd) {
				CMDQUEUE_DBG(cmd_queue, "ERR: submit queue slot is empty\n");
				continue;
			}

			CMDQUEUE_DBG(cmd_queue, "%s -> ecmd %llx\n", __func__, (u64)ecmd);
			ecmd->complete_entry.hdr.cstate = KDS_COMPLETED;
			ecmd->cb.notify(cmd_queue->ert_handle);
		}
	}
}

static void
command_queue_poll(void *queue_handle)
{
	struct command_queue *cmd_queue = (struct command_queue *)queue_handle;
	struct xrt_ert_command *ecmd = NULL, *next = NULL;

	if (!cmd_queue->sq_num)
		return;

	CMDQUEUE_DBG(cmd_queue, "cmd_queue->sq_num %d\n", cmd_queue->sq_num);

	if (cmd_queue->polling_mode)
		command_queue_check_csr(cmd_queue);

	/* check the completed command in submit queue */
	list_for_each_entry_safe(ecmd, next, &cmd_queue->sq, list) {

		if (ecmd->complete_entry.hdr.cstate == KDS_COMPLETED) {
			list_del(&ecmd->list);
			cmd_queue->sq_num--;
			command_queue_complete(ecmd, cmd_queue);
		} else
			continue;
	}
}

static int
command_queue_submit(struct xrt_ert_command *ecmd, void *queue_handle)
{
	struct command_queue *cmd_queue = (struct command_queue *)queue_handle;
	xdev_handle_t xdev = xocl_get_xdev(cmd_queue->pdev);
	u32 slot_addr;
	u32 mask_idx, cq_int_addr, mask;
	struct ert_packet *epkt = NULL;
	u32 cnt = 10000000;

	if (command_queue_acquire(ecmd, queue_handle) == no_index)
		return -EBUSY;

	CMDQUEUE_DBG(cmd_queue, "=> %s\n", __func__);

	epkt = (struct ert_packet *)ecmd->payload;

	slot_addr = ecmd->handle * cmd_queue->slot_size;
	cmd_queue->submit_queue[ecmd->handle] = ecmd;

	CMDQUEUE_DBG(cmd_queue, "%s slot_addr %x\n", __func__, slot_addr);

	list_add_tail(&ecmd->list, &cmd_queue->sq);
	++cmd_queue->sq_num;

	if (kds_echo) {
		ecmd->complete_entry.hdr.cstate = KDS_COMPLETED;
		ecmd->cb.notify(cmd_queue->ert_handle);
	} else {

		if (cmd_opcode(epkt) == ERT_START_CU
			|| cmd_opcode(epkt) == ERT_EXEC_WRITE
			|| cmd_opcode(epkt) == ERT_START_KEY_VAL) {
			// write kds selected cu_idx in first cumask (first word after header)
			iowrite32(ecmd->cu_idx, cmd_queue->cq_base + slot_addr + 4);

			// write remaining packet (past header and cuidx)
			xocl_memcpy_toio(cmd_queue->cq_base + slot_addr + 8,
					 ecmd->payload+2, (ecmd->payload_size-2)*sizeof(u32));
		} else if (cmd_opcode(epkt) == ERT_ACCESS_TEST
			|| cmd_opcode(epkt) == ERT_ACCESS_TEST_C) {
			int offset = 0;
			u32 val, h2h_pass = 1;
			for (offset = sizeof(struct ert_access_valid_cmd); offset < cmd_queue->slot_size; offset+=4) {
				iowrite32(HOST_RW_PATTERN, cmd_queue->cq_base + slot_addr + offset);

				val = ioread32(cmd_queue->cq_base + slot_addr + offset);

				if (val !=HOST_RW_PATTERN) {
					h2h_pass = 0;
					CMDQUEUE_ERR(cmd_queue, "Host <-> Host data integrity failed\n");
					break;
				}

			}
			iowrite32(h2h_pass, cmd_queue->cq_base + slot_addr + offsetof(struct ert_access_valid_cmd, h2h_access));
			CMDQUEUE_DBG(cmd_queue, "Host <-> Host %d slot_addr 0x%x\n", h2h_pass, slot_addr);
			iowrite32(cnt, cmd_queue->cq_base + slot_addr + offsetof(struct ert_access_valid_cmd, wr_count));
		} else {
			CMDQUEUE_DBG(cmd_queue, "%s cmd_opcode(epkt)  %d\n", __func__, cmd_opcode(epkt));
			xocl_memcpy_toio(cmd_queue->cq_base + slot_addr + 4,
				  ecmd->payload+1, (ecmd->payload_size-1)*sizeof(u32));
		}

		iowrite32(epkt->header, cmd_queue->cq_base + slot_addr);


		/* Host writes the patterns to a specific offset and device reads 
		 * Should not have pattern other than 0xFFFFFFFF or 0x0
		 */
		if (cmd_opcode(epkt) == ERT_ACCESS_TEST
		 || cmd_opcode(epkt) == ERT_ACCESS_TEST_C) {
			while (--cnt) {
				u32 pattern = 0xFFFFFFFF;
				if (cnt % 2)
					pattern = 0xFFFFFFFF;
				else
					pattern = 0x0;
				
				iowrite32(pattern, cmd_queue->cq_base + slot_addr + offsetof(struct ert_access_valid_cmd, wr_test));
			}
			iowrite32(cnt, cmd_queue->cq_base + slot_addr + offsetof(struct ert_access_valid_cmd, wr_count));
		}
		/*
		 * Always try to trigger interrupt to embedded scheduler.
		 * The reason is, the ert configure cmd is also sent to MB/PS through cq,
		 * and at the time the new ert configure cmd is sent, host doesn't know
		 * MB/PS is running in cq polling or interrupt mode. eg, if MB/PS is in
		 * cq interrupt mode, new ert configure is cq polling mode, but the new
		 * ert configure cmd has to be received by MB/PS throught interrupt mode
		 *
		 * Setting the bit in cq status register when MB/PS is in cq polling mode
		 * doesn't do harm since the interrupt is disabled and MB/PS will not read
		 * the register
		 */
		mask_idx = mask_idx32(ecmd->handle);
		cq_int_addr = CQ_STATUS_OFFSET + (mask_idx << 2);
		mask = 1 << idx_in_mask32(ecmd->handle, mask_idx);

		CMDQUEUE_DBG(cmd_queue, "++ mb_submit writes slot mask 0x%x to CQ_INT register at addr 0x%x\n",
			    mask, cq_int_addr);
		xocl_intc_ert_write32(xdev, mask, cq_int_addr);
	}

	return 0;
}

static irqreturn_t
cmd_queue_versal_isr(void *arg)
{
	struct command_queue *cmd_queue = (struct command_queue *)arg;
	xdev_handle_t xdev;
	struct xrt_ert_command *ecmd;
	u32 slots[ERT_MAX_SLOTS];
	u32 cnt = 0;
	int slot;
	int i;

	BUG_ON(!cmd_queue);

	CMDQUEUE_DBG(cmd_queue, "-> %s\n", __func__);
	xdev = xocl_get_xdev(cmd_queue->pdev);

	while (!(xocl_mailbox_versal_get(xdev, &slot)))
		slots[cnt++] = slot;

	if (!cnt)
		return IRQ_HANDLED;

	for (i = 0; i < cnt; i++) {
		slot = slots[i];
		CMDQUEUE_DBG(cmd_queue, "[%s] slot: %d\n", __func__, slot);
		ecmd = cmd_queue->submit_queue[slot];
		if (ecmd) {
			ecmd->complete_entry.hdr.cstate = KDS_COMPLETED;
			ecmd->cb.notify(cmd_queue->ert_handle);
		} else {
			CMDQUEUE_ERR(cmd_queue, "not in submitted queue %d\n", slot);
		}
	}

	CMDQUEUE_DBG(cmd_queue, "<- %s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t
cmd_queue_isr(int irq, void *arg)
{
	struct command_queue *cmd_queue = (struct command_queue *)arg;
	xdev_handle_t xdev;
	struct xrt_ert_command *ecmd;

	BUG_ON(!cmd_queue);

	CMDQUEUE_DBG(cmd_queue, "-> %s %d\n", __func__, irq);
	xdev = xocl_get_xdev(cmd_queue->pdev);

	BUG_ON(irq >= ERT_MAX_SLOTS);

	ecmd = cmd_queue->submit_queue[irq];
	if (ecmd) {
		ecmd->complete_entry.hdr.cstate = KDS_COMPLETED;
		ecmd->cb.notify(cmd_queue->ert_handle);
	} else {
		CMDQUEUE_ERR(cmd_queue, "not in submitted queue %d\n", irq);
	}

	CMDQUEUE_DBG(cmd_queue, "<- %s %d\n", __func__, irq);
	return IRQ_HANDLED;
}

static int
command_queue_config(uint32_t slot_size, bool polling_mode, void *ert_handle, void *queue_handle)
{
	struct command_queue *cmd_queue = (struct command_queue *)queue_handle;

	/*  1. cfg->slot_size need to be 32-bit aligned
	 *  2. the slot num max: 128
	 */

	CMDQUEUE_INFO(cmd_queue, "configuring scheduler cq_size(%lld) polling_mode(%d) \n",
		      cmd_queue->cq_range, cmd_queue->polling_mode);
	if (!cmd_queue->cq_range || !slot_size) {
		CMDQUEUE_ERR(cmd_queue, "should not have zero cq_range %lld, slot_size=%d",
			cmd_queue->cq_range, slot_size);
		return -EINVAL;
	} else if (!IS_ALIGNED(slot_size, 4)) {
		CMDQUEUE_ERR(cmd_queue, "slot_size should be 4 bytes aligned, slot_size=%d",
		  slot_size);
		return -EINVAL;
	} else if (slot_size < (cmd_queue->cq_range/ERT_MAX_SLOTS)) {
		CMDQUEUE_ERR(cmd_queue, "slot_size too small=%d", slot_size);
		return -EINVAL;
	}

	cmd_queue->ert_handle = ert_handle;

	cmd_queue->num_slots = cmd_queue->cq_range / slot_size;
	cmd_queue->slot_size = slot_size;
	cmd_queue->polling_mode = polling_mode;

	cmd_queue_reset(cmd_queue);

	return 0;
}

static uint32_t
command_queue_max_slot_num(void *queue_handle)
{
	return ERT_MAX_SLOTS;
}

static void
command_queue_abort(void *client, void *queue_handle)
{
	struct command_queue *cmd_queue = (struct command_queue *)queue_handle;
	struct xrt_ert_command *ecmd = NULL, *next = NULL;

	list_for_each_entry_safe(ecmd, next, &cmd_queue->sq, list) {
		if (ecmd->client != client)
			continue;

		if (ecmd->complete_entry.hdr.cstate != KDS_COMPLETED)
			ecmd->complete_entry.hdr.cstate = KDS_TIMEOUT;

		list_del(&ecmd->list);
		cmd_queue->sq_num--;
		command_queue_complete(ecmd, cmd_queue);
	}
}

static void
command_queue_intc_config(bool enable, void *queue_handle)
{
	struct command_queue *cmd_queue = (struct command_queue *)queue_handle;
	xdev_handle_t xdev = xocl_get_xdev(cmd_queue->pdev);
	uint32_t i = 0;

	CMDQUEUE_DBG(cmd_queue, "-> %s\n", __func__);

	if (XOCL_DSA_IS_VERSAL(xdev)) {
		if (enable)
			xocl_mailbox_versal_request_intr(xdev, cmd_queue_versal_isr, cmd_queue);
		else
			xocl_mailbox_versal_free_intr(xdev);
		return;
	}

	for (i = 0; i < cmd_queue->num_slots; i++) {
		if (enable) {
			xocl_intc_ert_request(xdev, i, cmd_queue_isr, cmd_queue);
			xocl_intc_ert_config(xdev, i, true);
		} else {
			xocl_intc_ert_config(xdev, i, false);
			xocl_intc_ert_request(xdev, i, NULL, NULL);
		}
	}
}

static struct xrt_ert_queue_funcs command_queue_func = {

	.poll    = command_queue_poll,

	.submit  = command_queue_submit,

	.queue_config = command_queue_config,

	.max_slot_num = command_queue_max_slot_num,

	.abort = command_queue_abort,

	.intc_config = command_queue_intc_config,
};

static int __command_queue_remove(struct platform_device *pdev)
{
	struct xrt_ert *command_queue;
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	void *hdl;

	command_queue = platform_get_drvdata(pdev);
	if (!command_queue) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	xocl_subdev_destroy_by_id(xdev, XOCL_SUBDEV_ERT_USER);

	sysfs_remove_group(&pdev->dev.kobj, &cmd_queue_attr_group);

	xocl_drvinst_release(command_queue, &hdl);

	platform_set_drvdata(pdev, NULL);

	xocl_drvinst_free(hdl);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void command_queue_remove(struct platform_device *pdev)
{
	__command_queue_remove(pdev);
}
#else
#define command_queue_remove __command_queue_remove
#endif

static int command_queue_probe(struct platform_device *pdev)
{
	struct command_queue *cmd_queue;
	int err = 0;
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct xocl_ert_cq_privdata *priv = NULL;
	struct xocl_subdev_info subdev_info = XOCL_DEVINFO_ERT_USER;

	cmd_queue = xocl_drvinst_alloc(&pdev->dev, sizeof(struct command_queue));
	if (!cmd_queue)
		return -ENOMEM;

	cmd_queue->dev = &pdev->dev;
	cmd_queue->pdev = pdev;

	platform_set_drvdata(pdev, cmd_queue);

	priv = XOCL_GET_SUBDEV_PRIV(&pdev->dev);
	if (!priv) {
		xocl_err(&pdev->dev, "Cannot get subdev priv");
		err = -EINVAL;
		goto done;
	}
	cmd_queue->cq_range = priv->cq_range;
	cmd_queue->cq_base = priv->cq_base;

	err = xocl_subdev_create(xdev, &subdev_info);
	if (err) {
		xocl_err(&pdev->dev, "can't create ERT_USER_COMMON subdev");
		goto done;
	}

	err = sysfs_create_group(&pdev->dev.kobj, &cmd_queue_attr_group);
	if (err) {
		xocl_err(&pdev->dev, "create ert_cq sysfs attrs failed: %d", err);
		goto done;
	}

	cmd_queue->num_slots = ERT_MAX_SLOTS;
	INIT_LIST_HEAD(&cmd_queue->sq);

	cmd_queue->queue.handle = cmd_queue;
	cmd_queue->queue.func = &command_queue_func;
	cmd_queue->queue.size = cmd_queue->cq_range;

	xocl_ert_user_init_queue(xdev, &cmd_queue->queue);

done:
	if (err) {
		command_queue_remove(pdev);
		return err;
	}
	return 0;
}

struct xocl_drv_private command_queue_priv = {
	.ops = NULL,
	.dev = -1,
};

struct platform_device_id command_queue_id_table[] = {
	{ XOCL_DEVNAME(XOCL_COMMAND_QUEUE), (kernel_ulong_t)&command_queue_priv },
	{ },
};

static struct platform_driver	command_queue_driver = {
	.probe		= command_queue_probe,
	.remove		= command_queue_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_COMMAND_QUEUE),
	},
	.id_table = command_queue_id_table,
};

int __init xocl_init_command_queue(void)
{
	return platform_driver_register(&command_queue_driver);
}

void xocl_fini_command_queue(void)
{
	platform_driver_unregister(&command_queue_driver);
}
