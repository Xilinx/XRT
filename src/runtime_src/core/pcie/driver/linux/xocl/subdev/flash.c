/*
 * Platform driver for flash controllers on Xilinx's Alveo cards.
 *
 * Copyright (C) 2019 Xilinx, Inc. All rights reserved.
 *
 * Authors: Lizhi Hou <lizhih@xilinx.com>
 *          Max Zhen <maxz@xilinx.com>
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
#include "mgmt-ioctl.h"

/* Status write command */
#define QSPI_CMD_STATUSREG_WRITE		0x01
/* Page Program command */
#define QSPI_CMD_PAGE_PROGRAM			0x02
/* Random read command */
#define QSPI_CMD_RANDOM_READ			0x03
/* Status Reg read command */
#define QSPI_CMD_STATUSREG_READ			0x05
/* Config Reg read command */
#define QSPI_CMD_CONFIGREG_READ			0x15
/* Security Reg read command */
#define QSPI_CMD_SECURREG_READ			0x2B
/* Enable flash write */
#define QSPI_CMD_WRITE_ENABLE			0x06
/* 4KB Subsector Erase command */
#define QSPI_CMD_4KB_SUBSECTOR_ERASE		0x20
/* Quad Input Fast Program */
#define QSPI_CMD_QUAD_WRITE			0x32
/* Extended quad input fast program */
#define QSPI_CMD_EXT_QUAD_WRITE			0x38
/* Dual Output Fast Read */
#define QSPI_CMD_DUAL_READ			0x3B
/* Clear flag register */
#define QSPI_CMD_CLEAR_FLAG_REGISTER		0x50
/* 32KB Subsector Erase command */
#define QSPI_CMD_32KB_SUBSECTOR_ERASE		0x52
/* Enhanced volatile configuration register write command */
#define QSPI_CMD_ENH_VOLATILE_CFGREG_WRITE	0x61
/* Enhanced volatile configuration register read command */
#define QSPI_CMD_ENH_VOLATILE_CFGREG_READ	0x65
/* Quad Output Fast Read */
#define QSPI_CMD_QUAD_READ			0x6B
/* Status flag read command */
#define QSPI_CMD_FLAG_STATUSREG_READ		0x70
/* Volatile configuration register write command */
#define QSPI_CMD_VOLATILE_CFGREG_WRITE		0x81
/* Volatile configuration register read command */
#define QSPI_CMD_VOLATILE_CFGREG_READ		0x85
/* Read ID Code */
#define QSPI_CMD_IDCODE_READ			0x9F
/* Non volatile configuration register write command */
#define QSPI_CMD_NON_VOLATILE_CFGREG_WRITE	0xB1
/* Non volatile configuration register read command */
#define QSPI_CMD_NON_VOLATILE_CFGREG_READ	0xB5
/* Dual IO Fast Read */
#define QSPI_CMD_DUAL_IO_READ			0xBB
/* Enhanced volatile configuration register write command */
#define QSPI_CMD_EXTENDED_ADDRESS_REG_WRITE	0xC5
/* Bulk Erase command */
#define QSPI_CMD_BULK_ERASE			0xC7
/* Enhanced volatile configuration register read command */
#define QSPI_CMD_EXTENDED_ADDRESS_REG_READ	0xC8
/* Sector Erase command */
#define QSPI_CMD_SECTOR_ERASE			0xD8
/* Quad IO Fast Read */
#define QSPI_CMD_QUAD_IO_READ			0xEB
/* Global block unlock */
#define QSPI_CMD_GBULK				0x98

#define	FLASH_ERR(flash, fmt, arg...)	\
	xocl_err(&flash->pdev->dev, fmt "\n", ##arg)
#define	FLASH_WARN(flash, fmt, arg...)	\
	xocl_warn(&flash->pdev->dev, fmt "\n", ##arg)
#define	FLASH_INFO(flash, fmt, arg...)	\
	xocl_info(&flash->pdev->dev, fmt "\n", ##arg)
#define	FLASH_DBG(flash, fmt, arg...)	\
	xocl_dbg(&flash->pdev->dev, fmt "\n", ##arg)

/*
 * QSPI control reg bits.
 */
#define QSPI_CR_LOOPBACK		(1 << 0)
#define QSPI_CR_ENABLED			(1 << 1)
#define QSPI_CR_MASTER_MODE		(1 << 2)
#define QSPI_CR_CLK_POLARITY		(1 << 3)
#define QSPI_CR_CLK_PHASE		(1 << 4)
#define QSPI_CR_TXFIFO_RESET		(1 << 5)
#define QSPI_CR_RXFIFO_RESET		(1 << 6)
#define QSPI_CR_MANUAL_SLAVE_SEL	(1 << 7)
#define QSPI_CR_TRANS_INHIBIT		(1 << 8)
#define QSPI_CR_LSB_FIRST		(1 << 9)
#define QSPI_CR_INIT_STATE		(QSPI_CR_TRANS_INHIBIT		| \
					QSPI_CR_MANUAL_SLAVE_SEL	| \
					QSPI_CR_RXFIFO_RESET		| \
					QSPI_CR_TXFIFO_RESET		| \
					QSPI_CR_ENABLED			| \
					QSPI_CR_MASTER_MODE)

/*
 * QSPI status reg bits.
 */
#define QSPI_SR_RX_EMPTY		(1 << 0)
#define QSPI_SR_RX_FULL			(1 << 1)
#define QSPI_SR_TX_EMPTY		(1 << 2)
#define QSPI_SR_TX_FULL			(1 << 3)
#define QSPI_SR_MODE_ERR		(1 << 4)
#define QSPI_SR_SLAVE_MODE		(1 << 5)
#define QSPI_SR_CPOL_CPHA_ERR		(1 << 6)
#define QSPI_SR_SLAVE_MODE_ERR		(1 << 7)
#define QSPI_SR_MSB_ERR			(1 << 8)
#define QSPI_SR_LOOPBACK_ERR		(1 << 9)
#define QSPI_SR_CMD_ERR			(1 << 10)
#define QSPI_SR_ERRS			(QSPI_SR_CMD_ERR	| 	\
					QSPI_SR_LOOPBACK_ERR	| 	\
					QSPI_SR_MSB_ERR		| 	\
					QSPI_SR_SLAVE_MODE_ERR	| 	\
					QSPI_SR_CPOL_CPHA_ERR	| 	\
					QSPI_SR_MODE_ERR)

#define	MAX_NUM_OF_SLAVES	2
#define	SLAVE_NONE		(-1)
#define	SLAVE_REG_MASK		((1 << MAX_NUM_OF_SLAVES) -1)
#define SLAVE_SELECT_NONE	SLAVE_REG_MASK

/*
 * We support erasing flash memory at three page unit. Page read-modify-write
 * is done at smallest page unit.
 */
#define	FLASH_LARGE_PAGE_SIZE	(32UL * 1024)
#define	FLASH_HUGE_PAGE_SIZE	(64UL * 1024)
#define	FLASH_PAGE_SIZE		(4UL * 1024)
#define	FLASH_PAGE_MASK		(FLASH_PAGE_SIZE - 1)
#define	FLASH_PAGE_ALIGN(off)	((off) & ~FLASH_PAGE_MASK)
#define	FLASH_PAGE_OFFSET(off)	((off) & FLASH_PAGE_MASK)
static inline size_t FLASH_PAGE_ROUNDUP(loff_t offset)
{
	if (FLASH_PAGE_OFFSET(offset))
		return round_up(offset, FLASH_PAGE_SIZE);
	return offset + FLASH_PAGE_SIZE;
}
struct xocl_flash;
static int macronix_configure(struct xocl_flash *flash);

/*
 * Wait for condition to be true for at most 1 second.
 * Return true, if time'd out, false otherwise.
 */
#define FLASH_BUSY_WAIT(condition)					\
({									\
	const int interval = 5; /* in microsec */			\
	int retry = 1000 * 1000 / interval; /* wait for 1 second */	\
	while (retry && !(condition)) {					\
		udelay(interval);					\
		retry--;						\
	}								\
	(retry == 0);							\
})

static size_t micron_code2sectors(u8 code)
{
	size_t max_sectors = 0;

	switch (code) {
	case 0x17:
		max_sectors = 1;
		break;
	case 0x18:
		max_sectors = 1;
		break;
	case 0x19:
		max_sectors = 2;
		break;
	case 0x20:
		max_sectors = 4;
		break;
	case 0x21:
		max_sectors = 8;
		break;
	case 0x22:
		max_sectors = 16;
		break;
	default:
		break;
	}
	return max_sectors;
}

/*
 * QSPI IP on some of the old shell/golden does not support
 * QSPI_CMD_EXT_QUAD_WRITE, but driver does not know if IP is new or old.
 * Since we only use micron chip on old boards, we'll just use different
 * write cmd for micron and macronix chip.
 */

static u8 micron_write_cmd(void)
{
	return QSPI_CMD_QUAD_WRITE;
}

static u8 macronix_write_cmd(void)
{
	return QSPI_CMD_EXT_QUAD_WRITE;
}

static size_t macronix_code2sectors(u8 code)
{
	if (code < 0x38 || code > 0x3c)
		return 0;
	return (1 << (code - 0x38));
}

static int micron_configure(struct xocl_flash *flash)
{
	return 0;
}

/*
 * Flash memory vendor specific operations.
 */
static struct qspi_flash_vendor {
	u8 vendor_id;
	const char *vendor_name;
	size_t (*code2sectors)(u8 code);
	u8 (*write_cmd)(void);
	int (*configure)(struct xocl_flash *flash);
} vendors[] = {
	{
		0x20, "micron", micron_code2sectors,
		micron_write_cmd, micron_configure
	},
	{
		0xc2, "macronix", macronix_code2sectors,
		macronix_write_cmd, macronix_configure
	},
};

struct qspi_flash_addr {
	u8 slave;
	u8 sector;
	u8 addr_lo;
	u8 addr_mid;
	u8 addr_hi;
};

/*
 * QSPI flash controller IP register layout
 */
struct qspi_reg {
	u32	qspi_padding1[16];
	u32	qspi_reset;
	u32	qspi_padding2[7];
	u32	qspi_ctrl;
	u32	qspi_status;
	u32	qspi_tx;
	u32	qspi_rx;
	u32	qspi_slave;
	u32	qspi_tx_fifo;
	u32	qspi_rx_fifo;
} __attribute__((packed));

struct xocl_flash {
	struct platform_device	*pdev;

	struct resource *res;
	struct xocl_flash_privdata *priv_data;
	struct mutex io_lock;
	bool sysfs_created;
	bool busy;
	bool io_debug;
	size_t flash_size;
	size_t num_slaves;
	u8 *io_buf;

	/* For now, only support QSPI */
	struct qspi_reg *qspi_regs;
	size_t qspi_fifo_depth;
	u8 qspi_curr_sector;
	struct qspi_flash_vendor *vendor;
	int qspi_curr_slave;
};

static inline const char *reg2name(struct xocl_flash *flash, u32 *reg)
{
	const char *reg_names[] = {
		"qspi_ctrl",
		"qspi_status",
		"qspi_tx",
		"qspi_rx",
		"qspi_slave",
		"qspi_tx_fifo",
		"qspi_rx_fifo",
	};
	size_t off = (uintptr_t)reg - (uintptr_t)flash->qspi_regs;

	if (off == offsetof(struct qspi_reg, qspi_reset))
		return "qspi_reset";
	if (off < offsetof(struct qspi_reg, qspi_ctrl))
		return "padding";
	off -= offsetof(struct qspi_reg, qspi_ctrl);
	return reg_names[off / sizeof(u32)];
}

static inline u32 flash_reg_rd(struct xocl_flash *flash, u32 *reg)
{
	u32 val = ioread32(reg);
	if (flash->io_debug)
		FLASH_INFO(flash, "REG_RD(%s)=0x%x", reg2name(flash, reg), val);
	return val;
}

static inline void flash_reg_wr(struct xocl_flash *flash, u32 *reg, u32 val)
{
	if (flash->io_debug)
		FLASH_INFO(flash, "REG_WR(%s,0x%x)", reg2name(flash, reg), val);
	iowrite32(val, reg);
}

static inline u32 flash_get_status(struct xocl_flash *flash)
{
	return flash_reg_rd(flash, &flash->qspi_regs->qspi_status);
}

static inline u32 flash_get_ctrl(struct xocl_flash *flash)
{
	return flash_reg_rd(flash, &flash->qspi_regs->qspi_ctrl);
}

static inline void flash_set_ctrl(struct xocl_flash *flash, u32 ctrl)
{
	flash_reg_wr(flash, &flash->qspi_regs->qspi_ctrl, ctrl);
}

static size_t flash_detect_slaves(struct xocl_flash *flash) 
{
	u32 slave_reg;
	size_t num_slaves;

	/* Clear the slave register to make sure we read correct slave value. */
	flash_reg_wr(flash, &flash->qspi_regs->qspi_slave, 0xFFFFFFFF);
	slave_reg = flash_reg_rd(flash, &flash->qspi_regs->qspi_slave);

	if(slave_reg == 0x1)
		num_slaves = 1;
	else if(slave_reg == 0x3)
		num_slaves = 2;
	else
		num_slaves = 0;

	return num_slaves;
}

static inline void flash_activate_slave(struct xocl_flash *flash, int index)
{
	u32 slave_reg;

	if (index == SLAVE_NONE)
		slave_reg = SLAVE_SELECT_NONE;
	else
		slave_reg = ~(1 << index);
	flash_reg_wr(flash, &flash->qspi_regs->qspi_slave, slave_reg);
}

/*
 * Pull one byte from flash RX fifo.
 * So far, only 8-bit data width is supported.
 */
static inline u8 flash_read8(struct xocl_flash *flash)
{
	return (u8)flash_reg_rd(flash, &flash->qspi_regs->qspi_rx);
}

/*
 * Push one byte to flash TX fifo.
 * So far, only 8-bit data width is supported.
 */
static inline void flash_send8(struct xocl_flash *flash, u8 val)
{
	flash_reg_wr(flash, &flash->qspi_regs->qspi_tx, val);
}

static inline bool flash_has_err(struct xocl_flash *flash)
{
	u32 status = flash_get_status(flash);
	if (!(status & QSPI_SR_ERRS))
		return false;

	FLASH_ERR(flash, "QSPI error status: 0x%x", status);
	return true;
}

/*
 * Caller should make sure the flash controller has exactly
 * len bytes in the fifo. It's an error if we pull out less.
 */
static int flash_rx(struct xocl_flash *flash, u8 *buf, size_t len)
{
	size_t cnt;
	u8 c;

	for (cnt = 0; cnt < len; cnt++) {
		if ((flash_get_status(flash) & QSPI_SR_RX_EMPTY) != 0)
			return -EINVAL;

        	c = flash_read8(flash);

		if (buf)
			buf[cnt] = c;
	}

	if ((flash_get_status(flash) & QSPI_SR_RX_EMPTY) == 0) {
		FLASH_ERR(flash, "failed to drain RX fifo");
		return -EINVAL;
	}

	if (flash_has_err(flash))
		return -EINVAL;

	return 0;
}

/*
 * Caller should make sure the fifo is large enough to host len bytes.
 */
static int flash_tx(struct xocl_flash *flash, u8 *buf, size_t len)
{
	u32 ctrl = flash_get_ctrl(flash);
	int i;

	BUG_ON(len > flash->qspi_fifo_depth);

	/* Stop transfering to the flash. */
	flash_set_ctrl(flash, ctrl | QSPI_CR_TRANS_INHIBIT);

	/* Fill out the FIFO. */
	for (i = 0; i < len; i++)
		flash_send8(flash, buf[i]);

	/* Start transfering to the flash. */
	flash_set_ctrl(flash, ctrl & ~QSPI_CR_TRANS_INHIBIT);

	/* Waiting for FIFO to become empty again. */
	if (FLASH_BUSY_WAIT(flash_get_status(flash) &
		(QSPI_SR_TX_EMPTY | QSPI_SR_ERRS))) {
		if (flash_has_err(flash)) {
			FLASH_ERR(flash, "QSPI write failed");
		} else {
			FLASH_ERR(flash, "QSPI write timeout, status: 0x%x",
				flash_get_status(flash));
		}
		return -ETIMEDOUT;
	}

	/* Always stop transfering to the flash after we finish. */
	flash_set_ctrl(flash, ctrl | QSPI_CR_TRANS_INHIBIT);

	if (flash_has_err(flash))
		return -EINVAL;

	return 0;
}

/*
 * Reset both RX and TX FIFO.
 */
static int flash_reset_fifo(struct xocl_flash *flash)
{
	const u32 status_fifo_mask = QSPI_SR_TX_FULL | QSPI_SR_RX_FULL |
		QSPI_SR_TX_EMPTY | QSPI_SR_RX_EMPTY;
	u32 fifo_status = flash_get_status(flash) & status_fifo_mask;

	if (fifo_status == (QSPI_SR_TX_EMPTY | QSPI_SR_RX_EMPTY))
		return 0;

	flash_set_ctrl(flash, flash_get_ctrl(flash) | QSPI_CR_TXFIFO_RESET |
		QSPI_CR_RXFIFO_RESET);

	if (FLASH_BUSY_WAIT((flash_get_status(flash) & status_fifo_mask) ==
		(QSPI_SR_TX_EMPTY | QSPI_SR_RX_EMPTY))) {
		FLASH_ERR(flash, "failed to reset FIFO, status: 0x%x",
			flash_get_status(flash));
		return -ETIMEDOUT;
	}
	return 0;
}

static int flash_transaction(struct xocl_flash *flash,
	u8 *buf, size_t len, bool need_output)
{
	int ret = 0;

	/* Reset both the TX and RX fifo before starting transaction. */
	ret = flash_reset_fifo(flash);
	if (ret)
		return ret;

	/* The slave index should be within range. */
	if (flash->qspi_curr_slave >= MAX_NUM_OF_SLAVES)
		return -EINVAL;
	flash_activate_slave(flash, flash->qspi_curr_slave);

	ret = flash_tx(flash, buf, len);
	if (ret)
		return ret;

	if (need_output) {
		ret = flash_rx(flash, buf, len);
	} else {
		/* Needs to drain the FIFO even when the data is not wanted. */
		(void) flash_rx(flash, NULL, len);
	}

	/* Always need to reset slave select register after each transaction */
	flash_activate_slave(flash, SLAVE_NONE);

	return ret;
}

static size_t flash_get_fifo_depth(struct xocl_flash *flash)
{
	size_t depth = 0;
	u32 ctrl;

	/* Reset TX fifo. */
	if (flash_reset_fifo(flash))
		return depth;

	/* Stop transfering to flash. */
	ctrl = flash_get_ctrl(flash);
	flash_set_ctrl(flash, ctrl | QSPI_CR_TRANS_INHIBIT);

	/*
	 * Find out fifo depth by keep pushing data to QSPI until
	 * the fifo is full. We can choose to send any data. But
	 * sending 0 seems to cause error, so pick a non-zero one.
	 */
	while (!(flash_get_status(flash) & (QSPI_SR_TX_FULL | QSPI_SR_ERRS))) {
		flash_send8(flash, 1);
		depth++;
	}

	/* Make sure flash is still in good shape. */
	if (flash_has_err(flash))
		return 0;

	/* Reset RX/TX fifo and restore ctrl since we just touched them. */
	flash_set_ctrl(flash, ctrl);
	(void) flash_reset_fifo(flash);

	return depth;
}

/*
 * Exec flash IO command on specified slave.
 */
static inline int flash_exec_io_cmd(struct xocl_flash *flash,
	size_t len, bool output_needed)
{
	char *buf = flash->io_buf;

	return flash_transaction(flash, buf, len, output_needed);
}

/* Test if flash memory is ready. */
static bool flash_is_ready(struct xocl_flash *flash)
{
	/*
	 * Reading flash device status input needs a dummy byte
	 * after cmd byte. The output is in the 2nd byte.
	 */
	u8 cmd[2] = { QSPI_CMD_STATUSREG_READ, };
	int ret = flash_transaction(flash, cmd, sizeof(cmd), true);

	if (ret || (cmd[1] & 0x1)) // flash device is busy
		return false;

	return true;
}

static int flash_enable_write(struct xocl_flash *flash)
{
	u8 cmd = QSPI_CMD_WRITE_ENABLE;
	int ret = flash_transaction(flash, &cmd, 1, false);

	if (ret)
		FLASH_ERR(flash, "Failed to enable flash write: %d", ret);
	return ret;
}

static bool flash_wait_until_ready(struct xocl_flash *flash)
{
	if (FLASH_BUSY_WAIT(flash_is_ready(flash))) {
		FLASH_ERR(flash, "QSPI flash device is not ready");
		return false;
	}
	return true;
}

static int macronix_configure(struct xocl_flash *flash)
{
	int ret;
	u8 cmd[3];

	FLASH_INFO(flash, "Configuring registers for Macronix");

	//Configure status register (Quad enable, default drive strength)
	if (!flash_wait_until_ready(flash))
		return -EINVAL;

	flash_enable_write(flash);
	cmd[0] = QSPI_CMD_STATUSREG_WRITE;
	cmd[1] = 0x40;
	cmd[2] = 0x07;
	ret = flash_transaction(flash, cmd, 3, false);
	if (ret)
		return ret;

	//Set gang block unlock
	if (!flash_wait_until_ready(flash))
		return -EINVAL;

	flash_enable_write(flash);
	cmd[0] = QSPI_CMD_GBULK;
	return flash_transaction(flash, cmd, 1, false);
}

static int flash_get_info(struct xocl_flash *flash)
{
	int i;
	struct qspi_flash_vendor *vendor = NULL;
	/*
	 * Reading flash device vendor ID. Vendor ID is in cmd[1], max vector
	 * number is in cmd[3] from output.
	 */
	u8 cmd[5] = { QSPI_CMD_IDCODE_READ, };
	int ret = flash_transaction(flash, cmd, sizeof(cmd), true);

	if (ret) {
		FLASH_ERR(flash, "Can't get flash memory ID, err: %d", ret);
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(vendors); i++) {
		if (cmd[1] == vendors[i].vendor_id) {
			vendor = &vendors[i];
			break;
		}
	}

	/* Find out flash vendor and size. */
	if (vendor == NULL) {
		FLASH_ERR(flash, "Unknown flash vendor: %d", cmd[1]);
		return -EINVAL;
	} else {
		FLASH_INFO(flash, "Flash vendor: %s", vendor->vendor_name);
		flash->vendor = vendor;
	}

	flash->flash_size = vendor->code2sectors(cmd[3]) * (16 * 1024 * 1024);
	if (flash->flash_size == 0) {
		FLASH_ERR(flash, "Unknown flash memory size code: %d", cmd[3]);
		return -EINVAL;
	} else {
		FLASH_INFO(flash, "Flash size: %ldMB",
			flash->flash_size / 1024 / 1024);
	}

	return 0;
}

static int flash_set_sector(struct xocl_flash *flash, u8 sector)
{
	int ret = 0;
	u8 cmd[] = { QSPI_CMD_EXTENDED_ADDRESS_REG_WRITE, sector };

	if (sector == flash->qspi_curr_sector)
		return 0;

	FLASH_DBG(flash, "setting sector to %d", sector);

	ret = flash_enable_write(flash);
	if (ret)
		return ret;

	ret = flash_transaction(flash, cmd, sizeof(cmd), false);
	if (ret) {
		FLASH_ERR(flash, "Failed to set sector %d: %d", sector, ret);
		return ret;
	}

	flash->qspi_curr_sector = sector;
	return ret;
}

/* For 24 bit addressing. */
static inline void flash_offset2faddr(loff_t addr,
	struct qspi_flash_addr *faddr)
{
	faddr->slave = (u8)(addr >> 56);
	faddr->sector = (u8)(addr >> 24);
	faddr->addr_lo = (u8)(addr);
	faddr->addr_mid = (u8)(addr >> 8);
	faddr->addr_hi = (u8)(addr >> 16);
}

static inline loff_t flash_faddr2offset(struct qspi_flash_addr *faddr)
{
	loff_t off = 0;

	off |= faddr->sector;
	off <<= 8;
	off |= faddr->addr_hi;
	off <<= 8;
	off |= faddr->addr_mid;
	off <<= 8;
	off |= faddr->addr_lo;
	off |= ((u64)faddr->slave) << 56;
	return off;
}

/* IO cmd starts with op code followed by address. */
static inline int
flash_setup_io_cmd_header(struct xocl_flash *flash,
	u8 op, struct qspi_flash_addr *faddr, size_t *header_len)
{
	int ret = 0;

	/* Set sector (the high byte of a 32-bit address), if needed. */
	ret = flash_set_sector(flash, faddr->sector);
	if (ret == 0) {
		/* The rest of address bytes are in cmd. */
		flash->io_buf[0] = op;
		flash->io_buf[1] = faddr->addr_hi;
		flash->io_buf[2] = faddr->addr_mid;
		flash->io_buf[3] = faddr->addr_lo;
		*header_len = 4;
	}

	return ret;
}

static int qspi_probe(struct xocl_flash *flash)
{
	int ret;
	size_t i;

	/* Probing on first flash only. */
	flash->qspi_curr_slave = 0;

	flash_set_ctrl(flash, QSPI_CR_INIT_STATE);

	/* Find out fifo depth before any read/write operations. */
	flash->qspi_fifo_depth = flash_get_fifo_depth(flash);
	if (flash->qspi_fifo_depth == 0)
		return -EINVAL;
	FLASH_INFO(flash, "QSPI FIFO depth is: %ld", flash->qspi_fifo_depth);

	if (!flash_wait_until_ready(flash))
		return -EINVAL;

	flash->num_slaves = flash_detect_slaves(flash);
	if(flash->num_slaves == 0)
		return -EINVAL;
	FLASH_INFO(flash, "Number of slave chips is: %ld", flash->num_slaves);

	/*
	 * Get flash info only from first chip assuming that all chips
	 * are identical. If not, we have a much bigger problem to solve.
	 */
	flash->qspi_curr_slave = 0;
	ret = flash_get_info(flash);
	if (ret)
		return ret;

	/* Configure all chips, if necessary. */
	for (i = 0; i < flash->num_slaves; ++i) {
		flash->qspi_curr_slave = i;
		ret = flash->vendor->configure(flash);
		if (ret)
			return ret;
	}

	flash->qspi_curr_slave = 0;
	flash->qspi_curr_sector = 0xff;

	return 0;
}

/*
 * Do one FIFO read from flash.
 * @cnt contains bytes actually read on successful return.
 */
static int flash_fifo_rd(struct xocl_flash *flash,
	loff_t off, u8 *buf, size_t *cnt)
{
	/* For read cmd, we need to exclude a few more dummy bytes in FIFO. */
	const size_t read_dummy_len = 4;

	int ret;
	struct qspi_flash_addr faddr;
	size_t header_len, total_len, payload_len;

	/* Should not cross page bundary. */
	BUG_ON(off + *cnt > FLASH_PAGE_ROUNDUP(off));
	flash_offset2faddr(off, &faddr);

	ret = flash_setup_io_cmd_header(flash,
		QSPI_CMD_QUAD_READ, &faddr, &header_len);
	if (ret)
		return ret;

	/* Figure out length of IO for this read. */

	/*
	 * One read should not be more than one fifo depth, so that we don't
	 * overrun flash->io_buf.
	 * The first header_len + read_dummy_len bytes in output buffer are
	 * always garbage, need to make room for them. What a wonderful memory
	 * controller!!
	 */
	payload_len = min(*cnt,
		flash->qspi_fifo_depth - header_len - read_dummy_len);
	total_len = payload_len + header_len + read_dummy_len;

	FLASH_DBG(flash, "reading %ld bytes @0x%llx", payload_len, off);

	/* Now do the read. */

	/*
	 * You tell the memory controller how many bytes you want to read
	 * by writing that many bytes to it. How hard would it be to just
	 * add one more integer to specify the length in the input cmd?!
	 */
	ret = flash_exec_io_cmd(flash, total_len, true);
	if (ret)
		return ret;

	/* Copy out the output. Skip the garbage part. */
	memcpy(buf, &flash->io_buf[header_len + read_dummy_len], payload_len);
	*cnt = payload_len;
	return 0;
}

/*
 * Do one FIFO write to flash. Assuming erase is already done.
 * @cnt contains bytes actually written on successful return.
 */
static int flash_fifo_wr(struct xocl_flash *flash,
	loff_t off, u8 *buf, size_t *cnt)
{
	/*
	 * For write cmd, we can't write more than write_max_len bytes in one
	 * IO request even though we have larger fifo. Otherwise, writes will
	 * randomly fail.
	 */
	const size_t write_max_len = 128UL;

	int ret;
	struct qspi_flash_addr faddr;
	size_t header_len, total_len, payload_len;

	flash_offset2faddr(off, &faddr);

	ret = flash_setup_io_cmd_header(flash,
		flash->vendor->write_cmd(), &faddr, &header_len);
	if (ret)
		return ret;

	/* Figure out length of IO for this write. */

	/*
	 * One IO should not be more than one fifo depth, so that we don't
	 * overrun flash->io_buf. And we don't go beyond the write_max_len;
	 */
	payload_len = min(*cnt, flash->qspi_fifo_depth - header_len);
	payload_len = min(payload_len, write_max_len);
	total_len = payload_len + header_len;

	FLASH_DBG(flash, "writing %ld bytes @0x%llx", payload_len, off);

	/* Copy in payload after header. */
	memcpy(&flash->io_buf[header_len], buf, payload_len);

	/* Now do the write. */

	ret = flash_enable_write(flash);
	if (ret)
		return ret;
	ret = flash_exec_io_cmd(flash, total_len, false);
	if (ret)
		return ret;
	if (!flash_wait_until_ready(flash))
		return -EINVAL;

	*cnt = payload_len;
	return 0;
}

/*
 * Load/store the whole buf of data from/to flash memory.
 */
static int flash_buf_rdwr(struct xocl_flash *flash,
	u8 *buf, loff_t off, size_t len, bool write)
{
	int ret = 0;
	size_t n, curlen;

	for (n = 0; ret == 0 && n < len; n += curlen) {
		curlen = len - n;
		if (write)
			ret = flash_fifo_wr(flash, off + n, &buf[n], &curlen);
		else
			ret = flash_fifo_rd(flash, off + n, &buf[n], &curlen);
	}

	/*
	 * Yield CPU after every buf IO so that Linux does not complain
	 * about CPU soft lockup.
	 */
	schedule();
	return ret;
}

static u8 flash_erase_cmd(size_t pagesz)
{
	u8 cmd = 0;
	const size_t onek = 1024;

	BUG_ON(!IS_ALIGNED(pagesz, onek));
	switch (pagesz / onek) {
	case 4:
		cmd = QSPI_CMD_4KB_SUBSECTOR_ERASE;
		break;
	case 32:
		cmd = QSPI_CMD_32KB_SUBSECTOR_ERASE;
		break;
	case 64:
		cmd = QSPI_CMD_SECTOR_ERASE;
		break;
	default:
		BUG_ON(1);
		break;
	}
	return cmd;
}

/*
 * Erase one flash page.
 */
static int flash_page_erase(struct xocl_flash *flash, loff_t off, size_t pagesz)
{
	int ret = 0;
	struct qspi_flash_addr faddr;
	size_t cmdlen;
	u8 cmd = flash_erase_cmd(pagesz);

	FLASH_DBG(flash, "Erasing 0x%lx bytes @0x%llx with cmd=0x%x",
		pagesz, off, (u32)cmd);

	BUG_ON(!IS_ALIGNED(off, pagesz));
	flash_offset2faddr(off, &faddr);

	if (!flash_wait_until_ready(flash))
		return -EINVAL;

	ret = flash_setup_io_cmd_header(flash, cmd, &faddr, &cmdlen);
	if (ret)
		return ret;

	ret = flash_enable_write(flash);
	if (ret)
		return ret;

	ret = flash_exec_io_cmd(flash, cmdlen, false);
	if (ret) {
		FLASH_ERR(flash, "Failed to erase 0x%lx bytes @0x%llx",
			pagesz, off);
		return ret;
	}

	if (!flash_wait_until_ready(flash))
		return -EINVAL;

	return 0;
}

static bool is_valid_offset(struct xocl_flash *flash, loff_t off)
{
	struct qspi_flash_addr faddr;

	flash_offset2faddr(off, &faddr);
	/* Assuming all flash are of the same size, we use
	 * offset into flash 0 to perform boundary check. */
	faddr.slave = 0;
	return flash_faddr2offset(&faddr) < flash->flash_size;
}

static int
flash_do_read(struct xocl_flash *flash, char *kbuf, size_t n, loff_t off)
{
	u8 *page = NULL;
	size_t cnt = 0;
	struct qspi_flash_addr faddr;
	int ret = 0;

	page = vmalloc(FLASH_PAGE_SIZE);
	if (page == NULL)
		return -ENOMEM;

	mutex_lock(&flash->io_lock);

	flash_offset2faddr(off, &faddr);
	if (faddr.slave >= flash->num_slaves) {
		FLASH_ERR(flash, "Can't read: out of slave boundary");
		ret = -ENOSPC;
	}
	flash->qspi_curr_slave = faddr.slave;

	if (ret == 0 && !flash_wait_until_ready(flash))
		ret = -EINVAL;

	while (ret == 0 && cnt < n) {
		loff_t thisoff = off + cnt;
		size_t thislen = min(n - cnt,
			FLASH_PAGE_ROUNDUP(thisoff) - (size_t)thisoff);
		char *thisbuf = &page[FLASH_PAGE_OFFSET(thisoff)];

		ret = flash_buf_rdwr(flash, thisbuf, thisoff, thislen, false);
		if (ret)
			break;

		memcpy(&kbuf[cnt], thisbuf, thislen);
		cnt += thislen;
	}

	mutex_unlock(&flash->io_lock);
	vfree(page);
	return ret;
}

/*
 * Read flash memory page by page into user buf.
 */
static ssize_t
flash_read(struct file *file, char __user *ubuf, size_t n, loff_t *off)
{
	struct xocl_flash *flash = file->private_data;
	char *kbuf = NULL;
	int ret = 0;

	FLASH_INFO(flash, "reading %ld bytes @0x%llx", n, *off);

	if (n == 0 || !is_valid_offset(flash, *off)) {
		FLASH_ERR(flash, "Can't read: out of boundary");
		return 0;
	}
	n = min(n, flash->flash_size - (size_t)*off);
	kbuf = vmalloc(n);
	if (kbuf == NULL)
		return -ENOMEM;

	ret = flash_do_read(flash, kbuf, n, *off);
	if (ret == 0) {
		if (copy_to_user(ubuf, kbuf, n) != 0)
			ret = -EFAULT;
	}
	vfree(kbuf);

	if (ret)
		return ret;

	*off += n;
	return n;
}

/* Read request from other parts of driver. */
static int flash_kread(struct platform_device *pdev,
		char *buf, size_t n, loff_t off)
{
	struct xocl_flash *flash = platform_get_drvdata(pdev);
	FLASH_INFO(flash, "kernel reading %ld bytes @0x%llx", n, off);
	return flash_do_read(flash, buf, n, off);
}

/*
 * Write a page. Perform read-modify-write as needed.
 * @cnt contains actual bytes copied from user on successful return.
 */
static int flash_page_rmw(struct xocl_flash *flash,
	const char __user *ubuf, u8 *kbuf, loff_t off, size_t *cnt)
{
	loff_t thisoff = FLASH_PAGE_ALIGN(off);
	size_t front = FLASH_PAGE_OFFSET(off);
	size_t mid = min(*cnt, FLASH_PAGE_SIZE - front);
	size_t last = FLASH_PAGE_SIZE - front - mid;
	u8 *thiskbuf = kbuf;
	int ret;

	if (front) {
		ret = flash_buf_rdwr(flash, thiskbuf, thisoff, front, false);
		if (ret)
			return ret;
	}
	thisoff += front;
	thiskbuf += front;
	if (copy_from_user(thiskbuf, ubuf, mid) != 0)
		return -EFAULT;
	*cnt = mid;
	thisoff += mid;
	thiskbuf += mid;
	if (last) {
		ret = flash_buf_rdwr(flash, thiskbuf, thisoff, last, false);
		if (ret)
			return ret;
	}

	ret = flash_page_erase(flash, FLASH_PAGE_ALIGN(off), FLASH_PAGE_SIZE);
	if (ret == 0) {
		ret = flash_buf_rdwr(flash, kbuf, FLASH_PAGE_ALIGN(off),
			FLASH_PAGE_SIZE, true);
	}
	return ret;
}

static inline size_t flash_get_page_io_size(loff_t off, size_t sz)
{
	if (IS_ALIGNED(off, FLASH_HUGE_PAGE_SIZE) &&
		sz >= FLASH_HUGE_PAGE_SIZE)
		return FLASH_HUGE_PAGE_SIZE;
	if (IS_ALIGNED(off, FLASH_LARGE_PAGE_SIZE) &&
		sz >= FLASH_LARGE_PAGE_SIZE)
		return FLASH_LARGE_PAGE_SIZE;
	if (IS_ALIGNED(off, FLASH_PAGE_SIZE) &&
		sz >= FLASH_PAGE_SIZE)
		return FLASH_PAGE_SIZE;

	return 0; // can't do full page IO
}

/*
 * Try to erase and write full (large/huge) page.
 * @cnt contains actual bytes copied from user on successful return.
 * Needs to fallback to RMW, if not possible.
 */
static int flash_page_wr(struct xocl_flash *flash,
	const char __user *ubuf, u8 *kbuf, loff_t off, size_t *cnt)
{
	int ret;
	size_t thislen = flash_get_page_io_size(off, *cnt);

	if (thislen == 0)
		return -EOPNOTSUPP;

	*cnt = thislen;

	if (copy_from_user(kbuf, ubuf, thislen) != 0)
		return -EFAULT;

	ret = flash_page_erase(flash, off, thislen);
	if (ret == 0)
		ret = flash_buf_rdwr(flash, kbuf, off, thislen, true);
	return ret;
}

/*
 * Write to flash memory page by page from user buf.
 */
static ssize_t
flash_write(struct file *file, const char __user *buf, size_t n, loff_t *off)
{
	struct xocl_flash *flash = file->private_data;
	u8 *page = NULL;
	size_t cnt = 0;
	int ret = 0;
	struct qspi_flash_addr faddr;

	FLASH_INFO(flash, "writing %ld bytes @0x%llx", n, *off);

	if (n == 0 || !is_valid_offset(flash, *off)) {
		FLASH_ERR(flash, "Can't write: out of boundary");
		return -ENOSPC;
	}
	n = min(n, flash->flash_size - (size_t)*off);

	page = vmalloc(FLASH_HUGE_PAGE_SIZE);
	if (page == NULL)
		return -ENOMEM;

	mutex_lock(&flash->io_lock);

	flash_offset2faddr(*off, &faddr);
	if (faddr.slave >= flash->num_slaves) {
		FLASH_ERR(flash, "Can't write: out of slave boundary");
		ret = -ENOSPC;
	}
	flash->qspi_curr_slave = faddr.slave;

	if (ret == 0 && !flash_wait_until_ready(flash))
		ret = -EINVAL;

	while (ret == 0 && cnt < n) {
		loff_t thisoff = *off + cnt;
		const char *thisbuf = buf + cnt;
		size_t thislen = n - cnt;

		/* Try write full page. */
		ret = flash_page_wr(flash, thisbuf, page, thisoff, &thislen);
		if (ret) {
			/* Fallback to RMW. */
			if (ret == -EOPNOTSUPP) {
				ret = flash_page_rmw(flash, thisbuf, page,
					thisoff, &thislen);
			}
			if (ret)
				break;
		}
		cnt += thislen;
	}

	mutex_unlock(&flash->io_lock);

	vfree(page);
	if (ret)
		return ret;

	*off += n;
	return n;
}

static loff_t
flash_llseek(struct file *filp, loff_t off, int whence)
{
	loff_t npos;

	switch(whence) {
	case 0: /* SEEK_SET */
		npos = off;
		break;
	case 1: /* SEEK_CUR */
		npos = filp->f_pos + off;
		break;
	case 2: /* SEEK_END: no need to support */
		return -EINVAL;
	default: /* should not happen */
		return -EINVAL;
	}
	if (npos < 0)
		return -EINVAL;

	filp->f_pos = npos;
	return npos;
}

/*
 * Only allow one client at a time.
 */
static int flash_open(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct xocl_flash *flash = xocl_drvinst_open(inode->i_cdev);

	if (!flash)
		return -ENXIO;

	mutex_lock(&flash->io_lock);
	if (flash->busy) {
		ret = -EBUSY;
	} else {
		file->private_data = flash;
		flash->busy = true;
	}
	mutex_unlock(&flash->io_lock);

	if (ret)
		xocl_drvinst_close(flash);
	return ret;
}

static int flash_close(struct inode *inode, struct file *file)
{
	struct xocl_flash *flash = file->private_data;

	if (!flash)
		return -EINVAL;

	mutex_lock(&flash->io_lock);
	flash->busy = false;
	file->private_data = NULL;
	mutex_unlock(&flash->io_lock);

	xocl_drvinst_close(flash);
	return 0;
}

static ssize_t bar_off_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_flash *flash = dev_get_drvdata(dev);
	xdev_handle_t xdev = xocl_get_xdev(flash->pdev);
	struct resource *res;
	int ret, bar_idx;
	resource_size_t bar_off;

	res = platform_get_resource(to_platform_device(dev), IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	ret = xocl_ioaddr_to_baroff(xdev, res->start, &bar_idx, &bar_off);
	if (ret)
		return ret;

	return sprintf(buf, "%lld\n", bar_off);
}

static DEVICE_ATTR_RO(bar_off);

static ssize_t flash_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_flash *flash = dev_get_drvdata(dev);
	xdev_handle_t xdev = xocl_get_xdev(flash->pdev);
	ssize_t ret;

	if (flash->priv_data) {
		ret = sprintf(buf, "%s\n", flash->priv_data->flash_type);
	} else {
		ret = sprintf(buf, "%s\n", XDEV(xdev)->priv.flash_type);
	}

	return ret;
}

static DEVICE_ATTR_RO(flash_type);

static ssize_t properties_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_flash *flash = dev_get_drvdata(dev);

	if (flash->priv_data)
		return sprintf(buf, "%s\n", (char *)flash->priv_data +
			flash->priv_data->properties);
	return -EINVAL;
}

static DEVICE_ATTR_RO(properties);

/* Show size of one chip. Double it for total size for dual chip platforms. */
static ssize_t size_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_flash *flash = dev_get_drvdata(dev);

	return sprintf(buf, "%ld\n", flash->flash_size);
}

static DEVICE_ATTR_RO(size);

static struct attribute *flash_attrs[] = {
	&dev_attr_bar_off.attr,
	&dev_attr_flash_type.attr,
	&dev_attr_properties.attr,
	&dev_attr_size.attr,
	NULL,
};

static struct attribute_group flash_attr_group = {
	.attrs = flash_attrs,
};

static int sysfs_create_flash(struct xocl_flash *flash)
{
	int ret;

	ret  = sysfs_create_group(&flash->pdev->dev.kobj, &flash_attr_group);
	if (ret)
		FLASH_ERR(flash, "create sysfs failed %d", ret);
	else
		flash->sysfs_created = true;

	return ret;
}

static void sysfs_destroy_flash(struct xocl_flash *flash)
{
	if (flash->sysfs_created)
		sysfs_remove_group(&flash->pdev->dev.kobj, &flash_attr_group);
}

static int __flash_remove(struct platform_device *pdev)
{
	struct xocl_flash *flash;
	void *hdl;

	flash = platform_get_drvdata(pdev);
	if (!flash)
		return -EINVAL;

	xocl_drvinst_release(flash, &hdl);
	platform_set_drvdata(pdev, NULL);

	sysfs_destroy_flash(flash);

	if (flash->io_buf)
		vfree(flash->io_buf);

	if (flash->qspi_regs)
		iounmap(flash->qspi_regs);

	mutex_destroy(&flash->io_lock);
	xocl_drvinst_free(hdl);
	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void flash_remove(struct platform_device *pdev)
{
	__flash_remove(pdev);
}
#else
#define flash_remove __flash_remove
#endif

static int flash_probe(struct platform_device *pdev)
{
	struct xocl_flash *flash;
	int ret;

	flash = xocl_drvinst_alloc(&pdev->dev, sizeof(*flash));
	if (!flash)
		return -ENOMEM;

	platform_set_drvdata(pdev, flash);
	flash->pdev = pdev;

	mutex_init(&flash->io_lock);
	flash->priv_data = XOCL_GET_SUBDEV_PRIV(&pdev->dev);

	flash->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!flash->res) {
		ret = -EINVAL;
		FLASH_ERR(flash, "empty resource");
		goto error;
	}

	/*
	 * This driver only supports spi. For all flash other than spi,
	 * just create sysfs. 
	 */
	if (flash->priv_data && strcmp(flash->priv_data->flash_type, FLASH_TYPE_SPI))
		goto done;

	flash->qspi_regs = ioremap_nocache(flash->res->start,
		flash->res->end - flash->res->start + 1);
	if (!flash->qspi_regs) {
		ret = -ENOMEM;
		FLASH_ERR(flash, "failed to map resource");
		goto error;
	}

	ret = qspi_probe(flash);
	if (ret)
		goto error;

	flash->io_buf = vmalloc(flash->qspi_fifo_depth);
	if (flash->io_buf == NULL) {
		ret = -ENOMEM;
		goto error;
	}

done:	
	ret = sysfs_create_flash(flash);
	if (ret)
		goto error;

	return 0;

error:
	FLASH_ERR(flash, "probing failed");
	flash_remove(pdev);
	return ret;
}

/* Get size request from other parts of driver. */
static int flash_ksize(struct platform_device *pdev, size_t *n)
{
	struct xocl_flash *flash = platform_get_drvdata(pdev);
	*n = flash->flash_size;
	return 0;
}

/* Kernel APIs exported from this sub-device driver. */
static struct xocl_flash_funcs flash_ops = {
	.read		= flash_kread,
	.get_size	= flash_ksize,
};

static const struct file_operations flash_fops = {
	.owner = THIS_MODULE,
	.open = flash_open,
	.release = flash_close,
	.llseek = flash_llseek,
	.read = flash_read,
	.write = flash_write,
};

struct xocl_drv_private flash_priv = {
	.ops = &flash_ops,
	.fops = &flash_fops,
	.dev = -1,
};

struct platform_device_id flash_id_table[] = {
	{ XOCL_DEVNAME(XOCL_FLASH), (kernel_ulong_t)&flash_priv },
	{ },
};

static struct platform_driver	flash_driver = {
	.probe		= flash_probe,
	.remove		= flash_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_FLASH),
	},
	.id_table = flash_id_table,
};

int __init xocl_init_flash(void)
{
	int err = alloc_chrdev_region(&flash_priv.dev, 0, XOCL_MAX_DEVICES,
			XOCL_FLASH);
	if (err)
		return err;

	err = platform_driver_register(&flash_driver);
	if (err == 0)
		return 0;

	unregister_chrdev_region(flash_priv.dev, XOCL_MAX_DEVICES);
	return err;
}

void xocl_fini_flash(void)
{
	unregister_chrdev_region(flash_priv.dev, XOCL_MAX_DEVICES);
	platform_driver_unregister(&flash_driver);
}
