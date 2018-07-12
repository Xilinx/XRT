/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2018 Xilinx, Inc. All rights reserved.
 *
 * Authors: Sonal Santan
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 */

#include <linux/version.h>
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,0,0)
#include <drm/drm_backport.h>
#endif
#include <drm/drmP.h>
#include <drm/drm_gem.h>
#include <drm/drm_mm.h>
#include <linux/eventfd.h>
#include <linux/uuid.h>
#include "common.h"

#if defined(XOCL_UUID)
xuid_t uuid_null = NULL_UUID_LE;
#endif

int xocl_info_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct drm_xocl_info *obj = data;
	struct xocl_dev *xdev = dev->dev_private;
	struct pci_dev *pdev = xdev->core.pdev;

	userpf_info(xdev, "INFO IOCTL");

	obj->vendor = pdev->vendor;
	obj->device = pdev->device;
	obj->subsystem_vendor = pdev->subsystem_vendor;
	obj->subsystem_device = pdev->subsystem_device;
	obj->driver_version = XOCL_DRIVER_VERSION_NUMBER;
	obj->pci_slot = PCI_SLOT(pdev->devfn);

	return 0;
}

int xocl_execbuf_ioctl(struct drm_device *dev,
	void *data, struct drm_file *filp)
{
	struct drm_gem_object *obj;
	struct drm_xocl_bo *xobj;
	struct drm_xocl_execbuf *args = data;
	struct xocl_dev *xdev = dev->dev_private;
	struct drm_xocl_bo *deps[8] = {0};
	struct client_ctx *client = filp->driver_priv;
	int numdeps;
	int ret = 0;

	if (atomic_read(&xdev->needs_reset)) {
		userpf_err(xdev, "device needs reset, use 'xbsak reset -h'");
		return -EBUSY;
	}

	if (!MB_SCHEDULER_DEV(xdev)) {
		userpf_err(xdev, "scheduler subdev does not exist");
		return -EINVAL;
	}

	/* If ctx xclbin uuid mismatch or no xclbin uuid then EPERM */
	if (uuid_is_null(&client->xclbin_id) || !uuid_equal(&xdev->xclbin_id,&client->xclbin_id)) {
		userpf_err(xdev, "Invalid xclbin for current process");
		return -EPERM;
	}

	/* Look up the gem object corresponding to the BO handle.
	 * This adds a reference to the gem object.  The refernece is
	 * passed to kds or released here if errors occur.
	 */
	obj =xocl_gem_object_lookup(dev, filp, args->exec_bo_handle);
	if (!obj) {
		userpf_err(xdev, "Failed to look up GEM BO %d\n",
			args->exec_bo_handle);
		return -ENOENT;
	}

	/* Convert gem object to xocl_bo extension */
	xobj =to_xocl_bo(obj);
	if (!xocl_bo_execbuf(xobj)) {
		ret = -EINVAL;
		goto out;
	}

	/* Copy dependencies from user.  It is an error if a BO handle specified
	 * as a dependency does not exists. Lookup gem object corresponding to bo
	 * handle.  Convert gem object to xocl_bo extension.  Note that the
	 * gem lookup acquires a reference to the drm object, this reference
	 * is passed on to the the scheduler via xocl_exec_add_buffer. */
	for (numdeps=0; numdeps<8 && args->deps[numdeps]; ++numdeps) {
		struct drm_gem_object *gobj = xocl_gem_object_lookup(dev,filp,args->deps[numdeps]);
		struct drm_xocl_bo *xbo = gobj ? to_xocl_bo(gobj) : NULL;
		if (!gobj)
			userpf_err(xdev,"Failed to look up GEM BO %d\n",args->deps[numdeps]);
		if (!xbo) {
			ret = -EINVAL;
			goto out;
		}
		deps[numdeps] = xbo;
	}

	/* Add exec buffer to scheduler (kds).  The scheduler manages the
	 * drm object references acquired by xobj and deps.  It is vital
	 * that the references are released properly. */
	ret = xocl_exec_add_buffer(xdev, client, xobj, numdeps, deps);
	if (ret) {
		userpf_err(xdev, "Failed to add exec buffer to scheduler\n");
		ret = -EINVAL;
		goto out;
	}

	/* Return here, noting that the gem objects passed to kds have
	 * references that must be released by kds itself.  User manages
	 * a regular reference to all BOs returned as file handles.  These
	 * references are released with the BOs are freed. */
	return ret;

out:
	drm_gem_object_unreference_unlocked(&xobj->base);
	return ret;
}

int xocl_ctx_ioctl(struct drm_device *dev, void *data,
		   struct drm_file *filp)
{
	struct drm_xocl_ctx *args = data;
	struct xocl_dev *xdev = dev->dev_private;
	struct client_ctx *client = filp->driver_priv;
	int ret = 0;

	mutex_lock(&xdev->ctx_list_lock);
	if (!uuid_equal(&xdev->xclbin_id, &args->xclbin_id)) {
		ret = -EPERM;
		goto out;
	}

	if (args->op == XOCL_CTX_OP_FREE_CTX) {
		bitmap_zero(client->cu_bitmap, MAX_CUS);
		goto out;
	}

	if (args->op != XOCL_CTX_OP_ALLOC_CTX) {
		ret = -EFAULT;
		goto out;
	}

	if (!bitmap_empty(client->cu_bitmap, MAX_CUS)) {
		ret = -EBUSY;
		goto out;
	}

	bitmap_copy(client->cu_bitmap, (const long unsigned int *)args->cu_bitmap, MAX_CUS);
	xocl_info(dev->dev, "CTX ioctl");
out:
	mutex_unlock(&xdev->ctx_list_lock);
	return ret;
}

static irqreturn_t xocl_user_isr(int irq, void *arg)
{
	struct xocl_dev *xdev = (struct xocl_dev *)arg;

	if (!xdev->user_msix_table[irq])
		userpf_err(xdev, "received unregistered user intr");
	if (eventfd_signal(xdev->user_msix_table[irq], 1) == 1)
		return 0;
	else
		userpf_err(xdev, "notify user intr failed");
	return IRQ_HANDLED;
}

int xocl_user_intr_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *filp)
{
	struct xocl_dev *xdev = dev->dev_private;
	struct eventfd_ctx *trigger;
	struct drm_xocl_user_intr *args = data;
	int	ret = 0;

	xocl_info(dev->dev, "USER INTR ioctl");

	if ((args->msix >= xdev->max_user_intr) ||
		(args->msix <  xdev->start_user_intr)) {
		userpf_err(xdev, "Invalid req intr %d, user start %d, max %d",
			args->msix, xdev->start_user_intr,
			xdev->max_user_intr);
		return -EINVAL;
	}

	mutex_lock(&xdev->user_msix_table_lock);
	if (xdev->user_msix_table[args->msix]) {
		ret = -EPERM;
		goto out;
	}

	if (args->fd < 0)
		goto out;
	trigger = eventfd_ctx_fdget(args->fd);
	if (IS_ERR(trigger)) {
		ret = PTR_ERR(trigger);
		goto out;
	}

	/* When will user unregister intr ??
	 * Should we allow user register one intr multiple times?
	 * Leave the logic as is for now
	 */
	xdev->user_msix_table[args->msix] = trigger;
	xocl_user_interrupt_reg(xdev, args->msix, xocl_user_isr, xdev);
	xocl_user_interrupt_config(xdev, args->msix, true);
out:
	mutex_unlock(&xdev->user_msix_table_lock);
	return ret;
}

/* should be obsoleted after mailbox implememted */
static const struct axlf_section_header* get_axlf_section(const struct axlf* top, enum axlf_section_kind kind)
{
	int i = 0;
	printk(KERN_INFO "Trying to find section header for axlf section %d", kind);
	for(i = 0; i < top->m_header.m_numSections; i++)
	{
		printk(KERN_INFO "Section is %d",top->m_sections[i].m_sectionKind);
		if(top->m_sections[i].m_sectionKind == kind) {
			printk(KERN_INFO "Found section header for axlf");
			return &top->m_sections[i];
		}
	}
	printk(KERN_INFO "Did NOT find section header for axlf section %d", kind);
	return NULL;
}

static uint live_client_size(struct xocl_dev *xdev)
{
	const struct list_head *ptr;
	const struct client_ctx *entry;
	uint count = 0;

	BUG_ON(!mutex_is_locked(&xdev->ctx_list_lock));

	list_for_each(ptr, &xdev->ctx_list) {
		entry = list_entry(ptr, struct client_ctx, link);
		//if (!bitmap_empty(entry->cu_bitmap, MAX_CUS))
		count++;
	}
	return count;
}

static int xocl_read_axlf_ioctl_helper(struct xocl_dev *xdev,
				       struct drm_xocl_axlf *axlf_obj_ptr)
{
	long err = 0;
	unsigned i = 0;
	uint64_t copy_buffer_size = 0;
	struct axlf* copy_buffer = 0;
	const struct axlf_section_header *memHeader = 0;
	char __user *buffer = 0;
	int32_t bank_count = 0;
	//short ddr = 0;
	struct axlf bin_obj;
	struct xocl_mem_topology *topology;

	userpf_info(xdev, "READ_AXLF IOCTL \n");

	if(!xocl_is_unified(xdev)) {
		printk(KERN_INFO "XOCL: not unified dsa");
		return err;
	}

	printk(KERN_INFO "XOCL: Marker 0 %p\n", axlf_obj_ptr);
	if (copy_from_user((void *)&bin_obj, (void*)axlf_obj_ptr->xclbin, sizeof(struct axlf)))
		return -EFAULT;
	if (memcmp(bin_obj.m_magic, "xclbin2", 8))
		return -EINVAL;

	if (uuid_is_null(&bin_obj.m_header.uuid)) {
		// Legacy xclbin, convert legacy id to new id
		memcpy(&bin_obj.m_header.uuid, &bin_obj.m_header.m_timeStamp, 8);
	}

	/*
	 * Support for multiple processes
	 * 1. We lock &xdev->ctx_list_lock so no new contexts can be opened and no live contexts
	 *    can be closed
	 * 2. If more than one context exists -- more than one clients are connected -- we cannot
	 *    swap the xclbin return -EPERM
	 * 3. If no live contexts exist there may still be sumbitted exec BOs from a
	 *    previous context (which was subsequently closed), hence we check for exec BO count.
	 *    If exec BO are outstanding we return -EBUSY
	 */
	if (!uuid_equal(&xdev->xclbin_id, &bin_obj.m_header.uuid)) {
		// Check for submitted exec bos for this device that have not been processed
		if (atomic_read(&xdev->outstanding_execs)) {
			err = -EBUSY;
			goto done;
		}
		// Check if there are other open contexts on this device
		if (!list_is_singular(&xdev->ctx_list)) {
			err = -EPERM;
			goto done;
		}
	}

	//Ignore timestamp matching for AWS platform
	if (!xocl_is_aws(xdev) && !xocl_verify_timestamp(xdev,
		bin_obj.m_header.m_featureRomTimeStamp)) {
		printk(KERN_ERR "TimeStamp of ROM did not match Xclbin\n");
		err = -EINVAL;
		goto done;
	}

	printk(KERN_INFO "XOCL: VBNV and TimeStamps matched\n");

	err = xocl_icap_lock_bitstream(xdev, &bin_obj.m_header.uuid,
		pid_nr(task_tgid(current)));
	if (err)
		goto done;

	if(bin_obj.m_uniqueId == xdev->unique_id_last_bitstream) {
		printk(KERN_INFO "Skipping repopulating topology, connectivity,ip_layout data\n");
		goto done;
	}

	//Switching the xclbin, make sure none of the buffers are used.
	err = xocl_check_topology(xdev);
	if(err)
		goto done;

	xocl_cleanup_mem(xdev);

	//Copy from user space and proceed.
	copy_buffer_size = (bin_obj.m_header.m_numSections)*sizeof(struct axlf_section_header) + sizeof(struct axlf);
	copy_buffer = (struct axlf*)vmalloc(copy_buffer_size);
	if(!copy_buffer) {
		printk(KERN_ERR "Unable to create copy_buffer");
		err = -EFAULT;
		goto done;
	}
	printk(KERN_INFO "XOCL: Marker 5\n");

	if (copy_from_user((void *)copy_buffer, (void *)axlf_obj_ptr->xclbin, copy_buffer_size)) {
		err = -EFAULT;
		goto done;
	}

	buffer = (char __user *)axlf_obj_ptr->xclbin;
	err = !access_ok(VERIFY_READ, buffer, bin_obj.m_header.m_length);
	if (err) {
		err = -EFAULT;
		goto done;
	}

	//----
	printk(KERN_INFO "Finding IP_LAYOUT section\n");
	memHeader = get_axlf_section(copy_buffer, IP_LAYOUT);
	if (memHeader == 0) {
		printk(KERN_INFO "Did not find IP_LAYOUT section.\n");
	} else {
		printk(KERN_INFO "%s XOCL: IP_LAYOUT offset = %llx, size = %llx, xclbin length = %llx\n", __FUNCTION__, memHeader->m_sectionOffset , memHeader->m_sectionSize, bin_obj.m_header.m_length);

		if((memHeader->m_sectionOffset + memHeader->m_sectionSize) > bin_obj.m_header.m_length) {
			printk(KERN_INFO "%s XOCL: IP_LAYOUT section extends beyond xclbin boundary %llx\n", __FUNCTION__, bin_obj.m_header.m_length);
			err = -EINVAL;
			goto done;
		}
		printk(KERN_INFO "XOCL: Marker 5.1\n");
		buffer += memHeader->m_sectionOffset;
		xdev->layout.layout = vmalloc(memHeader->m_sectionSize);
		err = copy_from_user(xdev->layout.layout, buffer, memHeader->m_sectionSize);
		printk(KERN_INFO "XOCL: Marker 5.2\n");
		if (err)
			goto done;
		xdev->layout.size = memHeader->m_sectionSize;
		printk(KERN_INFO "XOCL: Marker 5.3\n");
	}

	//----
	printk(KERN_INFO "Finding DEBUG_IP_LAYOUT section\n");
	memHeader = get_axlf_section(copy_buffer, DEBUG_IP_LAYOUT);
	if (memHeader == 0) {
		printk(KERN_INFO "Did not find DEBUG_IP_LAYOUT section.\n");
	} else {
		printk(KERN_INFO "%s XOCL: DEBUG_IP_LAYOUT offset = %llx, size = %llx, xclbin length = %llx\n", __FUNCTION__, memHeader->m_sectionOffset , memHeader->m_sectionSize, bin_obj.m_header.m_length);

		if((memHeader->m_sectionOffset + memHeader->m_sectionSize) > bin_obj.m_header.m_length) {
			printk(KERN_INFO "%s XOCL: DEBUG_IP_LAYOUT section extends beyond xclbin boundary %llx\n", __FUNCTION__, bin_obj.m_header.m_length);
			err = -EINVAL;
			goto done;
		}
		printk(KERN_INFO "XOCL: Marker 6.1\n");
		buffer = (char __user *)axlf_obj_ptr->xclbin;
		buffer += memHeader->m_sectionOffset;
		xdev->debug_layout.layout = vmalloc(memHeader->m_sectionSize);
		err = copy_from_user(xdev->debug_layout.layout, buffer, memHeader->m_sectionSize);
		printk(KERN_INFO "XOCL: Marker 6.2\n");
		if (err)
			goto done;
		xdev->debug_layout.size = memHeader->m_sectionSize;
		printk(KERN_INFO "XOCL: Marker 6.3\n");
	}

	//---
	printk(KERN_INFO "Finding CONNECTIVITY section\n");
	memHeader = get_axlf_section(copy_buffer, CONNECTIVITY);
	if (memHeader == 0) {
		printk(KERN_INFO "Did not find CONNECTIVITY section.\n");
	} else {
		printk(KERN_INFO "%s XOCL: CONNECTIVITY offset = %llx, size = %llx\n", __FUNCTION__, memHeader->m_sectionOffset , memHeader->m_sectionSize);
		if((memHeader->m_sectionOffset + memHeader->m_sectionSize) > bin_obj.m_header.m_length) {
			err = -EINVAL;
			goto done;
		}
		buffer = (char __user *)axlf_obj_ptr->xclbin;
		buffer += memHeader->m_sectionOffset;
		xdev->connectivity.connections = vmalloc(memHeader->m_sectionSize);
		err = copy_from_user(xdev->connectivity.connections, buffer, memHeader->m_sectionSize);
		if (err)
			goto done;
		xdev->connectivity.size = memHeader->m_sectionSize;
	}

	//---
	printk(KERN_INFO "Finding MEM_TOPOLOGY section\n");
	memHeader = get_axlf_section(copy_buffer, MEM_TOPOLOGY);
	if (memHeader == 0) {
		printk(KERN_INFO "Did not find MEM_TOPOLOGY section.\n");
		err = -EINVAL;
		goto done;
	}
	printk(KERN_INFO "XOCL: Marker 7\n");

	printk(KERN_INFO "%s XOCL: MEM_TOPOLOGY offset = %llx, size = %llx\n", __FUNCTION__, memHeader->m_sectionOffset , memHeader->m_sectionSize);

	if((memHeader->m_sectionOffset + memHeader->m_sectionSize) > bin_obj.m_header.m_length) {
		err = -EINVAL;
		goto done;
	}


	printk(KERN_INFO "XOCL: Marker 8\n");

	buffer = (char __user *)axlf_obj_ptr->xclbin;
	buffer += memHeader->m_sectionOffset;

	xdev->topology.topology = vmalloc(memHeader->m_sectionSize);
	err = copy_from_user(xdev->topology.topology, buffer, memHeader->m_sectionSize);
	if (err)
	    goto done;
	xdev->topology.size = memHeader->m_sectionSize;

	get_user(bank_count, buffer);
	xdev->topology.bank_count = bank_count;
	buffer += offsetof(struct mem_topology, m_mem_data);
	xdev->topology.m_data_length = bank_count*sizeof(struct mem_data);
	xdev->topology.m_data = vmalloc(xdev->topology.m_data_length);
	err = copy_from_user(xdev->topology.m_data, buffer, bank_count*sizeof(struct mem_data));
	if (err) {
		err = -EFAULT;
		goto done;
	}


	printk(KERN_INFO "XOCL: Marker 9\n");

	topology = &xdev->topology;

	printk(KERN_INFO "XOCL: Topology count = %d, data_length = %d\n", topology->bank_count, xdev->topology.m_data_length);

	xdev->mm = devm_kzalloc(xdev->ddev->dev, sizeof(struct drm_mm) * topology->bank_count, GFP_KERNEL);
	xdev->mm_usage_stat = devm_kzalloc(xdev->ddev->dev, sizeof(struct drm_xocl_mm_stat) * topology->bank_count, GFP_KERNEL);
	if (!xdev->mm || !xdev->mm_usage_stat) {
		err = -ENOMEM;
		goto done;
	}

	for (i=0; i < topology->bank_count; i++)
	{
		printk(KERN_INFO "XOCL, DDR Info Index: %d Type:%d Used:%d Size:%llx Base_addr:%llx\n", i,
			topology->m_data[i].m_type, topology->m_data[i].m_used, topology->m_data[i].m_size,
			topology->m_data[i].m_base_address);
	}

//	ddr = 0;
//	for (i=0; i < topology->bank_count; i++)
//	{
//		printk(KERN_INFO "XOCL, DDR Info Index: %d Type:%d Used:%d Size:%llx Base_addr:%llx\n", i,
//			topology->m_data[i].m_type, topology->m_data[i].m_used, topology->m_data[i].m_size,
//			topology->m_data[i].m_base_address);
//		if (topology->m_data[i].m_used)
//		{
//			ddr++;
//			if ((topology->bank_size !=0) && (topology->bank_size != topology->m_data[i].m_size)) {
//				//we support only same sized banks for initial testing, so return error.
//				printk(KERN_INFO "%s err: %ld\n", __FUNCTION__, err);
//				err = -EFAULT;
//				vfree(xdev->topology.m_data);
//				memset(&xdev->topology, 0, sizeof(xdev->topology));
//				goto done;
//			}
//			topology->bank_size = topology->m_data[i].m_size;
//		}
//	}

//	//xdev->topology.used_bank_count = ddr;
//	printk(KERN_INFO "XOCL: Unified flow, used bank count :%d bank size(KB):%llx\n", ddr, xdev->topology.bank_size);

	//initialize the used banks and their sizes. Currently only fixed sizes are supported.
	for (i=0; i < topology->bank_count; i++)
	{
		if (topology->m_data[i].m_used) {
			printk(KERN_INFO "%s Allocating DDR:%d with base_addr:%llx, size %llx \n", __FUNCTION__, i,
				topology->m_data[i].m_base_address, topology->m_data[i].m_size*1024);
			drm_mm_init(&xdev->mm[i], topology->m_data[i].m_base_address, topology->m_data[i].m_size*1024);
			printk(KERN_INFO "drm_mm_init called \n");
		}
	}

	//Populate with "this" bitstream, so avoid redownload the next time
	xdev->unique_id_last_bitstream = bin_obj.m_uniqueId;
	uuid_copy(&xdev->xclbin_id, &bin_obj.m_header.uuid);
	userpf_info(xdev, "Loaded xclbin %pUb", &xdev->xclbin_id);

done:
	if (err != 0)
		(void) xocl_icap_unlock_bitstream(xdev, &bin_obj.m_header.uuid,
			pid_nr(task_tgid(current)));
	printk(KERN_INFO "%s err: %ld\n", __FUNCTION__, err);
	vfree(copy_buffer);
	return err;

}

int xocl_read_axlf_ioctl(struct drm_device *dev,
			 void *data,
			 struct drm_file *filp)
{
	struct drm_xocl_axlf *axlf_obj_ptr = data;
	struct xocl_dev *xdev = dev->dev_private;
	struct client_ctx *client = filp->driver_priv;
	int err = 0;

	mutex_lock(&xdev->ctx_list_lock);
	err = xocl_read_axlf_ioctl_helper(xdev, axlf_obj_ptr);
	uuid_copy(&client->xclbin_id, (err ? &uuid_null : &xdev->xclbin_id));
	mutex_unlock(&xdev->ctx_list_lock);
	return err;
}

uint get_live_client_size(struct xocl_dev *xdev) {
	uint count;
	mutex_lock(&xdev->ctx_list_lock);
	count = live_client_size(xdev);
	mutex_unlock(&xdev->ctx_list_lock);
	return count;
}

void reset_notify_client_ctx(struct xocl_dev *xdev)
{
	atomic_set(&xdev->needs_reset,0);
	wmb();
}
