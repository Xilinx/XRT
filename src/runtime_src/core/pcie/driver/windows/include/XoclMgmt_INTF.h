/*
 *  Copyright (C) 2018-2021, Xilinx Inc.  All rights reserved.
 *
 *  Author(s):
 *  Arpit Patel <arpitp@xilinx.com>
 *  Michael Preston <mipres@microsoft.com>
 *  Jeff Baxter <jeffb@microsoft.com>
 *  Code with base from XRT Linux Management Driver
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
 */
#pragma once
#include "xclfeatures.h"
#include "xclbin.h"
#define XCLMGMT_NUM_SUPPORTED_CLOCKS    4

//
// Xilinx driver interface GUID
//
DEFINE_GUID(GUID_XILINX_PF_INTERFACE,
    0xd5bf220b, 0xf9c4, 0x415d, 0xbf, 0xac, 0x8, 0x6e, 0xbd, 0x65, 0x3f, 0x8f);

#define XCLMGMT_REG_NAME               L"Xclmgmt"
#define XCLMGMT_SERIAL_NUMBER_REG_NAME L"SerialNumber"

//
// Xilinx driver IOCTL definitions
//
enum XCLMGMT_IOC_TYPES {
    XCLMGMT_IOC_INFO,
    XCLMGMT_IOC_FREQ_SCALE,
    XCLMGMT_IOC_OCL_RESET,
    XCLMGMT_IOC_HOT_RESET,
    XCLMGMT_IOC_REBOOT,
    XCLMGMT_IOC_ICAP_DOWNLOAD_AXLF,
    XCLMGMT_IOC_ERR_INFO,
    XCLMGMT_IOC_GET_BAR_ADDR,
    XCLMGMT_IOC_GET_DEVICE_INFO,
    XCLMGMT_IOC_SET_VLAN_INFO,
    XCLMGMT_IOC_GET_QSPI_INFO,
    XCLMGMT_IOC_PRP_ICAP_PROGRAM_AXLF,
    XCLMGMT_IOC_PRP_ICAP_PROGRAM_AXLF_STATUS,
    XCLMGMT_IOC_GET_UUID_INFO,
    XCLMGMT_IOC_SET_DATA_RETENTION,
    XCLMGMT_IOC_GET_DATA_RETENTION,
    XCLMGMT_IOC_PRP_FORCE_ICAP_PROGRAM_AXLF,
    XCLMGMT_IOC_GET_DEVICE_PCI_INFO,
    XCLMGMT_IOC_MAX
};

/* IOC_INFO takes struct xclmgmt_ioc_device_info */
#define XCLMGMT_OID_GET_IOC_DEVICE_INFO CTL_CODE(FILE_DEVICE_UNKNOWN, XCLMGMT_IOC_INFO, METHOD_BUFFERED, FILE_ANY_ACCESS)
/* IOC_ICAPDOWNLOAD_AXLF provides  struct xclmgmt_ioc_bitstream_axlf as input */
#define XCLMGMT_OID_ICAPDOWNLOAD_AXLF   CTL_CODE(FILE_DEVICE_UNKNOWN, XCLMGMT_IOC_ICAP_DOWNLOAD_AXLF, METHOD_BUFFERED, FILE_ANY_ACCESS)
/* IOC_FREQSCALE provides struct xclmgmt_ioc_freqscaling as input */
#define XCLMGMT_OID_FREQSCALE           CTL_CODE(FILE_DEVICE_UNKNOWN, XCLMGMT_IOC_FREQ_SCALE, METHOD_BUFFERED, FILE_ANY_ACCESS)
/* IOC_HOTRESET is meant to issue a HOT Reset command to the device - Note we are only going to issue Secondary Bus Reset */
#define XCLMGMT_OID_HOTRESET            CTL_CODE(FILE_DEVICE_UNKNOWN, XCLMGMT_IOC_HOT_RESET, METHOD_BUFFERED, FILE_ANY_ACCESS)
/* IOC_OCLRESET is meant to issue OCL Reset */
#define XCLMGMT_OID_OCLRESET            CTL_CODE(FILE_DEVICE_UNKNOWN, XCLMGMT_IOC_OCL_RESET, METHOD_BUFFERED, FILE_ANY_ACCESS)
/* IOC_REBOOT is meant to issue a IOC Reboot - which is Fundamental Reset of the PCIe */
/* #define XCLMGMT_OID_REBOOT              CTL_CODE(FILE_DEVICE_UNKNOWN, XCLMGMT_IOC_REBOOT, METHOD_DIRECTED, FILE_ANY_ACCESS)*/
/* IOC_ERRINFO provides the Error Info and gets struct xclErrorStatus as output */
#define XCLMGMT_OID_ERRINFO             CTL_CODE(FILE_DEVICE_UNKNOWN, XCLMGMT_IOC_ERR_INFO, METHOD_BUFFERED, FILE_ANY_ACCESS)
/* IOC_GET_BAR_ADDR gets the device BAR address mapped into user mode */
#define XCLMGMT_OID_GET_BAR_ADDR        CTL_CODE(FILE_DEVICE_UNKNOWN, XCLMGMT_IOC_GET_BAR_ADDR, METHOD_BUFFERED, FILE_ANY_ACCESS)
/* IOC_GET_DEVICE_INFO gets the device-specific info */
#define XCLMGMT_OID_GET_DEVICE_INFO     CTL_CODE(FILE_DEVICE_UNKNOWN, XCLMGMT_IOC_GET_DEVICE_INFO, METHOD_BUFFERED, FILE_ANY_ACCESS)
/* IOC_SET_VLAN_INFO sets VLAN info for the device*/
#define XCLMGMT_OID_SET_VLAN_INFO       CTL_CODE(FILE_DEVICE_UNKNOWN, XCLMGMT_IOC_SET_VLAN_INFO, METHOD_BUFFERED, FILE_ANY_ACCESS)
/* IOC_GET_QSPI_INFO gets the start address of Flash Controller */
#define XCLMGMT_OID_GET_QSPI_INFO       CTL_CODE(FILE_DEVICE_UNKNOWN, XCLMGMT_IOC_GET_QSPI_INFO, METHOD_BUFFERED, FILE_ANY_ACCESS)
/* IOC_PRP_ICAP_PROGRAM_AXLF provides  struct xclmgmt_ioc_bitstream_axlf as input and program PRP region */
#define XCLMGMT_OID_PRP_ICAP_PROGRAM_AXLF CTL_CODE(FILE_DEVICE_UNKNOWN, XCLMGMT_IOC_PRP_ICAP_PROGRAM_AXLF, METHOD_BUFFERED, FILE_ANY_ACCESS)
/* IOC_PRP_ICAP_PROGRAM_AXLF provides   returns PLP program status  */
#define XCLMGMT_IOC_PRP_ICAP_PROGRAM_AXLF_STATUS CTL_CODE(FILE_DEVICE_UNKNOWN, XCLMGMT_IOC_PRP_ICAP_PROGRAM_AXLF_STATUS, METHOD_BUFFERED, FILE_ANY_ACCESS)
/* Provides Information about UUID in case of 2RP */
#define XCLMGMT_OID_GET_UUID_INFO CTL_CODE(FILE_DEVICE_UNKNOWN, XCLMGMT_IOC_GET_UUID_INFO, METHOD_BUFFERED, FILE_ANY_ACCESS)
/*Set data retention value*/
#define XCLMGMT_OID_SET_DATA_RETENTION CTL_CODE(FILE_DEVICE_UNKNOWN, XCLMGMT_IOC_SET_DATA_RETENTION, METHOD_BUFFERED, FILE_ANY_ACCESS)
/*Get data retention value*/
#define XCLMGMT_OID_GET_DATA_RETENTION CTL_CODE(FILE_DEVICE_UNKNOWN, XCLMGMT_IOC_GET_DATA_RETENTION, METHOD_BUFFERED, FILE_ANY_ACCESS)
/* IOC_PRP_FORCE_ICAP_PROGRAM_AXLF provides  struct xclmgmt_ioc_bitstream_axlf as input, and plp download status as output, force program PRP region */
#define XCLMGMT_OID_PRP_FORCE_ICAP_PROGRAM_AXLF CTL_CODE(FILE_DEVICE_UNKNOWN, XCLMGMT_IOC_PRP_FORCE_ICAP_PROGRAM_AXLF, METHOD_BUFFERED, FILE_ANY_ACCESS)
/* IOC_GET_DEVICE_PCI_INFO gets the device-specific info */
#define XCLMGMT_OID_GET_DEVICE_PCI_INFO     CTL_CODE(FILE_DEVICE_UNKNOWN, XCLMGMT_IOC_GET_DEVICE_PCI_INFO, METHOD_BUFFERED, FILE_ANY_ACCESS)
//
// Struct for XCLMGMT_OID_GET_DEVICE_INFO IOCTL
// MAC address is a 48-bit formatted string - "aa:bb:cc:dd:ee:ff"
//
#pragma pack(push)
#pragma pack(1)
typedef struct {
    CHAR   SerialNumber[16];
    CHAR   ShellName[64];
    CHAR   ShellFilename[64];
    CHAR   BMCVersion[16];
    CHAR   MacAddress[20];
    UINT32 VlanTag;
} XCLMGMT_DEVICE_INFO, *PXCLMGMT_DEVICE_INFO;
#pragma pack(pop)

#pragma warning(disable:4201) // nameless struct/union warning
typedef union _DRIVER_VERSION
{
    struct
    {
        /* [Minor Version Number] Indicates the minor version is "0". */
        USHORT MNR;
        /* [Major Version Number] Indicates the major version is "1". */
        USHORT MJR;
    };
    ULONG AsUlong;
} DRIVER_VERSION, *PDRIVER_VERSION;
#pragma warning(default:4201) // nameless struct/union warning
typedef struct pcie_config_info {
    USHORT vendor;
    USHORT device;
    USHORT subsystem_vendor;
    USHORT subsystem_device;
    USHORT pcie_link_width;
    USHORT pcie_link_speed;
}PCIE_CONFIG_INFO, *PPCIE_CONFIG_INFO;

typedef struct sysmon_info {
    UINT32 temp;
    UINT32 temp_min;
    UINT32 temp_max;
    UINT32 vcc;
    UINT32 vcc_min;
    UINT32 vcc_max;
    UINT32 vcc_aux;
    UINT32 vcc_aux_min;
    UINT32 vcc_aux_max;
    UINT32 vcc_bram;
    UINT32 vcc_bram_min;
    UINT32 vcc_bram_max;
}SYSMON_INFO, *PSYSMON_INFO;

/* Structure available for golden */
typedef struct xclmgmt_ioc_device_pci_info {
    PCIE_CONFIG_INFO pcie_info;
}XCLMGMT_IOC_DEVICE_PCI_INFO, *PXCLMGMT_IOC_DEVICE_PCI_INFO;

/* Structure not available for golden */
typedef struct xclmgmt_ioc_device_info {
    DRIVER_VERSION   version;
    ULONGLONG        feature_id;
    ULONGLONG        time_stamp;
    USHORT           ddr_channel_num;
    USHORT           ddr_channel_size;
    CHAR             vbnv[64];
    CHAR             fpga[64];
    SYSMON_INFO      sysmoninfo;
    UINT32           ocl_frequency[XCLMGMT_NUM_SUPPORTED_CLOCKS];
    bool             mig_calibration[4];
    USHORT           num_clocks;
    ULONGLONG        xmc_offset;
    struct FeatureRomHeader rom_hdr;
}XCLMGMT_IOC_DEVICE_INFO, *PXCLMGMT_IOC_DEVICE_INFO;

/* Structure used to save 2RP related UUID information */
typedef struct xclmgmt_ioc_uuid_info {
    CHAR             blp_logic_uuid[64];
    CHAR             blp_interface_uuid[64];
    CHAR             plp_logic_uuid[64];
    CHAR             plp_interface_uuid[64];
}XCLMGMT_IOC_UUID_INFO, *PXCLMGMT_IOC_UUID_INFO;

struct rp_download {
    USHORT rp_type;
    axlf *axlf_buf;
};
enum {
    RP_DOWNLOAD_NORMAL,
    RP_DOWNLOAD_DRY,
    RP_DOWNLOAD_FORCE,
    RP_DOWNLOAD_CLEAR,
};

//PRP download status
enum {
    RP_DOWNLOAD_IN_PROGRESS,
    RP_DOWLOAD_SUCCESS,
    RP_DOWLOAD_FAILED,
};
