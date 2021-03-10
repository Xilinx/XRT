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

#ifndef _FLASH_XRT_DATA_H_
#define _FLASH_XRT_DATA_H_

#define XRT_DATA_MAGIC	"XRTDATA"

/*
 * This header file contains data structure for xrt meta data on flash. This
 * file is included in user space utilities and kernel drivers. The data
 * structure is used to describe on-flash xrt data which is written by utility
 * and read by driver. Any change of the data structure should either be
 * backward compatible or cause version to be bumped up.
 */

struct flash_data_ident {
	char fdi_magic[7];
	char fdi_version;
};

/*
 * On-flash meta data describing XRT data on flash. Either fdh_id_begin or
 * fdh_id_end should be at well-known location on flash so that the reader
 * can easily pick up fdi_version from flash before it tries to interpret
 * the whole data structure.
 * E.g., you align header in the end of the flash so that fdh_id_end is at well
 * known location or align header at the beginning of the flash so that
 * fdh_id_begin is at well known location.
 */
struct flash_data_header {
	struct flash_data_ident fdh_id_begin;
	uint32_t fdh_data_offset;
	uint32_t fdh_data_len;
	uint32_t fdh_data_parity;
	uint8_t fdh_reserved[16];
	struct flash_data_ident fdh_id_end;
};

static inline uint32_t flash_xrt_data_get_parity32(unsigned char *buf, size_t n)
{
	char *p;
	size_t i;
	size_t len;
	uint32_t parity = 0;

	for (len = 0; len < n; len += 4) {
		uint32_t tmp = 0;
		size_t thislen = n - len;

		/* One word at a time. */
		if (thislen > 4)
			thislen = 4;

		for (i = 0, p = (char *)&tmp; i < thislen; i++)
			p[i] = buf[len + i];
		parity ^= tmp;
	}
	return parity;
}

#endif
