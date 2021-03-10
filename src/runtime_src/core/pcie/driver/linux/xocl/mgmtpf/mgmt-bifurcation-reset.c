/**
 *  Copyright (C) 2020 Xilinx, Inc. All rights reserved.
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

#include <linux/firmware.h>
#include "mgmt-core.h"
#include <linux/module.h>
#include "../xocl_drv.h"
#include "../xocl_xclbin.h"

#define XCLMGMT_RESET_MAX_RETRY		10
#define DUAL_FPGA_RESET_SLEEP		5

#if !defined(__PPC64__)
static void xclmgmt_reset_pci_pre(struct xclmgmt_dev *lro)
{
	struct pci_dev *pdev = lro->pci_dev;
	struct pci_bus *bus;

	mgmt_info(lro, "Reset PCI pre");

	/* what if user PF in VM ? */
	xocl_pci_save_config_all(lro);

	pci_disable_device(pdev);

	/* Reset secondary bus. */
	bus = pdev->bus;

	/*
	 * When flipping the SBR bit, device can fall off the bus. This is usually
	 * no problem at all so long as drivers are working properly after SBR.
	 * However, some systems complain bitterly when the device falls off the bus.
	 * Such as a Dell Servers, The iDRAC is totally independent from the
	 * operating system; it will still reboot the machine even if the operating
	 * system ignores the error.
	 * The quick solution is to temporarily disable the SERR reporting of
	 * switch port during SBR.
	 */
	pci_read_config_word(bus->self, PCI_COMMAND, &lro->pci_cmd);
	pci_write_config_word(bus->self, PCI_COMMAND, (lro->pci_cmd & ~PCI_COMMAND_SERR));
	pcie_capability_read_word(bus->self, PCI_EXP_DEVCTL, &lro->devctl);
	pcie_capability_write_word(bus->self, PCI_EXP_DEVCTL,
					   (lro->devctl & ~PCI_EXP_DEVCTL_FERE));
}

static void xclmgmt_reset_pci(struct xclmgmt_dev *lro)
{
	struct pci_dev *pdev = lro->pci_dev;
	struct pci_bus *bus;
	u8 pci_bctl;

	mgmt_info(lro, "Reset PCI");
	bus = pdev->bus;
	pci_read_config_byte(bus->self, PCI_BRIDGE_CONTROL, &pci_bctl);
	pci_bctl |= PCI_BRIDGE_CTL_BUS_RESET;
	pci_write_config_byte(bus->self, PCI_BRIDGE_CONTROL, pci_bctl);

	msleep(100);
	pci_bctl &= ~PCI_BRIDGE_CTL_BUS_RESET;
	pci_write_config_byte(bus->self, PCI_BRIDGE_CONTROL, pci_bctl);
	ssleep(DUAL_FPGA_RESET_SLEEP);
}

static void xclmgmt_reset_pci_post(struct xclmgmt_dev *lro)
{
	struct pci_dev *pdev = lro->pci_dev;
	struct pci_bus *bus;

	mgmt_info(lro, "Reset PCI post");
	bus = pdev->bus;

	pci_write_config_word(bus->self, PCI_COMMAND, (lro->pci_cmd | PCI_COMMAND_SERR));
	pcie_capability_write_word(bus->self, PCI_EXP_DEVCTL,
					   (lro->devctl | PCI_EXP_DEVCTL_FERE));

	pci_enable_device(pdev);

	xocl_wait_pci_status(pdev, 0, 0, 0);

	xocl_pci_restore_config_all(lro);

	xclmgmt_config_pci(lro);

	xocl_pmc_enable_reset(lro);
}

/**
 * Perform a PCIe secondary bus reset. Note: Use this method over pcie fundamental reset.
 * This method is known to work better.
 */

static long xclmgmt_hot_reset_pre(struct xclmgmt_dev *lro, bool force)
{
	long err = 0;
	const char *ep_name;
	struct pci_dev *pdev = lro->pci_dev;

	if (!pdev->bus || !pdev->bus->self) {
		mgmt_err(lro, "Unable to identify device root port for card %d",
		       lro->instance);
		err = -ENODEV;
		goto done;
	}

	ep_name = pdev->bus->name;
	mgmt_info(lro, "Trying to reset card %d in slot %s:%02x:%1x",
		lro->instance, ep_name,
		PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));

	if (!force && xrt_reset_syncup) {
		mgmt_info(lro, "wait for master off for all functions");
		err = xocl_wait_master_off(lro);
		if (err)
			goto done;
	}

	xocl_thread_stop(lro);

	/*
	 * lock pci config space access from userspace,
	 * save state and issue PCIe secondary bus reset
	 */
	if (!XOCL_DSA_PCI_RESET_OFF(lro)) {
		xocl_subdev_destroy_by_level(lro, XOCL_SUBDEV_LEVEL_URP);
		(void) xocl_subdev_offline_by_id(lro, XOCL_SUBDEV_UARTLITE);
		(void) xocl_subdev_offline_by_id(lro, XOCL_SUBDEV_FLASH);
		(void) xocl_subdev_offline_by_id(lro, XOCL_SUBDEV_ICAP);
		(void) xocl_subdev_offline_by_id(lro, XOCL_SUBDEV_MAILBOX);
		(void) xocl_subdev_offline_by_id(lro, XOCL_SUBDEV_AF);
		(void) xocl_subdev_offline_by_id(lro, XOCL_SUBDEV_AXIGATE);
		(void) xocl_subdev_offline_by_id(lro, XOCL_SUBDEV_PS);
		/* request XMC/ERT to stop */
		xocl_mb_stop(lro);
		/* If the PCIe board has PS */
		xocl_ps_sys_reset(lro);

		xclmgmt_reset_pci_pre(lro);
	} else {
		mgmt_warn(lro, "PCI Hot reset is not supported on this board.");
	}
done:
	return err;
}

static long xclmgmt_hot_reset_post(struct xclmgmt_dev *lro, bool force)
{
	long err = 0;
	struct xocl_board_private *dev_info = &lro->core.priv;
	int retry = 0;

	if (!XOCL_DSA_PCI_RESET_OFF(lro)) {
		xclmgmt_reset_pci_post(lro);

		/* restart XMC/ERT */
		xocl_mb_reset(lro);
		/* If the PCIe board has PS. This could take 50 seconds */
		(void) xocl_subdev_online_by_id(lro, XOCL_SUBDEV_PS);
		xocl_ps_wait(lro);
		(void) xocl_subdev_online_by_id(lro, XOCL_SUBDEV_AF);
		(void) xocl_subdev_online_by_id(lro, XOCL_SUBDEV_MAILBOX);
		(void) xocl_subdev_online_by_id(lro, XOCL_SUBDEV_ICAP);
		(void) xocl_subdev_online_by_id(lro, XOCL_SUBDEV_FLASH);
		(void) xocl_subdev_online_by_id(lro, XOCL_SUBDEV_UARTLITE);
	} else {
		mgmt_warn(lro, "PCI Hot reset is not supported on this board.");
	}

	/* Workaround for some DSAs. Flush axilite busses */
	if (dev_info->flags & XOCL_DSAFLAG_AXILITE_FLUSH)
		platform_axilite_flush(lro);

	/*
	 * Check firewall status. Status should be 0 (cleared)
	 * Otherwise issue message that a warm reboot is required.
	 */
	msleep(20);
	while (retry++ < XCLMGMT_RESET_MAX_RETRY && xocl_af_check(lro, NULL)) {
		xocl_af_clear(lro);
		msleep(20);
	}

	if (retry >= XCLMGMT_RESET_MAX_RETRY) {
		mgmt_err(lro, "Board is not able to recover by PCI Hot reset. "
			"Please warm reboot");
		return -EIO;
	}

	/* Workaround for some DSAs. Flush axilite busses */
	if (dev_info->flags & XOCL_DSAFLAG_AXILITE_FLUSH)
		platform_axilite_flush(lro);

	lro->reset_requested = false;
	xocl_thread_start(lro);

	xocl_clear_pci_errors(lro);
	store_pcie_link_info(lro);

	if (lro->preload_xclbin)
		xocl_xclbin_download(lro, lro->preload_xclbin);
	if (xrt_reset_syncup)
		xocl_set_master_on(lro);
	else if (!force)
		xclmgmt_connect_notify(lro, true);

	return err;
}
#endif

/*
 * For u30 with 2 FPGAs on one card, the POR pin for each FPGA are
 * physically connected together. Reset whichever will cause the other
 * one also being reset. So the logics here are
 * 1. before SBR is issued, if there are 2 FPGAs, do the preparations on
 *    both
 * 2. issue SBR on only one FPGA
 * 3. restore both after SBR
 *
 * Also, during reset, the pcie linkdown will last, in worst case, 3-5s
 * for the u30 card with PS/PL   
 */
long xclmgmt_hot_reset_bifurcation(struct xclmgmt_dev *lro,
	struct xclmgmt_dev *buddy_lro, bool force)
{
	long err = 0;

#if defined(__PPC64__)
	return -ENOTSUPP;
#else
	err = xclmgmt_hot_reset_pre(buddy_lro, force);
	if (err)
		return err;
	err = xclmgmt_hot_reset_pre(lro, force);
	if (err)
		return err;

	xclmgmt_reset_pci(lro);

	err = xclmgmt_hot_reset_post(buddy_lro, force);
	if (err)
		return err;
	return xclmgmt_hot_reset_post(lro, force);
#endif
}
