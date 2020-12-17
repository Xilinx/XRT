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

#include <linux/firmware.h>
#include "mgmt-core.h"
#include <linux/module.h>
#include "../xocl_drv.h"

#define XCLMGMT_RESET_MAX_RETRY		10

static void xclmgmt_reset_pci(struct xclmgmt_dev *lro);
/**
 * @returns: NULL if AER apability is not found walking up to the root port
 *         : pci_dev ptr to the port which is AER capable.
 */
static struct pci_dev *find_aer_cap(struct pci_dev *bridge)
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
	if (bridge) {
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
	if (bridge) {
		cap = pci_find_ext_capability(bridge, PCI_EXT_CAP_ID_ERR);
		if (cap) {
			pci_write_config_dword(bridge, cap + PCI_ERR_UNCOR_MASK, orig_mask);
			return 0;
		}
	}

	return -ENOSYS;
}

/**
 * Workaround for some DSAs that need axilite bus flushed after reset
 */
void platform_axilite_flush(struct xclmgmt_dev *lro)
{
	u32 val, i, gpio_val;

	mgmt_info(lro, "Flushing axilite busses.");

	/* The flush sequence works as follows:
	 * Read axilite peripheral up to 4 times
	 * Check if firewall trips and clear it.
	 * Touch all axilite interconnects with clock crossing
	 * in platform which requires reading multiple peripherals
	 * (Feature ROM, MB Reset GPIO, Sysmon)
	 */
	for (i = 0; i < 4; i++) {
		val = MGMT_READ_REG32(lro, _FEATURE_ROM_BASE);
		xocl_af_clear(lro);
	}

	for (i = 0; i < 4; i++) {
		gpio_val = MGMT_READ_REG32(lro, _MB_GPIO);
		xocl_af_clear(lro);
	}

	for (i = 0; i < 4; i++) {
		val = MGMT_READ_REG32(lro, _SYSMON_BASE);
		xocl_af_clear(lro);
	}

	/* Can only read this safely if not in reset */
	if (gpio_val == 1) {
		for (i = 0; i < 4; i++) {
			val = MGMT_READ_REG32(lro, _MB_IMAGE_SCHE);
			xocl_af_clear(lro);
		}
	}

	for (i = 0; i < 4; i++) {
		val = MGMT_READ_REG32(lro, _XHWICAP_CR);
		xocl_af_clear(lro);
	}

	for (i = 0; i < 4; i++) {
		val = MGMT_READ_REG32(lro, _GPIO_NULL_BASE);
		xocl_af_clear(lro);
	}

	for (i = 0; i < 4; i++) {
		val = MGMT_READ_REG32(lro, _AXI_GATE_BASE);
		xocl_af_clear(lro);
	}
}

static int xocl_match_slot_and_wait(struct device *dev, void *data)
{
	struct xclmgmt_dev *lro = data;
	struct pci_dev *pdev;
	int userpf= -1;
	int ret = 0;

	pdev = to_pci_dev(dev);

	if (lro->core.fdt_blob) {
		userpf = xocl_fdt_get_userpf(lro, lro->core.fdt_blob);
		if (userpf < 0) {
			mgmt_err(lro, "can not find userpf");
			return -EINVAL;
		}
	}

	if (pdev != lro->core.pdev &&
		(XOCL_DEV_ID(pdev) >> 3) == (XOCL_DEV_ID(lro->pci_dev) >> 3) &&
		(userpf < 0 || PCI_FUNC(pdev->devfn) == userpf))
		ret = xocl_wait_pci_status(pdev, PCI_COMMAND_MASTER, 0, 60);

	return ret;
}

int xocl_wait_master_off(struct xclmgmt_dev *lro)
{
	return bus_for_each_dev(&pci_bus_type, NULL, lro, xocl_match_slot_and_wait);
}

static int xocl_match_slot_set_master(struct device *dev, void *data)
{
	struct xclmgmt_dev *lro = data;
	struct pci_dev *pdev;
	u16 pci_cmd;
	int userpf = -1;
	int ret = 0;

	pdev = to_pci_dev(dev);

	if (lro->core.fdt_blob) {
		userpf = xocl_fdt_get_userpf(lro, lro->core.fdt_blob);
		if (userpf < 0) {
			mgmt_err(lro, "can not find userpf");
			return -EINVAL;
		}
	}

	if (pdev != lro->core.pdev &&
		(XOCL_DEV_ID(pdev) >> 3) == (XOCL_DEV_ID(lro->pci_dev) >> 3) &&
		(userpf < 0 || PCI_FUNC(pdev->devfn) == userpf)) {
		pci_read_config_word(pdev, PCI_COMMAND, &pci_cmd);
		if (!(pci_cmd & PCI_COMMAND_MASTER)) {
			pci_cmd |= PCI_COMMAND_MASTER;
			pci_write_config_word(pdev, PCI_COMMAND, pci_cmd);
		}
	}

	return ret;
}

int xocl_set_master_on(struct xclmgmt_dev *lro)
{
	return bus_for_each_dev(&pci_bus_type, NULL, lro, xocl_match_slot_set_master);
}

/*
 * On u30, there are 2 FPGAs, due to the issue of
 * https://jira.xilinx.com/browse/ALVEO-266
 * reset either FPGA will cause the other one being reset too.
 * A workaround is required to handle this case
 */
static int xclmgmt_get_buddy_cb(struct device *dev, void *data)
{
	struct xclmgmt_dev *src_xdev = *(struct xclmgmt_dev **)(data);
	struct xclmgmt_dev *tgt_xdev;

	/*
	 * skip
	 * 1.non xilinx device
	 * 2.itself
	 * 3.other devcies not being droven by same driver. using func id
	 * may not handle u25 where there is another device on same card 
	 */
	if (!src_xdev || !dev || to_pci_dev(dev)->vendor != 0x10ee ||
	   	XOCL_DEV_ID(to_pci_dev(dev)) ==
		XOCL_DEV_ID(src_xdev->core.pdev) ||
		strcmp(dev->driver->name, "xclmgmt")) 
		return 0;

	tgt_xdev = dev_get_drvdata(dev);
	if (tgt_xdev && strcmp(src_xdev->core.serial_num, "") &&
		strcmp(tgt_xdev->core.serial_num, "") &&
		!strcmp(src_xdev->core.serial_num, tgt_xdev->core.serial_num)) {
	       *(struct xclmgmt_dev **)data = tgt_xdev;
		mgmt_info(src_xdev, "2nd FPGA found on same card: %x:%x.%x",
			to_pci_dev(dev)->bus->number,
			PCI_SLOT(to_pci_dev(dev)->devfn),
			PCI_FUNC(to_pci_dev(dev)->devfn));
       		return 1;
	}
	return 0;
}

/**
 * Perform a PCIe secondary bus reset. Note: Use this method over pcie fundamental reset.
 * This method is known to work better.
 */

long xclmgmt_hot_reset(struct xclmgmt_dev *lro, bool force)
{
	long err = 0;
	const char *ep_name;
	struct pci_dev *pdev = lro->pci_dev;
	struct xocl_board_private *dev_info = &lro->core.priv;
	int retry = 0;
	struct xclmgmt_dev *buddy_lro = lro;

	/*
	 * if 2nd FPGA is found, buddy_lro is set as lro of the other
	 * one, otherwise, it is set as null
	 */
	if (!xocl_get_buddy_fpga(&buddy_lro, xclmgmt_get_buddy_cb))
		buddy_lro = NULL;
	if (buddy_lro)
		return xclmgmt_hot_reset_bifurcation(lro, buddy_lro, force);

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
		/* request XMC/ERT to stop */
		xocl_mb_stop(lro);
		/* If the PCIe board has PS */
		xocl_ps_sys_reset(lro);
#if defined(__PPC64__)
		pci_fundamental_reset(lro);
#else
		xclmgmt_reset_pci(lro);
#endif

		/* restart XMC/ERT */
		xocl_mb_reset(lro);
		/* If the PCIe board has PS. This could take 50 seconds */
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
	if (xrt_reset_syncup)
		xocl_set_master_on(lro);
	else if (!force)
		xclmgmt_connect_notify(lro, true);

done:
	return err;
}

static void xocl_save_config_space(struct pci_dev *pdev, u32 *saved_config)
{
	int i;

	for (i = 0; i < 16; i++)
		pci_read_config_dword(pdev, i * 4, &saved_config[i]);
}

static int xocl_match_slot_and_save(struct device *dev, void *data)
{
	struct xclmgmt_dev *lro = data;
	struct pci_dev *pdev;

	pdev = to_pci_dev(dev);

	if ((XOCL_DEV_ID(pdev) >> 3) == (XOCL_DEV_ID(lro->pci_dev) >> 3)) {
		pci_cfg_access_lock(pdev);
		pci_save_state(pdev);
		xocl_save_config_space(pdev,
				lro->saved_config[PCI_FUNC(pdev->devfn)]);
	}

	return 0;
}

void xocl_pci_save_config_all(struct xclmgmt_dev *lro)
{
	bus_for_each_dev(&pci_bus_type, NULL, lro, xocl_match_slot_and_save);
}

static void xocl_restore_config_space(struct pci_dev *pdev, u32 *config_saved)
{
	int i;
	u32 val;

	for (i = 0; i < 16; i++) {
		pci_read_config_dword(pdev, i * 4, &val);
		if (val == config_saved[i])
			continue;

		pci_write_config_dword(pdev, i * 4, config_saved[i]);
		pci_read_config_dword(pdev, i * 4, &val);
		if (val != config_saved[i]) {
			xocl_err(&pdev->dev,
				"restore config at %d failed", i * 4);
		}
	}
}

static int xocl_match_slot_and_restore(struct device *dev, void *data)
{
	struct xclmgmt_dev *lro = data;
	struct pci_dev *pdev;

	pdev = to_pci_dev(dev);

	if ((XOCL_DEV_ID(pdev) >> 3) == (XOCL_DEV_ID(lro->pci_dev) >> 3)) {
		xocl_restore_config_space(pdev,
			lro->saved_config[PCI_FUNC(pdev->devfn)]);

		/*
		 * For U50 built with 2RP flow. PLP Gate is closed after
		 * pci hot reset.
		 * XRT expects firewall trip instead of hard hang if there
		 * is an unexpected access of non-exist
		 * IPs. (E.g. Invalid access from a active VM)
		 *
		 * However there is a old u50 gen3x4-xdma-base_2-2902115
		 * which will hard hang host if we do not open PLP gate before
		 * restoring pci state and unlock access
		 *
		 * for the new platforms which support pcie firewall which
		 * should be able to block some of the unexpected access
		 * (BAR 0 access will not be blocked by pcie firewall.
		 */
		(void) xocl_subdev_online_by_id(lro, XOCL_SUBDEV_AXIGATE);

		pci_restore_state(pdev);
		pci_cfg_access_unlock(pdev);
	}

	return 0;
}

void xocl_pci_restore_config_all(struct xclmgmt_dev *lro)
{
	bus_for_each_dev(&pci_bus_type, NULL, lro, xocl_match_slot_and_restore);
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

	/*
	 * lock pci config space access from userspace,
	 * save state and issue PCIe fundamental reset
	 */
	printk(KERN_INFO "%s: pci_fundamental_reset \n", DRV_NAME);

	/* Save pci config space for botht the pf's */
	xocl_pci_save_config_all(lro);

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

	/* restore pci config space for botht the pf's */
	rc = pcie_unmask_surprise_down(pci_dev, orig_mask);

	xocl_pci_restore_config_all(lro);

	return rc;
}

static void xclmgmt_reset_pci(struct xclmgmt_dev *lro)
{
	struct pci_dev *pdev = lro->pci_dev;
	struct pci_bus *bus;
	u8 pci_bctl;
	u16 pci_cmd, devctl;

	mgmt_info(lro, "Reset PCI");

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
	pci_read_config_word(bus->self, PCI_COMMAND, &pci_cmd);
	pci_write_config_word(bus->self, PCI_COMMAND, (pci_cmd & ~PCI_COMMAND_SERR));
	pcie_capability_read_word(bus->self, PCI_EXP_DEVCTL, &devctl);
	pcie_capability_write_word(bus->self, PCI_EXP_DEVCTL,
					   (devctl & ~PCI_EXP_DEVCTL_FERE));

	pci_read_config_byte(bus->self, PCI_BRIDGE_CONTROL, &pci_bctl);
	pci_bctl |= PCI_BRIDGE_CTL_BUS_RESET;
	pci_write_config_byte(bus->self, PCI_BRIDGE_CONTROL, pci_bctl);

	msleep(100);
	pci_bctl &= ~PCI_BRIDGE_CTL_BUS_RESET;
	pci_write_config_byte(bus->self, PCI_BRIDGE_CONTROL, pci_bctl);
	ssleep(1);

	pcie_capability_write_word(bus->self, PCI_EXP_DEVCTL, devctl);
	pci_write_config_word(bus->self, PCI_COMMAND, pci_cmd);

	pci_enable_device(pdev);

	xocl_wait_pci_status(pdev, 0, 0, 0);

	xocl_pci_restore_config_all(lro);

	xclmgmt_config_pci(lro);

	xocl_pmc_enable_reset(lro);
}

int xclmgmt_update_userpf_blob(struct xclmgmt_dev *lro)
{
	int len, userpf_idx;
	int ret;
	struct FeatureRomHeader	rom_header;
	int offset;
	const int *version;

	if (!lro->core.fdt_blob)
		return 0;

	len = fdt_totalsize(lro->core.fdt_blob) + sizeof(rom_header) + 1024;
	/* assuming device tree is no bigger than 100MB */
	if (len > 100 * 1024 * 1024)
		return -EINVAL;

	if (lro->userpf_blob)
		vfree(lro->userpf_blob);

	lro->userpf_blob = vzalloc(len);
	if (!lro->userpf_blob)
		return -ENOMEM;

	ret = fdt_create_empty_tree(lro->userpf_blob, len);
	if (ret) {
		mgmt_err(lro, "create fdt failed %d", ret);
		goto failed;
	}

	userpf_idx = xocl_fdt_get_userpf(lro, lro->core.fdt_blob);
	if (userpf_idx >= 0) {
		ret = xocl_fdt_overlay(lro->userpf_blob, 0,
			lro->core.fdt_blob, 0, userpf_idx, -1);
		if (ret) {
			mgmt_err(lro, "overlay fdt failed %d", ret);
			goto failed;
		}
	}

	ret = xocl_get_raw_header(lro, &rom_header);
	if (ret) {
		mgmt_err(lro, "get featurerom raw header failed %d", ret);
		goto failed;
	}

	ret = xocl_fdt_add_pair(lro, lro->userpf_blob, "vrom", &rom_header,
		sizeof(rom_header));
	if (ret) {
		mgmt_err(lro, "add vrom failed %d", ret);
		goto failed;
	}

	/* Get ERT firmware major version from mgmtpf blob */
	offset = xocl_fdt_path_offset(lro, lro->core.fdt_blob,
					"/" NODE_ENDPOINTS "/" NODE_ERT_FW_MEM
					"/" NODE_FIRMWARE);
	if (offset < 0) {
		mgmt_info(lro, "firmware node is not in %s", NODE_ERT_FW_MEM);
		goto done;
	}
	version = xocl_fdt_getprop(lro, lro->core.fdt_blob, offset, PROP_VERSION_MAJOR, NULL);

	/* Add ERT firmware major version to userpf blob */
	offset = xocl_fdt_path_offset(lro, lro->userpf_blob,
					"/" NODE_ENDPOINTS "/" NODE_ERT_SCHED);
	if (offset < 0) {
		mgmt_err(lro, "get ert sched node failed %d", offset);
	}
	(void) xocl_fdt_setprop(lro, lro->userpf_blob, offset, PROP_VERSION_MAJOR,
				version, sizeof(int));

done:
	fdt_pack(lro->userpf_blob);
	lro->userpf_blob_updated = true;

	return 0;

failed:
	if (lro->userpf_blob) {
		vfree(lro->userpf_blob);
		lro->userpf_blob = NULL;
	}

	return ret;
}

int xclmgmt_program_shell(struct xclmgmt_dev *lro)
{
	int ret;
	char *blob = NULL;
	int len;

	if (!lro->core.fdt_blob && xocl_get_timestamp(lro) == 0)
		xclmgmt_load_fdt(lro);

	blob = lro->core.fdt_blob;
	if (!blob) {
		mgmt_err(lro, "Can not get dtb");
		ret = -EINVAL;
		goto failed;
	}
	len = fdt_totalsize(lro->core.blp_blob);
	if (len > 100 * 1024 * 1024) {
		mgmt_err(lro, "dtb is too big");
		ret = -EINVAL;
		goto failed;
	}
	lro->core.fdt_blob = vmalloc(len);
	if (!lro->core.fdt_blob) {
		ret = -ENOMEM;
		lro->core.fdt_blob = blob;
		goto failed;
	}
	memcpy(lro->core.fdt_blob, lro->core.blp_blob, len);
	ret = xocl_icap_download_rp(lro, XOCL_SUBDEV_LEVEL_PRP,
			RP_DOWNLOAD_DRY);
	if (ret) {
		vfree(lro->core.fdt_blob);
		lro->core.fdt_blob = blob;
		goto failed;
	}

	vfree(blob);

	/* dry run passed, any failure below will cause device offline */

	xocl_drvinst_set_offline(lro, true);

	xocl_thread_stop(lro);

	ret = xocl_subdev_destroy_prp(lro);
	if (ret) {
		mgmt_err(lro, "destroy prp failed %d", ret);
		goto failed;
	}

	xocl_subdev_destroy_by_id(lro, XOCL_SUBDEV_AF);

	ret = xocl_icap_download_rp(lro, XOCL_SUBDEV_LEVEL_PRP,
			RP_DOWNLOAD_FORCE);
	if (ret) {
		mgmt_err(lro, "program shell failed %d", ret);
		goto failed;
	}

	xocl_subdev_create_by_id(lro, XOCL_SUBDEV_AF);

	ret = xocl_subdev_create_prp(lro);
	if (ret && ret != -ENODEV) {
		mgmt_err(lro, "failed to create prp %d", ret);
		goto failed;
	}

	(void) xocl_peer_listen(lro, xclmgmt_mailbox_srv, (void *)lro);

	/* reload possible cmc and ert images */
	xocl_icap_post_download_rp(lro);

	xocl_thread_start(lro);

	xclmgmt_update_userpf_blob(lro);
	xocl_drvinst_set_offline(lro, false);

failed:

	return ret;

}

int xclmgmt_load_fdt(struct xclmgmt_dev *lro)
{
	struct xocl_board_private *dev_info = &lro->core.priv;
	const struct axlf_section_header	*dtc_header;
	struct axlf				*bin_axlf;
	int					ret;
	char					*fw_buf = NULL;
	size_t					fw_size = 0;


	if (xocl_subdev_is_vsec_recovery(lro)) {
		mgmt_info(lro, "Skip load_fdt for vsec Golden image");
		(void) xocl_peer_listen(lro, xclmgmt_mailbox_srv, (void *)lro);
		return 0;
	}

	mutex_lock(&lro->busy_mutex);
	ret = xocl_rom_load_firmware(lro, &fw_buf, &fw_size);
	if (ret)
		goto failed;
	bin_axlf = (struct axlf *)fw_buf;

	dtc_header = xocl_axlf_section_header(lro, bin_axlf, PARTITION_METADATA);
	if (!dtc_header) {
		ret = -ENOENT;
		mgmt_err(lro, "firmware does not contain PARTITION_METADATA");
		goto failed;
	}

	ret = xocl_fdt_blob_input(lro,
			(char *)fw_buf + dtc_header->m_sectionOffset,
			dtc_header->m_sectionSize, XOCL_SUBDEV_LEVEL_BLD,
			bin_axlf->m_header.m_platformVBNV);
	if (ret) {
		mgmt_err(lro, "Invalid PARTITION_METADATA");
		goto failed;
	}

	if (lro->core.priv.flags & XOCL_DSAFLAG_MFG) {
		/* Minimum set up for golden image. */
		(void) xocl_subdev_create_by_id(lro, XOCL_SUBDEV_FLASH);
		(void) xocl_subdev_create_by_id(lro, XOCL_SUBDEV_MB);
		goto failed;
	}

	lro->core.blp_blob = vmalloc(fdt_totalsize(lro->core.fdt_blob));
	if (!lro->core.blp_blob) {
		ret = -ENOMEM;
		goto failed;
	}
	memcpy(lro->core.blp_blob, lro->core.fdt_blob,
			fdt_totalsize(lro->core.fdt_blob));

	xclmgmt_connect_notify(lro, false);
	xocl_subdev_destroy_all(lro);
	ret = xocl_subdev_create_all(lro);
	if (ret)
		goto failed;

	/* VERSAL doesn't have icap to download, will need to refactor the code */
	if (!(dev_info->flags & XOCL_DSAFLAG_VERSAL))
		ret = xocl_icap_download_boot_firmware(lro);

	if (ret)
		goto failed;

	xclmgmt_update_userpf_blob(lro);

	xocl_thread_start(lro);

	/* Launch the mailbox server. */
	(void) xocl_peer_listen(lro, xclmgmt_mailbox_srv, (void *)lro);

	lro->ready = true;

failed:
	vfree(fw_buf);
	mutex_unlock(&lro->busy_mutex);

	return ret;
}

void xclmgmt_ocl_reset(struct xclmgmt_dev *lro)
{
	xocl_icap_reset_axi_gate(lro);
}

void xclmgmt_ert_reset(struct xclmgmt_dev *lro)
{
	/* This is for reset PS ERT */
	xocl_ps_reset(lro);
	xocl_ps_wait(lro);
}

void xclmgmt_softkernel_reset(struct xclmgmt_dev *lro)
{
	xocl_ps_sk_reset(lro);
}
