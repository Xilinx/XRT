/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2016 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Sonal Santan <sonal.santan@xilinx.com>
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

#include <linux/fpga/fpga-mgr.h>
#include "xclbin.h"
#include "zoclsvm_drv.h"

static int bitstream_parse_header(const unsigned char *Data, unsigned int Size, struct XHwIcap_Bit_Header *Header)
{
	unsigned int I;
	unsigned int Len;
	unsigned int Tmp;
	unsigned int Index;

	/* Start Index at start of bitstream */
	Index = 0;

	/* Initialize HeaderLength.  If header returned early inidicates
	 * failure.
	 */
	Header->HeaderLength = XHI_BIT_HEADER_FAILURE;

	/* Get "Magic" length */
	Header->MagicLength = Data[Index++];
	Header->MagicLength = (Header->MagicLength << 8) | Data[Index++];

	/* Read in "magic" */
	for (I = 0; I < Header->MagicLength - 1; I++) {
		Tmp = Data[Index++];
		if (I%2 == 0 && Tmp != XHI_EVEN_MAGIC_BYTE)
			return -1;   /* INVALID_FILE_HEADER_ERROR */

		if (I%2 == 1 && Tmp != XHI_ODD_MAGIC_BYTE)
			return -1;   /* INVALID_FILE_HEADER_ERROR */

	}

	/* Read null end of magic data. */
	Tmp = Data[Index++];

	/* Read 0x01 (short) */
	Tmp = Data[Index++];
	Tmp = (Tmp << 8) | Data[Index++];

	/* Check the "0x01" half word */
	if (Tmp != 0x01)
		return -1;	 /* INVALID_FILE_HEADER_ERROR */



	/* Read 'a' */
	Tmp = Data[Index++];
	if (Tmp != 'a')
		return -1;	  /* INVALID_FILE_HEADER_ERROR	*/


	/* Get Design Name length */
	Len = Data[Index++];
	Len = (Len << 8) | Data[Index++];

	/* allocate space for design name and final null character. */
	Header->DesignName = kmalloc(Len, GFP_KERNEL);

	/* Read in Design Name */
	for (I = 0; I < Len; I++)
		Header->DesignName[I] = Data[Index++];


	if (Header->DesignName[Len-1] != '\0')
		return -1;

	/* Read 'b' */
	Tmp = Data[Index++];
	if (Tmp != 'b')
		return -1;	/* INVALID_FILE_HEADER_ERROR */


	/* Get Part Name length */
	Len = Data[Index++];
	Len = (Len << 8) | Data[Index++];

	/* allocate space for part name and final null character. */
	Header->PartName = kmalloc(Len, GFP_KERNEL);

	/* Read in part name */
	for (I = 0; I < Len; I++)
		Header->PartName[I] = Data[Index++];

	if (Header->PartName[Len-1] != '\0')
		return -1;

	/* Read 'c' */
	Tmp = Data[Index++];
	if (Tmp != 'c')
		return -1;	/* INVALID_FILE_HEADER_ERROR */


	/* Get date length */
	Len = Data[Index++];
	Len = (Len << 8) | Data[Index++];

	/* allocate space for date and final null character. */
	Header->Date = kmalloc(Len, GFP_KERNEL);

	/* Read in date name */
	for (I = 0; I < Len; I++)
		Header->Date[I] = Data[Index++];

	if (Header->Date[Len - 1] != '\0')
		return -1;

	/* Read 'd' */
	Tmp = Data[Index++];
	if (Tmp != 'd')
		return -1;	/* INVALID_FILE_HEADER_ERROR  */

	/* Get time length */
	Len = Data[Index++];
	Len = (Len << 8) | Data[Index++];

	/* allocate space for time and final null character. */
	Header->Time = kmalloc(Len, GFP_KERNEL);

	/* Read in time name */
	for (I = 0; I < Len; I++)
		Header->Time[I] = Data[Index++];

	if (Header->Time[Len - 1] != '\0')
		return -1;

	/* Read 'e' */
	Tmp = Data[Index++];
	if (Tmp != 'e')
		return -1;	/* INVALID_FILE_HEADER_ERROR */

	/* Get byte length of bitstream */
	Header->BitstreamLength = Data[Index++];
	Header->BitstreamLength = (Header->BitstreamLength << 8) | Data[Index++];
	Header->BitstreamLength = (Header->BitstreamLength << 8) | Data[Index++];
	Header->BitstreamLength = (Header->BitstreamLength << 8) | Data[Index++];
	Header->HeaderLength = Index;

	DRM_INFO("Design \"%s\": Part \"%s\": Timestamp \"%s %s\": Raw data size 0x%x\n",
                 Header->DesignName, Header->PartName, Header->Time,
                 Header->Date, Header->BitstreamLength);

	return 0;
}

static int zoclsvm_pcap_download(struct drm_zoclsvm_dev *zdev, const void __user *bit_buf, unsigned long length)
{
        int err;
        struct XHwIcap_Bit_Header bit_header;
        char *buffer = NULL;
        char *data = NULL;

        memset(&bit_header, 0, sizeof(bit_header));
        buffer = kmalloc(DMA_HWICAP_BITFILE_BUFFER_SIZE, GFP_KERNEL);

	if (!buffer) {
		err = -ENOMEM;
		goto free_buffers;
	}

        if (copy_from_user(buffer, bit_buf, DMA_HWICAP_BITFILE_BUFFER_SIZE)) {
                err = -EFAULT;
		goto free_buffers;
	}

	if (bitstream_parse_header(buffer, DMA_HWICAP_BITFILE_BUFFER_SIZE, &bit_header)) {
		err = -EINVAL;
		goto free_buffers;
	}

	if ((bit_header.HeaderLength + bit_header.BitstreamLength) > length) {
		err = -EINVAL;
		goto free_buffers;
	}

	bit_buf += bit_header.HeaderLength;

        data = vmalloc(bit_header.BitstreamLength);
	if (!data) {
		err = -ENOMEM;
		goto free_buffers;
	}

        if (copy_from_user(data, bit_buf, bit_header.BitstreamLength)) {
                err = -EFAULT;
		goto free_buffers;
	}

        err = fpga_mgr_buf_load(zdev->fpga_mgr, 0, data, bit_header.BitstreamLength);

free_buffers:
        kfree(buffer);
	kfree(bit_header.DesignName);
	kfree(bit_header.PartName);
	kfree(bit_header.Date);
	kfree(bit_header.Time);
        vfree(data);
	return err;
}

int zoclsvm_pcap_download_ioctl(struct drm_device *dev, void *data,
				struct drm_file *filp)
{
        struct xclBin bin_obj;
        char __user *buffer;
        struct drm_zoclsvm_dev *zdev = dev->dev_private;
        const struct drm_zocl_pcap_download *args = data;

        if (copy_from_user((void *)&bin_obj, (void *)args->xclbin, sizeof(struct xclBin)))
		return -EFAULT;
	if (memcmp(bin_obj.m_magic, "xclbin0", 8))
		return -EINVAL;

	if ((bin_obj.m_primaryFirmwareOffset + bin_obj.m_primaryFirmwareLength) > bin_obj.m_length)
		return -EINVAL;

        if (bin_obj.m_secondaryFirmwareLength)
		return -EINVAL;

        buffer = (char __user *)args->xclbin;

	if (!access_ok(VERIFY_READ, buffer, bin_obj.m_length))
		return -EFAULT;

	buffer += bin_obj.m_primaryFirmwareOffset;

        return zoclsvm_pcap_download(zdev, buffer, bin_obj.m_primaryFirmwareLength);
}

int zoclsvm_read_axlf_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	return 0;
}
