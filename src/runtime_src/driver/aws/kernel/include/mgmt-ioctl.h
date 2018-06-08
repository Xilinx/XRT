#ifndef _AWSMGMT_IOCALLS_POSIX_H_
#define _AWSMGMT_IOCALLS_POSIX_H_

#include <linux/ioctl.h>

#define AWSMGMT_IOC_MAGIC	'X'
#define AWSMGMT_NUM_SUPPORTED_CLOCKS 4
#define AWSMGMT_NUM_ACTUAL_CLOCKS 3

enum AWSMGMT_IOC_TYPES {
	AWSMGMT_IOC_INFO,
	AWSMGMT_IOC_ICAP_DOWNLOAD,
	AWSMGMT_IOC_FREQ_SCALING,
	AWSMGMT_IOC_ICAP_DOWNLOAD_AXLF,
	AWSMGMT_IOC_MAX
};

struct awsmgmt_ioc_info {
	unsigned short vendor;
	unsigned short device;
	unsigned short subsystem_vendor;
	unsigned short subsystem_device;
	unsigned driver_version;
	unsigned device_version;
	unsigned short ocl_frequency[AWSMGMT_NUM_SUPPORTED_CLOCKS];
	unsigned pcie_link_width;
	unsigned pcie_link_speed;
	bool mig_calibration[4];
};

struct awsmgmt_ioc_bitstream {
	struct xclBin *xclbin;
};

struct awsmgmt_ioc_bitstream_axlf {
	struct axlf *xclbin;
};

struct awsmgmt_ioc_freqscaling {
	unsigned ocl_region;
	unsigned short ocl_target_freq[AWSMGMT_NUM_SUPPORTED_CLOCKS];
};

#define AWSMGMT_IOCINFO		  _IOWR(AWSMGMT_IOC_MAGIC,AWSMGMT_IOC_INFO,		 struct awsmgmt_ioc_info)
#define AWSMGMT_IOCICAPDOWNLOAD	  _IOW(AWSMGMT_IOC_MAGIC,AWSMGMT_IOC_ICAP_DOWNLOAD,	 struct awsmgmt_ioc_bitstream)
#define AWSMGMT_IOCICAPDOWNLOAD_AXLF	 _IOW(AWSMGMT_IOC_MAGIC,AWSMGMT_IOC_ICAP_DOWNLOAD_AXLF,	 struct awsmgmt_ioc_bitstream_axlf)
#define AWSMGMT_IOCFREQSCALING    _IOW(AWSMGMT_IOC_MAGIC,AWSMGMT_IOC_FREQ_SCALING,	 struct awsmgmt_ioc_freqscaling)

#endif


