/**
 *  Copyright (C) 2015-2018, Xilinx Inc
 *
 *  This file is dual licensed.  It may be redistributed and/or modified
 *  under the terms of the Apache 2.0 License OR version 2 of the GNU
 *  General Public License.
 *
 *  Apache License Verbiage
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  GPL license Verbiage:
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef _XCLBIN_H_
#define _XCLBIN_H_

#if defined(__KERNEL__)
#include <linux/types.h>
#include <linux/uuid.h>
#include <linux/version.h>
#elif defined(__cplusplus)
#include <cstdlib>
#include <cstdint>
#include <algorithm>
#include <uuid/uuid.h>
#else
#include <stdlib.h>
#include <stdint.h>
#include <uuid/uuid.h>
#endif

#if !defined(__KERNEL__)
typedef uuid_t xuid_t;
#else //(__KERNEL__)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
typedef uuid_t xuid_t;
#elif defined(RHEL_RELEASE_CODE)
#if RHEL_RELEASE_CODE > RHEL_RELEASE_VERSION(7,4)
typedef uuid_t xuid_t;
#else
typedef uuid_le xuid_t;
#endif
#else
typedef uuid_le xuid_t;
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * Container format for Xilinx bitstreams, metadata and other
     * binary blobs.
     * Every segment must be aligned at 8 byte boundary with null byte padding
     * between adjacent segments if required.
     * For segements which are not present both offset and length must be 0 in
     * the header.
     * Currently only xclbin0\0 is recognized as file magic. In future if/when file
     * format is updated the magic string will be changed to xclbin1\0 and so on.
     */
    enum XCLBIN_MODE {
        XCLBIN_FLAT,
        XCLBIN_PR,
        XCLBIN_TANDEM_STAGE2,
        XCLBIN_TANDEM_STAGE2_WITH_PR,
        XCLBIN_HW_EMU,
        XCLBIN_SW_EMU,
        XCLBIN_MODE_MAX
    };

    /*
     *  AXLF LAYOUT
     *  -----------
     *
     *  -----------------------------------------
     *  | Magic                                 |
     *  -----------------------------------------
     *  | Header                                |
     *  -----------------------------------------
     *  | One or more section headers           |
     *  -----------------------------------------
     *  | Matching number of sections with data |
     *  -----------------------------------------
     *
     */

    enum axlf_section_kind {
        BITSTREAM = 0,
        CLEARING_BITSTREAM,
        EMBEDDED_METADATA,
        FIRMWARE,
        DEBUG_DATA,
        SCHED_FIRMWARE,
        MEM_TOPOLOGY,
        CONNECTIVITY,
        IP_LAYOUT,
        DEBUG_IP_LAYOUT,
        DESIGN_CHECK_POINT,
        CLOCK_FREQ_TOPOLOGY,
        MCS,
        BMC,
        BUILD_METADATA,
        KEYVALUE_METADATA,
        USER_METADATA,
        DNA_CERTIFICATE,
        PDI 
    };

    enum MEM_TYPE {
        MEM_DDR3,
        MEM_DDR4,
        MEM_DRAM,
        MEM_STREAMING,
        MEM_PREALLOCATED_GLOB,
        MEM_ARE, //Aurora
        MEM_HBM,
        MEM_BRAM,
        MEM_URAM,
        MEM_STREAMING_CONNECTION
    };

    enum IP_TYPE {
        IP_MB = 0,
        IP_KERNEL, //kernel instance
        IP_DNASC,
        IP_DDR4_CONTROLLER
    };

    struct axlf_section_header {
        uint32_t m_sectionKind;             /* Section type */
        char m_sectionName[16];             /* Examples: "stage2", "clear1", "clear2", "ocl1", "ocl2, "ublaze", "sched" */
        uint64_t m_sectionOffset;           /* File offset of section data */
        uint64_t m_sectionSize;             /* Size of section data */
    };

    struct axlf_header {
        uint64_t m_length;                  /* Total size of the xclbin file */
        uint64_t m_timeStamp;               /* Number of seconds since epoch when xclbin was created */
        uint64_t m_featureRomTimeStamp;     /* TimeSinceEpoch of the featureRom */
        uint16_t m_versionPatch;            /* Patch Version */
        uint8_t m_versionMajor;             /* Major Version - Version: 2.1.0*/
        uint8_t m_versionMinor;             /* Minor Version */
        uint32_t m_mode;                    /* XCLBIN_MODE */
	union {
	    struct {
		uint64_t m_platformId;      /* 64 bit platform ID: vendor-device-subvendor-subdev */
		uint64_t m_featureId;       /* 64 bit feature id */
	    } rom;
	    unsigned char rom_uuid[16];     /* feature ROM UUID for which this xclbin was generated */
	};
        unsigned char m_platformVBNV[64];   /* e.g. xilinx:xil-accel-rd-ku115:4ddr-xpr:3.4: null terminated */
	union {
	    char m_next_axlf[16];           /* Name of next xclbin file in the daisy chain */
	    xuid_t uuid;                    /* uuid of this xclbin*/
	};
        char m_debug_bin[16];               /* Name of binary with debug information */
        uint32_t m_numSections;             /* Number of section headers */
    };

    struct axlf {
        char m_magic[8];                            /* Should be "xclbin2\0"  */
        unsigned char m_cipher[32];                 /* Hmac output digest */
        unsigned char m_keyBlock[256];              /* Signature for validation of binary */
        uint64_t m_uniqueId;                        /* axlf's uniqueId, use it to skip redownload etc */
        struct axlf_header m_header;                /* Inline header */
        struct axlf_section_header m_sections[1];   /* One or more section headers follow */
    };

    typedef struct axlf xclBin;

    /**** BEGIN : Xilinx internal section *****/

    /* bitstream information */
    struct xlnx_bitstream {
        uint8_t m_freq[8];
        char bits[1];
    };

    /****   MEMORY TOPOLOGY SECTION ****/
    struct mem_data {
	uint8_t m_type; //enum corresponding to mem_type.
	uint8_t m_used; //if 0 this bank is not present
	union {
	    uint64_t m_size; //if mem_type DDR, then size in KB;
	    uint64_t route_id; //if streaming then "route_id"
	};
	union {
	    uint64_t m_base_address;//if DDR then the base address;
	    uint64_t flow_id; //if streaming then "flow id"
	};
	unsigned char m_tag[16]; //DDR: BANK0,1,2,3, has to be null terminated; if streaming then stream0, 1 etc
    };

    struct mem_topology {
        int32_t m_count; //Number of mem_data
        struct mem_data m_mem_data[1]; //Should be sorted on mem_type
    };

    /****   CONNECTIVITY SECTION ****/
    /* Connectivity of each argument of Kernel. It will be in terms of argument
     * index associated. For associating kernel instances with arguments and
     * banks, start at the connectivity section. Using the m_ip_layout_index
     * access the ip_data.m_name. Now we can associate this kernel instance
     * with its original kernel name and get the connectivity as well. This
     * enables us to form related groups of kernel instances.
     */

    struct connection {
        int32_t arg_index; //From 0 to n, may not be contiguous as scalars skipped
        int32_t m_ip_layout_index; //index into the ip_layout section. ip_layout.m_ip_data[index].m_type == IP_KERNEL
        int32_t mem_data_index; //index of the m_mem_data . Flag error is m_used false.
    };

    struct connectivity {
        int32_t m_count;
        struct connection m_connection[1];
    };


    /****   IP_LAYOUT SECTION ****/
    /* IPs on AXI lite - their types, names, and base addresses.*/
    struct ip_data {
        uint32_t m_type; //map to IP_TYPE enum
        uint32_t properties; //32 bits to indicate ip specific property. eg if m_type == IP_KERNEL then bit 0 is for interrupt.
        uint64_t m_base_address;
        uint8_t m_name[64]; //eg Kernel name corresponding to KERNEL instance, can embed CU name in future.
    };

    struct ip_layout {
        int32_t m_count;
        struct ip_data m_ip_data[1]; //All the ip_data needs to be sorted by m_base_address.
    };

    /*** Debug IP section layout ****/
    enum DEBUG_IP_TYPE {
        UNDEFINED = 0,
        LAPC,
        ILA,
        AXI_MM_MONITOR,
        AXI_TRACE_FUNNEL,
        AXI_MONITOR_FIFO_LITE,
        AXI_MONITOR_FIFO_FULL,
        ACCEL_MONITOR,
        AXI_STREAM_MONITOR
    };

    struct debug_ip_data {
        uint8_t m_type; // type of enum DEBUG_IP_TYPE
        uint8_t m_index;
        uint8_t m_properties;
        uint8_t m_major;
        uint8_t m_minor;
        uint8_t m_reserved[3];
        uint64_t m_base_address;
        uint8_t m_name[128];
    };

    struct debug_ip_layout {
        uint16_t m_count;
        struct debug_ip_data m_debug_ip_data[1];
    };

    enum CLOCK_TYPE {                      /* Supported clock frequency types */
        CT_UNUSED = 0,                     /* Initialized value */
        CT_DATA   = 1,                     /* Data clock */
        CT_KERNEL = 2,                     /* Kernel clock */
        CT_SYSTEM = 3                      /* System Clock */
    };

    struct clock_freq {                    /* Clock Frequency Entry */
        u_int16_t m_freq_Mhz;              /* Frequency in MHz */
        u_int8_t m_type;                   /* Clock type (enum CLOCK_TYPE) */
        u_int8_t m_unused[5];              /* Not used - padding */
        char m_name[128];                  /* Clock Name */
    };

    struct clock_freq_topology {           /* Clock frequency section */
        int16_t m_count;                   /* Number of entries */
        struct clock_freq m_clock_freq[1]; /* Clock array */
    };

    enum MCS_TYPE {                        /* Supported MCS file types */
        MCS_UNKNOWN = 0,                   /* Initialized value */
        MCS_PRIMARY = 1,                   /* The primary mcs file data */
        MCS_SECONDARY = 2,                 /* The secondary mcs file data */
    };

    struct mcs_chunk {                     /* One chunk of MCS data */
        uint8_t m_type;                    /* MCS data type */
        uint8_t m_unused[7];               /* padding */
        uint64_t m_offset;                 /* data offset from the start of the section */
        uint64_t m_size;                   /* data size */
    };

    struct mcs {                           /* MCS data section */
        int8_t m_count;                    /* Number of chunks */
        int8_t m_unused[7];                /* padding */
        struct mcs_chunk m_chunk[1];       /* MCS chunks followed by data */
    };

    struct bmc {                           /* bmc data section  */
        uint64_t m_offset;                 /* data offset from the start of the section */
        uint64_t m_size;                   /* data size (bytes)*/
        char m_image_name[64];             /* Name of the image (e.g., MSP432P401R) */
        char m_device_name[64];            /* Device ID         (e.g., VCU1525)  */
        char m_version[64];
        char m_md5value[33];               /* MD5 Expected Value(e.g., 56027182079c0bd621761b7dab5a27ca)*/
        char m_padding[7];                 /* Padding */
    };

    enum CHECKSUM_TYPE
    {
        CST_UNKNOWN = 0,
        CST_SDBM = 1,
        CST_LAST
    };

    /**** END : Xilinx internal section *****/

# ifdef __cplusplus
    namespace xclbin {
      inline const axlf_section_header*
      get_axlf_section(const axlf* top, axlf_section_kind kind)
      {
        auto begin = top->m_sections;
        auto end = begin + top->m_header.m_numSections;
        auto itr = std::find_if(begin,end,[kind](const axlf_section_header& sec) { return sec.m_sectionKind==kind; });
        return (itr!=end) ? &(*itr) : nullptr;
      }
    }
# endif

#ifdef __cplusplus
}
#endif

#endif
