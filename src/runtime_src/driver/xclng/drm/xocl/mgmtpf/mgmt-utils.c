/**
 *  Copyright (C) 2017 Xilinx, Inc. All rights reserved.
 *
 *  Utility Functions for sysmon, axi firewall and other peripherals.
 *  Author: Umang Parekh
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include "mgmt-core.h"
#include <linux/module.h>
#include "../xocl_drv.h"

#define XCLMGMT_RESET_MAX_RETRY		10
void xocl_reset_notify(struct pci_dev *pdev, bool prepare);

/**
 * @returns: NULL if AER apability is not found walking up to the root port
 *         : pci_dev ptr to the port which is AER capable.
 */
static struct pci_dev * find_aer_cap(struct pci_dev *bridge)
{
	struct pci_dev *prev_bridge = bridge;
	int cap;

	if (bridge == NULL)
		return NULL;
	/*
	 * Walk the hierarchy up to the root port
	 **/
	do {
		printk(KERN_DEBUG "%s: inside do while loop..find_aer_cap \n", DRV_NAME);
		cap = pci_find_ext_capability(bridge, PCI_EXT_CAP_ID_ERR);
		if (cap) {
			printk(KERN_DEBUG "%s: AER capability found. \n", DRV_NAME);
			return bridge;
		}

		prev_bridge = bridge;
		bridge = bridge->bus->self;

		if (!bridge || prev_bridge == bridge) {
			printk(KERN_DEBUG "%s: AER capability not found. Ignoring boot command. \n", DRV_NAME);
			return NULL;
		}

	} while (pci_pcie_type(bridge) != PCI_EXP_TYPE_ROOT_PORT);

	return NULL;
}

/*
 * pcie_(un)mask_surprise_down inspired by myri10ge driver, myri10ge.c
 */
static int pcie_mask_surprise_down(struct pci_dev *pdev, u32 *orig_mask)
{
	struct pci_dev *bridge = pdev->bus->self;
	int cap;
	u32 mask;

	printk(KERN_INFO "%s: pcie_mask_surprise_down \n", DRV_NAME);

	bridge = find_aer_cap(bridge);
	if(bridge) {
		cap = pci_find_ext_capability(bridge, PCI_EXT_CAP_ID_ERR);
		if (cap) {
			pci_read_config_dword(bridge, cap + PCI_ERR_UNCOR_MASK, orig_mask);
			mask = *orig_mask;
			mask |= 0x20;
			pci_write_config_dword(bridge, cap + PCI_ERR_UNCOR_MASK, mask);
			return 0;
		}
	}

	return -ENOSYS;
}

static int pcie_unmask_surprise_down(struct pci_dev *pdev, u32 orig_mask)
{
	struct pci_dev *bridge = pdev->bus->self;
	int cap;

	printk(KERN_DEBUG "%s: pcie_unmask_surprise_down \n", DRV_NAME);

	bridge = find_aer_cap(bridge);
	if(bridge) {
		cap = pci_find_ext_capability(bridge, PCI_EXT_CAP_ID_ERR);
		if (cap) {
			pci_write_config_dword(bridge, cap + PCI_ERR_UNCOR_MASK, orig_mask);
			return 0;
		}
	}

	return -ENOSYS;
}

void freezeAXIGate(struct xclmgmt_dev *lro)
{
	(void) xocl_icap_freeze_axi_gate(lro);
}

void freeAXIGate(struct xclmgmt_dev *lro)
{
	(void) xocl_icap_free_axi_gate(lro);
}

/**
 * Prepare the XOCL engine for reset
 * prepare = true will call xdma_offline
 * prepare = false will call xdma online
 */
void xocl_reset(struct xclmgmt_dev *lro, bool prepare)
{
	struct pci_dev *pdev = lro->user_pci_dev;
	struct mailbox_req mbreq = { 0 };
	int err = -EINVAL;
	size_t resplen = sizeof (err);
	void (*reset)(struct pci_dev *pdev, bool prepare);

	mbreq.req = prepare ?
		MAILBOX_REQ_RESET_BEGIN : MAILBOX_REQ_RESET_END;
	(void) xocl_peer_request(lro, &mbreq, &err, &resplen, NULL, NULL);
	if (err != 0) {
		/* Fallback to our hacky way if mailbox is not available. */
		mgmt_err(lro, "cannot reset peer via mailbox, err=%d", err);
		reset = symbol_get(xocl_reset_notify);
		if (reset) {
			mgmt_err(lro, "calling xocl_reset_notify() directly");
			device_lock(&pdev->dev);
			reset(pdev, prepare);
			device_unlock(&pdev->dev);
			symbol_put(xocl_reset_notify);
		}
	}
}

/**
 * Workaround for some DSAs that need axilite bus flushed after reset
 */
void platform_axilite_flush(struct xclmgmt_dev *lro)
{
	u32 val, i;

	mgmt_info(lro, "Flushing axilite busses.");

	/* The flush sequence works as follows:
	 * Read axilite peripheral up to 4 times
	 * Check if firewall trips and clear it.
	 * Touch all axilite interconnects with clock crossing
	 * in platform which requires reading multiple peripherals
	 * (Feature ROM, MB Reset GPIO, Sysmon)
	 */
	for (i = 0; i < 4; i++) {
		val = MGMT_READ_REG32(lro, FEATURE_ROM_BASE);
		xocl_af_clear(lro);
	}

	for (i = 0; i < 4; i++) {
		val = MGMT_READ_REG32(lro, MB_GPIO);
		xocl_af_clear(lro);
	}

	for (i = 0; i < 4; i++) {
		val = MGMT_READ_REG32(lro, SYSMON_BASE);
		xocl_af_clear(lro);
	}

	for (i = 0; i < 4; i++) {
		val = MGMT_READ_REG32(lro, MB_IMAGE_SCHE);
		xocl_af_clear(lro);
	}

	for (i = 0; i < 4; i++) {
                val = MGMT_READ_REG32(lro, XHWICAP_CR);
		xocl_af_clear(lro);
        }

        for (i = 0; i < 4; i++) {
                val = MGMT_READ_REG32(lro, GPIO_NULL_BASE);
		xocl_af_clear(lro);
        }

        for (i = 0; i < 4; i++) {
                val = MGMT_READ_REG32(lro, AXI_GATE_BASE);
		xocl_af_clear(lro);
        }
}

/**
 * Perform a PCIe secondary bus reset. Note: Use this method over pcie fundamental reset.
 * This method is known to work better.
 */

long reset_hot_ioctl(struct xclmgmt_dev *lro)
{
	long err = 0;
	const char *ep_name;
	struct pci_dev *pdev = lro->pci_dev;
	int retry = 0;


	if (!pdev->bus || !pdev->bus->self) {
		mgmt_err(lro, "Unable to identify device root port for card %d",
		       lro->instance);
		err = -ENODEV;
		goto done;
	}

	ep_name = pdev->bus->name;
#if defined(__PPC64__)
	mgmt_err(lro, "Ignore reset operation for card %d in slot %s:%02x:%1x",
		lro->instance, ep_name,
		PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));
#else
	mgmt_err(lro, "Trying to reset card %d in slot %s:%02x:%1x",
		lro->instance, ep_name,
		PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));

	if (lro->user_pci_dev) {
		/*
 		 * Make the xdma offline before issuing hot reset.
		 * assume all axi access from userpf is stopped
		 * busy lock is taken to make sure no axi access at this time
		 */
		xocl_reset(lro, true);
	}

	if (!lro->reset_firewall) {
		/*
		 * if reset request comes from IOCTL, reset kernel and
		 * Microblaze.
		 */
		freezeAXIGate(lro);
		msleep(500);
		freeAXIGate(lro);
		msleep(500);
	}

	/*
	 * lock pci config space access from userspace,
	 * save state and issue PCIe secondary bus reset
	 */
	if (!XOCL_DSA_PCI_RESET_OFF(lro)) {
		(void) xocl_mailbox_reset(lro, false);
		xclmgmt_reset_pci(lro);
		(void) xocl_mailbox_reset(lro, true);
	} else {
		mgmt_err(lro, "PCI Hot reset is not supported on this board.");
	}

	/* Workaround for some DSAs. Flush axilite busses */
	platform_axilite_flush(lro);

	/*
	 * Check firewall status. Status should be 0 (cleared)
	 * Otherwise issue message that a warm reboot is required.
	 */
	do {
		msleep(20);
	} while (retry++ < XCLMGMT_RESET_MAX_RETRY &&
		xocl_af_check(lro, NULL));

	if (retry >= XCLMGMT_RESET_MAX_RETRY){
		mgmt_err(lro, "Board is not able to recover by PCI Hot reset. "
			"Please warm reboot");
		return -EIO;
	}
	
	//Also freeze and free AXI gate to reset the OCL region.
	freezeAXIGate(lro);
	msleep(500);
	freeAXIGate(lro);
	msleep(500);

	if (lro->user_pci_dev) {
		// Bring the xdma online
		xocl_reset(lro, false);
	}

	/* Workaround for some DSAs. Flush axilite busses */
	platform_axilite_flush(lro);
	/*
 	 * Potential redudant stop on MB in case it was in a bad state
 	 * TODO: We should not need to reload elf, but doing anyways
 	 */
	xocl_mb_reset(lro);

#endif
done:
	return err;
}
/*
 * Inspired by GenWQE driver, card_base.c
 */
int pci_fundamental_reset(struct xclmgmt_dev *lro)
{
	int rc;
	u32 orig_mask;
	u8 hot;
	struct pci_dev *pci_dev = lro->pci_dev;

	/* Make the user pf offline before issuing reset. */
	if (lro->user_pci_dev) {
		xocl_reset(lro, true);
	}

	//freeze and free AXI gate to reset the OCL region before and after the pcie reset.
	freezeAXIGate(lro);
	msleep(500);
	freeAXIGate(lro);
	msleep(500);

	/*
	 * lock pci config space access from userspace,
	 * save state and issue PCIe fundamental reset
	 */
	printk(KERN_INFO "%s: pci_fundamental_reset \n", DRV_NAME);

	// Save pci config space for botht the pf's
	pci_cfg_access_lock(pci_dev);
	pci_save_state(pci_dev);
	if (lro->user_pci_dev) {
		pci_cfg_access_lock(lro->user_pci_dev);
		pci_save_state(lro->user_pci_dev);
	}

	rc = pcie_mask_surprise_down(pci_dev, &orig_mask);
	if (rc)
		goto done;
	printk(KERN_INFO "%s: pci_fundamental_reset 1\n", DRV_NAME);

#if defined(__PPC64__)
	/*
	 * On PPC64LE use pcie_warm_reset which will cause the FPGA to
	 * reload from PROM
	 */
	rc = pci_set_pcie_reset_state(pci_dev, pcie_warm_reset);
	if (rc)
		goto done;
	/* keep PCIe reset asserted for 250ms */
	msleep(250);
	rc = pci_set_pcie_reset_state(pci_dev, pcie_deassert_reset);
	if (rc)
		goto done;
	/* Wait for 2s to reload flash and train the link */
	msleep(2000);
#else
	rc = xocl_icap_reset_bitstream(lro);
	if (rc)
		goto done;

	printk(KERN_INFO "%s: pci_fundamental_reset 2\n", DRV_NAME);
	/* Now perform secondary bus reset which should reset most of the device */
	pci_read_config_byte(pci_dev->bus->self, PCI_MIN_GNT, &hot);
	/* Toggle the PCIe hot reset bit in the root port */
	pci_write_config_byte(pci_dev->bus->self, PCI_MIN_GNT, hot | 0x40);
	msleep(500);
	pci_write_config_byte(pci_dev->bus->self, PCI_MIN_GNT, hot);
	msleep(500);
#endif
done:
	printk(KERN_INFO "%s: pci_fundamental_reset done routine\n", DRV_NAME);

	// restore pci config space for botht the pf's
	pci_restore_state(pci_dev);
	rc = pcie_unmask_surprise_down(pci_dev, orig_mask);
	pci_cfg_access_unlock(pci_dev);
	if (lro->user_pci_dev) {
		pci_restore_state(lro->user_pci_dev);
		pci_cfg_access_unlock(lro->user_pci_dev);
	}

	//Also freeze and free AXI gate to reset the OCL region.
	freezeAXIGate(lro);
	msleep(500);
	freeAXIGate(lro);
	msleep(500);

	// Bring the user pf online
	if (lro->user_pci_dev) {
		xocl_reset(lro, false);
	}

	// TODO: Figure out a way to reinit DMA engine which is other PF
	//if (!rc)
	//	rc = reinit(lro);
	return rc;
}

unsigned compute_unit_busy(struct xclmgmt_dev *lro)
{
	int i = 0;
	unsigned result = 0;
	u32 r = MGMT_READ_REG32(lro, AXI_GATE_BASE_RD_BASE);

	/*
	 * r != 0x3 implies that OCL region is isolated and we cannot read 
	 * CUs' status
	 */
	if (r != 0x3)
		return 0;

	/* ?? Assumed only 16 CUs ? */
	for (i = 0; i < 16; i++) {
		r = MGMT_READ_REG32(lro, OCL_CTLR_BASE + i * OCL_CU_CTRL_RANGE);
		if (r == 0x1)
			result |= (r << i);
	}
	return result;
}

void xclmgmt_reset_pci(struct xclmgmt_dev *lro)
{
	struct pci_dev *pdev = lro->pci_dev;
	struct pci_bus *bus;
	int i;
	u16 pci_cmd;
	u8 pci_bctl;

	mgmt_info(lro, "Reset PCI");
	
        pci_cfg_access_lock(pdev);
	pci_save_state(pdev);

	/* what if user PF in VM ? */
	if (lro->user_pci_dev) {
		pci_cfg_access_lock(lro->user_pci_dev);
		pci_save_state(lro->user_pci_dev);
	}
	/* Reset secondary bus. */
	bus = pdev->bus;
	pci_read_config_byte(bus->self, PCI_BRIDGE_CONTROL, &pci_bctl);
	pci_bctl |= PCI_BRIDGE_CTL_BUS_RESET;
	pci_write_config_byte(bus->self, PCI_BRIDGE_CONTROL, pci_bctl);

	msleep(100);
	pci_bctl &= ~PCI_BRIDGE_CTL_BUS_RESET;
	pci_write_config_byte(bus->self, PCI_BRIDGE_CONTROL, pci_bctl);

	for (i = 0; i < 5000; i++) {
		pci_read_config_word(pdev, PCI_COMMAND, &pci_cmd);
		if (pci_cmd != 0xffff)
			break;
		msleep(1);
	}

	mgmt_info(lro, "Resetting for %d ms", i); 

	if (lro->user_pci_dev) {
		pci_restore_state(lro->user_pci_dev);
		pci_cfg_access_unlock(lro->user_pci_dev);
	}

	pci_restore_state(pdev);
	pci_cfg_access_unlock(pdev);
}
