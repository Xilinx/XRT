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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
#include <linux/hashtable.h>
#endif
#include "version.h"
#include "common.h"

#if defined(XOCL_UUID)
xuid_t uuid_null = NULL_UUID_LE;
#endif

static int
xocl_client_lock_bitstream(struct xocl_dev *xdev, struct client_ctx *client)
{
	if (atomic_read(&client->xclbin_locked))
		return 0;

	if (xocl_icap_lock_bitstream(xdev, &xdev->xclbin_id,pid_nr(task_tgid(current)))) {
		userpf_err(xdev,"could not lock bitstream for process %d",pid_nr(task_tgid(current)));
		return 1;
	}

//      Allow second process to use current xclbin without downloading
//	if (uuid_is_null(&client->xclbin_id))
//		uuid_copy(&client->xclbin_id,&xdev->xclbin_id);
//	else
	if (!uuid_equal(&xdev->xclbin_id,&client->xclbin_id)) {
		userpf_err(xdev,"device xclbin does not match context xclbin, cannot obtain lock for process %d",
			   pid_nr(task_tgid(current)));
		goto out;
	}

	atomic_set(&client->xclbin_locked,true);
	userpf_info(xdev,"process %d successfully locked xcblin",pid_nr(task_tgid(current)));
	return 0;

out:
	(void) xocl_icap_unlock_bitstream(xdev, &xdev->xclbin_id,pid_nr(task_tgid(current)));
	return 1;
}


int xocl_info_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct drm_xocl_info *obj = data;
	struct xocl_dev *xdev = dev->dev_private;
	struct pci_dev *pdev = xdev->core.pdev;
	u32 major, minor, patch;

	userpf_info(xdev, "INFO IOCTL");

	sscanf(XRT_DRIVER_VERSION, "%d.%d.%d", &major, &minor, &patch);

	obj->vendor = pdev->vendor;
	obj->device = pdev->device;
	obj->subsystem_vendor = pdev->subsystem_vendor;
	obj->subsystem_device = pdev->subsystem_device;
	obj->driver_version = XOCL_DRV_VER_NUM(major, minor, patch);
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

	/* Look up the gem object corresponding to the BO handle.
	 * This adds a reference to the gem object.  The refernece is
	 * passed to kds or released here if errors occur.
	 */
	obj = xocl_gem_object_lookup(dev, filp, args->exec_bo_handle);
	if (!obj) {
		userpf_err(xdev, "Failed to look up GEM BO %d\n",
			args->exec_bo_handle);
		return -ENOENT;
	}

	/* Convert gem object to xocl_bo extension */
	xobj = to_xocl_bo(obj);
	if (!xocl_bo_execbuf(xobj)) {
		ret = -EINVAL;
		goto out;
	}

	ret = xocl_exec_validate(xdev, client, xobj);
	if (ret) {
		userpf_err(xdev, "Exec buffer validation failed\n");
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

	/* acquire lock on xclbin if necessary */
	ret = xocl_client_lock_bitstream(xdev,client);
	if (ret) {
		userpf_err(xdev, "Failed to lock xclbin\n");
		ret = -EINVAL;
		goto out;
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

/*
 * Create a context (ony shared supported today) on a CU. Take a lock on xclbin if
 * it has not been acquired before. Shared the same lock for all context requests
 * for that process
 */
int xocl_ctx_ioctl(struct drm_device *dev, void *data,
		   struct drm_file *filp)
{
	bool acquire_lock = false;
	struct drm_xocl_ctx *args = data;
	struct xocl_dev *xdev = dev->dev_private;
	struct client_ctx *client = filp->driver_priv;
	int ret = 0;

	mutex_lock(&client->lock);
	mutex_lock(&xdev->ctx_list_lock);
	if (!uuid_equal(&xdev->xclbin_id, &args->xclbin_id)) {
		ret = -EBUSY;
		goto out;
	}

	if (args->cu_index >= xdev->layout->m_count) {
		userpf_err(xdev, "cuidx(%d) >= numcus(%d)\n",args->cu_index,xdev->layout->m_count);
		ret = -EINVAL;
		goto out;
	}

	if (args->op == XOCL_CTX_OP_FREE_CTX) {
		ret = test_and_clear_bit(args->cu_index, client->cu_bitmap) ? 0 : -EINVAL;
		if (ret) // No context was previously allocated for this CU
			goto out;

		// CU unlocked explicitly
		--client->num_cus;
		--xdev->ip_reference[args->cu_index];
		if (!client->num_cus) {
                        // We just gave up the last context, give up the xclbin lock
			ret = xocl_icap_unlock_bitstream(xdev, &xdev->xclbin_id,
							 pid_nr(task_tgid(current)));
		}
		xocl_info(dev->dev, "CTX del(%pUb, %d, %u)", &xdev->xclbin_id, pid_nr(task_tgid(current)),
			  args->cu_index);
		goto out;
	}

	if (args->op != XOCL_CTX_OP_ALLOC_CTX) {
		ret = -EINVAL;
		goto out;
	}

	if ((args->flags != XOCL_CTX_SHARED)) {
		userpf_err(xdev, "Only shared contexts are supported in this release");
		ret = -EPERM;
		goto out;
	}

	if (!client->num_cus && !atomic_read(&client->xclbin_locked))
		// Process has no other context on any CU yet, hence we need to lock the xclbin
		// A process uses just one lock for all its contexts
		acquire_lock = true;

	if (test_and_set_bit(args->cu_index, client->cu_bitmap)) {
		userpf_info(xdev, "Context has already been allocated before by this process");
		// Context was previously allocated for the same CU, cannot allocate again
		ret = 0;
		goto out;
	}

	if (acquire_lock) {
                // This is the first context on any CU for this process, lock the xclbin
		ret = xocl_client_lock_bitstream(xdev, client);
		if (ret) {
			// Locking of xclbin failed, give up our context
			clear_bit(args->cu_index, client->cu_bitmap);
			goto out;
		}
		else {
			uuid_copy(&client->xclbin_id, &xdev->xclbin_id);
		}
	}

	// Everything is good so far, hence increment the CU reference count
	++client->num_cus; // explicitly acquired
	++xdev->ip_reference[args->cu_index];
	xocl_info(dev->dev, "CTX add(%pUb, %d, %u, %d)", &xdev->xclbin_id, pid_nr(task_tgid(current)), args->cu_index,acquire_lock);
out:
	// If all explicit resources are given up, then release the xclbin
	if (!client->num_cus)
		atomic_set(&client->xclbin_locked,false);
	mutex_unlock(&xdev->ctx_list_lock);
	mutex_unlock(&client->lock);
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

char *kind_to_string(enum axlf_section_kind kind)
{
	switch (kind) {
	case 0:  return "BITSTREAM";
	case 1:  return "CLEARING_BITSTREAM";
	case 2:  return "EMBEDDED_METADATA";
	case 3:  return "FIRMWARE";
	case 4:  return "DEBUG_DATA";
	case 5:  return "SCHED_FIRMWARE";
	case 6:  return "MEM_TOPOLOGY";
	case 7:  return "CONNECTIVITY";
	case 8:  return "IP_LAYOUT";
	case 9:  return "DEBUG_IP_LAYOUT";
	case 10: return "DESIGN_CHECK_POINT";
	case 11: return "CLOCK_FREQ_TOPOLOGY";
	default: return "UNKNOWN";
	}
}

/* should be obsoleted after mailbox implememted */
static const struct axlf_section_header *
get_axlf_section(const struct axlf *top, enum axlf_section_kind kind)
{
	int i = 0;

	DRM_INFO("Finding %s section header", kind_to_string(kind));
	for (i = 0; i < top->m_header.m_numSections; i++) {
		if (top->m_sections[i].m_sectionKind == kind)
			return &top->m_sections[i];
	}
	DRM_INFO("Did not find AXLF section %s", kind_to_string(kind));
	return NULL;
}

int
xocl_check_section(const struct axlf_section_header *header, uint64_t len,
		enum axlf_section_kind kind)
{
	uint64_t offset;
	uint64_t size;

	DRM_INFO("Section %s details:", kind_to_string(kind));
	DRM_INFO("  offset = 0x%llx", header->m_sectionOffset);
	DRM_INFO("  size = 0x%llx", header->m_sectionSize);

	offset = header->m_sectionOffset;
	size = header->m_sectionSize;
	if (offset + size <= len)
		return 0;

	DRM_INFO("Section %s extends beyond xclbin boundary 0x%llx\n",
			kind_to_string(kind), len);
	return -EINVAL;
}

/* Return value: Negative for error, or the size in bytes has been copied */
int
xocl_read_sect(enum axlf_section_kind kind, void *sect,
		struct axlf *axlf_full, char __user *xclbin_ptr)
{
	const struct axlf_section_header *memHeader;
	uint64_t xclbin_len;
	uint64_t offset;
	uint64_t size;
	void **sect_tmp = (void *)sect;
	int err = 0;

	memHeader = get_axlf_section(axlf_full, kind);
	if (!memHeader)
		return 0;

	xclbin_len = axlf_full->m_header.m_length;
	err = xocl_check_section(memHeader, xclbin_len, kind);
	if (err)
		return err;

	offset = memHeader->m_sectionOffset;
	size = memHeader->m_sectionSize;
	*sect_tmp = vmalloc(size);
	err = copy_from_user(*sect_tmp, &xclbin_ptr[offset], size);
	if (err) {
		vfree(*sect_tmp);
		sect = NULL;
		return -EINVAL;
	}

	return size;
}

/*
 * Should be called with xdev->ctx_list_lock held
 */
static uint live_client_size(struct xocl_dev *xdev)
{
	const struct list_head *ptr;
	const struct client_ctx *entry;
	uint count = 0;

	BUG_ON(!mutex_is_locked(&xdev->ctx_list_lock));

	list_for_each(ptr, &xdev->ctx_list) {
		entry = list_entry(ptr, struct client_ctx, link);
		count++;
	}
	return count;
}

static int xocl_init_mm(struct xocl_dev *xdev)
{
	size_t length = 0;
	size_t mm_size = 0, mm_stat_size = 0;
	size_t size = 0, wrapper_size = 0;
	size_t ddr_bank_size;
	struct mem_topology *topo;
	struct mem_data *mem_data;
	uint32_t shared;
	struct xocl_mm_wrapper *wrapper;
	uint64_t reserved1 = 0;
	uint64_t reserved2 = 0;
	uint64_t reserved_start;
	uint64_t reserved_end;
	int err = 0;
	int i = 0;

	if (XOCL_DSA_IS_MPSOC(xdev)) {
		/* TODO: This is still hardcoding.. */
		reserved1 = 0x80000000;
		reserved2 = 0x1000000;
	}

	topo = xdev->topology;
	length = topo->m_count * sizeof(struct mem_data);
	size = topo->m_count * sizeof(void *);
	wrapper_size = sizeof(struct xocl_mm_wrapper);
	mm_size = sizeof(struct drm_mm);
	mm_stat_size = sizeof(struct drm_xocl_mm_stat);

	DRM_INFO("XOCL: Topology count = %d, data_length = %ld\n",
			topo->m_count, length);

	xdev->mm = vzalloc(size);
	xdev->mm_usage_stat = vzalloc(size);
	if (!xdev->mm || !xdev->mm_usage_stat)
		return -ENOMEM;

	for (i = 0; i < topo->m_count; i++) {
		mem_data = &topo->m_mem_data[i];
		ddr_bank_size = mem_data->m_size * 1024;

		DRM_INFO("  Mem Index %d", i);
		DRM_INFO("  Base Address:0x%llx\n", mem_data->m_base_address);
		DRM_INFO("  Size:0x%lx", ddr_bank_size);
		DRM_INFO("  Type:%d", mem_data->m_type);
		DRM_INFO("  Used:%d", mem_data->m_used);
	}

	/* Initialize the used banks and their sizes */
	/* Currently only fixed sizes are supported */
	for (i = 0; i < topo->m_count; i++) {
		mem_data = &topo->m_mem_data[i];
		if (!mem_data->m_used)
			continue;

		if (mem_data->m_type == MEM_STREAMING)
			continue;

		ddr_bank_size = mem_data->m_size * 1024;
		DRM_INFO("XOCL: Allocating DDR bank%d", i);
		DRM_INFO("  base_addr:0x%llx, total size:0x%lx\n",
				mem_data->m_base_address,
				ddr_bank_size);

		if (XOCL_DSA_IS_MPSOC(xdev)) {
			reserved_end = mem_data->m_base_address + ddr_bank_size;
			reserved_start = reserved_end - reserved1 - reserved2;
			DRM_INFO("  reserved region:0x%llx - 0x%llx\n",
				 reserved_start, reserved_end - 1);
		}

		shared = xocl_get_shared_ddr(xdev, mem_data);
		if (shared != 0xffffffff) {
			DRM_INFO("Found duplicated memory region!\n");
			xdev->mm[i] = xdev->mm[shared];
			xdev->mm_usage_stat[i] = xdev->mm_usage_stat[shared];
			continue;
		}

		DRM_INFO("Found a new memory region\n");
		wrapper = vzalloc(wrapper_size);
		xdev->mm[i] = vzalloc(mm_size);
		xdev->mm_usage_stat[i] = vzalloc(mm_stat_size);

		if (!xdev->mm[i] || !xdev->mm_usage_stat[i] || !wrapper) {
			err = -ENOMEM;
			goto failed_at_i;
		}

		wrapper->start_addr = mem_data->m_base_address;
		wrapper->size = mem_data->m_size*1024;
		wrapper->mm = xdev->mm[i];
		wrapper->mm_usage_stat = xdev->mm_usage_stat[i];
		wrapper->ddr = i;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
		hash_add(xdev->mm_range, &wrapper->node, wrapper->start_addr);
#endif

		drm_mm_init(xdev->mm[i], mem_data->m_base_address,
				ddr_bank_size - reserved1 - reserved2);

		DRM_INFO("drm_mm_init called\n");
	}

	return 0;

failed_at_i:
	if (wrapper)
		vfree(wrapper);

	for (; i >= 0; i--) {
		drm_mm_takedown(xdev->mm[i]);
		if (xdev->mm[i])
			vfree(xdev->mm[i]);
		if (xdev->mm_usage_stat[i])
			vfree(xdev->mm_usage_stat[i]);
	}
	return err;
}

static int
xocl_read_axlf_helper(struct xocl_dev *xdev, struct drm_xocl_axlf *axlf_ptr)
{
	long err = 0;
	uint64_t axlf_size = 0;
	struct axlf *axlf = 0;
	char __user *buf = 0;
	struct axlf bin_obj;
	size_t size_of_header;
	size_t num_of_sections;
	size_t size;
	int preserve_mem = 0;
	struct mem_topology *new_topology;

	userpf_info(xdev, "READ_AXLF IOCTL\n");

	if(!xocl_is_unified(xdev)) {
		printk(KERN_INFO "XOCL: not unified dsa");
		return err;
	}

	if (copy_from_user(&bin_obj, axlf_ptr->xclbin, sizeof(struct axlf)))
		return -EFAULT;

	if (memcmp(bin_obj.m_magic, "xclbin2", 8))
		return -EINVAL;

	if (xocl_xrt_version_check(xdev, &bin_obj, true))
		return -EINVAL;

	if (uuid_is_null(&bin_obj.m_header.uuid)) {
		// Legacy xclbin, convert legacy id to new id
		memcpy(&bin_obj.m_header.uuid, &bin_obj.m_header.m_timeStamp, 8);
	}

	userpf_info(xdev, "%s:%d\n", __FILE__, __LINE__);
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
	}

	userpf_info(xdev, "%s:%d\n", __FILE__, __LINE__);
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

	if (uuid_equal(&xdev->xclbin_id, &bin_obj.m_header.uuid)) {
		printk(KERN_INFO "Skipping repopulating topology, connectivity,ip_layout data\n");
		goto done;
	}


	//Copy from user space and proceed.
	size_of_header = sizeof(struct axlf_section_header);
	num_of_sections = bin_obj.m_header.m_numSections;
	axlf_size = sizeof(struct axlf) + size_of_header * num_of_sections;
	axlf = vmalloc(axlf_size);
	if (!axlf) {
		DRM_ERROR("Unable to create axlf\n");
		err = -ENOMEM;
		goto done;
	}
	printk(KERN_INFO "XOCL: Marker 5\n");

	if (copy_from_user(axlf, axlf_ptr->xclbin, axlf_size)) {
		err = -EFAULT;
		goto done;
	}

	buf = (char __user *)axlf_ptr->xclbin;
	err = !access_ok(VERIFY_READ, buf, bin_obj.m_header.m_length);
	if (err) {
		err = -EFAULT;
		goto done;
	}

	/* Populating MEM_TOPOLOGY sections. */
	size = xocl_read_sect(MEM_TOPOLOGY, &new_topology, axlf, buf);
	if (size <= 0) {
		if (size != 0)
			goto done;
	} else if (sizeof_sect(new_topology, m_mem_data) != size) {
		err = -EINVAL;
		goto done;
	}

	/* Compare MEM_TOPOLOGY previous vs new. Ignore this and keep disable preserve_mem if not for aws.*/
	if (xocl_is_aws(xdev) && (xdev->topology != NULL)) {
		if ( (size == sizeof_sect(xdev->topology, m_mem_data)) &&
		    !memcmp(new_topology, xdev->topology, size) ) {
			printk(KERN_INFO "XOCL: MEM_TOPOLOGY match, preserve mem_topology.\n");
			preserve_mem = 1;
		} else {
			printk(KERN_INFO "XOCL: MEM_TOPOLOGY mismatch, do not preserve mem_topology.\n");
		}
	}

	/* Switching the xclbin, make sure none of the buffers are used. */
	if (!preserve_mem) {
		err = xocl_check_topology(xdev);
		if(err)
			goto done;
		xocl_cleanup_mem(xdev);
	}
	xocl_cleanup_connectivity(xdev);

	/* Copy MEM_TOPOLOGY from new_toplogy if not preserving memory. */
	if (!preserve_mem)
		xdev->topology = new_topology;
	else
		vfree(new_topology);

	/* Populating IP_LAYOUT sections */
	/* zocl_read_sect return size of section when successfully find it */
	size = xocl_read_sect(IP_LAYOUT, &xdev->layout, axlf, buf);
	if (size <= 0) {
		if (size != 0)
			goto done;
	} else if (sizeof_sect(xdev->layout, m_ip_data) != size) {
		err = -EINVAL;
		goto done;
	}

	/* Populating DEBUG_IP_LAYOUT sections */
	size = xocl_read_sect(DEBUG_IP_LAYOUT, &xdev->debug_layout, axlf, buf);
	if (size <= 0) {
		if (size != 0)
			goto done;
	} else if (sizeof_sect(xdev->debug_layout, m_debug_ip_data) != size) {
		err = -EINVAL;
		goto done;
	}

	/* Populating CONNECTIVITY sections */
	size = xocl_read_sect(CONNECTIVITY, &xdev->connectivity, axlf, buf);
	if (size <= 0) {
		if (size != 0)
			goto done;
	} else if (sizeof_sect(xdev->connectivity, m_connection) != size) {
		err = -EINVAL;
		goto done;
	}

	if (!preserve_mem) {
		err = xocl_init_mm(xdev);
		if (err)
			goto done;
	}

	//Populate with "this" bitstream, so avoid redownload the next time
	uuid_copy(&xdev->xclbin_id, &bin_obj.m_header.uuid);
	userpf_info(xdev, "Loaded xclbin %pUb", &xdev->xclbin_id);

	err = xocl_icap_parse_axlf_section(xdev, buf, IP_LAYOUT);
	if(err)
		goto done;
	err = xocl_icap_parse_axlf_section(xdev, buf, MEM_TOPOLOGY);
	if(err)
		goto done;
	err = xocl_icap_parse_axlf_section(xdev, buf, CONNECTIVITY);
	if(err)
		goto done;
	err = xocl_icap_parse_axlf_section(xdev, buf, DEBUG_IP_LAYOUT);
	if(err)
		goto done;

done:
	if (size < 0)
		err = size;
	/*
	 * Always give up ownership for multi process use case; the real locking
	 * is done by context creation API or by execbuf
	 */
	(void) xocl_icap_unlock_bitstream(xdev, &bin_obj.m_header.uuid,
					  pid_nr(task_tgid(current)));
	printk(KERN_INFO "%s err: %ld\n", __FUNCTION__, err);
	vfree(axlf);
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
	err = xocl_read_axlf_helper(xdev, axlf_obj_ptr);
	/*
	 * Record that user land configured this context for current device xclbin
	 * It doesn't mean that the context has a lock on the xclbin, only that
	 * when a lock is eventually acquired it can be verified to be against to
	 * be a lock on expected xclbin
	 */
	uuid_copy(&client->xclbin_id, (err ? &uuid_null : &xdev->xclbin_id));
	//uuid_copy(&client->xclbin_id, &uuid_null);
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
