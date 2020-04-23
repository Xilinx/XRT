/**********
Copyright (c) 2018, Xilinx, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********/

#ifndef _DEVICEINTERFACE_H
#define _DEVICEINTERFACE_H

#include "xbtestcommon.h"
#include "clchecker.h"
#include "xbutildumpparser.h"
#include "xclbinutilparser.h"

class DeviceInterface
{

private:

    std::string m_log_msg_test_type = "DEVICE     : ";

    Xbtest_Pfm_Def_t m_xbtest_pfm_def;

    std::atomic<bool> *m_abort = nullptr;
    Logging *m_log = nullptr;
    Global_Config_t m_global_config;

    std::string m_device_name;
    uint m_device_index;
    std::string m_xclbin_filename;
    bool m_verify_ret;

    // opencl
    xclDeviceHandle m_deviceHandle;
    std::vector<cl::Platform> cl_platforms;
    std::vector<cl::Device> cl_devices;
    cl::Device cl_device;
    cl::CommandQueue cl_cmd_queue;
    cl::Context cl_context;
    cl::Program cl_program;

    const char* XILINX_PLATFORM_STR = "Xilinx";

    std::string m_xclbin_uuid;
    MemTopology_t m_mem_topology;

    XbutilDumpParser *m_xbutil_dump_parser;
    XclbinUtilParser *m_xclbinutil_parser;

    const static uint MAX_NUM_KERNELS   = 8;
    const static uint MAX_KERNEL_CORES  = 32;

    const std::string KRNL_PWR_NAME     = "krnl_powertest_slr";
    const std::string KRNL_MEM_DDR_NAME = "krnl_memtest_ddr";
    const std::string KRNL_MEM_HBM_NAME = "krnl_memtest_hbm";
    const std::string KRNL_GT_NAME      = "krnl_gt_test";
    const std::string KRNL_GT_MAC_NAME  = "krnl_gt_mac_test";

    // kernels
    std::vector<cl::Kernel> kernels;
    std::string cl_kernel_names[NUM_KERNEL_TYPE][MAX_NUM_KERNELS];
    cl::Kernel cl_kernels[NUM_KERNEL_TYPE][MAX_NUM_KERNELS];
    int m_num_krnls[NUM_KERNEL_TYPE];
    int m_power_kernel_slr[MAX_NUM_KERNELS];
    int m_mem_kernel_num_core[NUM_KERNEL_TYPE][MAX_NUM_KERNELS];
    int m_mem_kernel_num_mem[NUM_KERNEL_TYPE][MAX_NUM_KERNELS];
    int m_mem_kernel_inst[NUM_KERNEL_TYPE][MAX_NUM_KERNELS];
    std::string m_mem_kernel_tag[NUM_KERNEL_TYPE][MAX_NUM_KERNELS][MAX_NUM_KERNEL_CORE];
    int m_mem_kernel_dst_type[NUM_KERNEL_TYPE][MAX_NUM_KERNELS][MAX_NUM_KERNEL_CORE];
    int m_mem_kernel_dst_idx[NUM_KERNEL_TYPE][MAX_NUM_KERNELS][MAX_NUM_KERNEL_CORE];

    // buffer pointers
    Connectivity_t m_connectivity;

    static const uint M00_ARG_INDEX = 4;
    static const uint64_t M00_AXI_BUFF_SIZE_BYTES   = 0x10000;  // 64KB
    static const uint64_t M00_AXI_SUB_SIZE_BYTES    = 0x400;    // 1KB
    std::vector<cl::Buffer> cl_m00_axi_buffer;
    std::vector<uint>       cl_m00_axi_mem_topology_index;
    std::vector<size_t>     cl_m00_axi_buffer_origin;

    static const uint64_t M_AXI_DDR_SIZE_BYTES      = 0x100000; // 1MB
    static const uint64_t M_AXI_HBM_SIZE_BYTES      = 0x100000;
    static const uint64_t M_AXI_TMP_HBM_SIZE_BYTES  = 0x100000;

    cl::Buffer cl_m00_axi_ptr[NUM_KERNEL_TYPE][MAX_NUM_KERNELS];
    cl::Buffer cl_m_axi_ptr[NUM_KERNEL_TYPE][MAX_NUM_KERNELS][MAX_KERNEL_CORES];

    uint m_xclbin_download_time = 0;

    typedef struct CuIndexNameMap_t
    {
        std::vector<std::string>  name;
        uint                      count;
    } CuIndexNameMap_t;

    CuIndexNameMap_t m_cu_index_name_map;

public:

    const static uint GT_MAC_BUF_SIZE       = 2048; // 2048 x 32b words
    const static uint GT_MAC_STATUS_SIZE    = 448;  // Buffer for status report from MAC

    void LogMessage ( LogLevel Level, std::string msg );

    DeviceInterface( Xbtest_Pfm_Def_t xbtest_pfm_def, Global_Config_t global_config, std::atomic<bool> *gAbort );
    ~DeviceInterface();
    bool SetupDevice( Json_Parameters_t *DeviceParameters );

    std::string GetClKernelNames( int kernel_type, int kernel_idx );

    int GetNumKernels( int kernel_type );
    int GetNumPowerKernels();
    int GetNumMemDDRKernels();
    int GetNumMemHBMKernels();
    int GetNumGTKernels();
    int GetNumGTMACKernels();

    int GetPowerKernelSLR( int kernel_idx );

    int GetMemKernelDDRNumCore( int kernel_idx );
    int GetMemKernelDDRNumMem( int kernel_idx );
    std::string GetMemKernelDDRTag( int kernel_idx, int kernel_core_idx );
    int GetMemKernelDDRDstType( int kernel_idx, int kernel_core_idx );
    int GetMemKernelDDRDstIdx( int kernel_idx, int kernel_core_idx );
    int GetMemKernelDDRInst( int kernel_idx );

    int GetMemKernelHBMNumCore( int kernel_idx );
    int GetMemKernelHBMNumMem( int kernel_idx );
    std::string GetMemKernelHBMTag( int kernel_idx, int kernel_core_idx );
    int GetMemKernelHBMDstType( int kernel_idx, int kernel_core_idx );
    int GetMemKernelHBMDstIdx( int kernel_idx, int kernel_core_idx );
    int GetMemKernelHBMInst( int kernel_idx );

    std::string MemTypeIndexToMemTag ( std::string mem_type, uint mem_index );
    bool IsTagOfType ( std::string mem_tag, std::string mem_type );
    void ForceNotUseBankTag ( std::string *mem_tag );
    bool GetInMemTopology ( MemTopology_t mem_topology, std::string mem_type, std::string mem_tag, uint mem_index, MemData_t *mem_data, uint *mem_topol_idx );
    bool FindNextUsedInMemTopology ( MemTopology_t mem_topology, std::string mem_type, uint *mem_topol_idx, std::string *mem_tag, MemData_t *mem_data );
    bool SetupKernels();
    void CreateHBMBuffer();

    uint ReadKernel( int kernel_type, int kernel_idx, uint address );
    uint ReadMemDDRKernel( int kernel_idx, int kernel_core_idx, uint address );
    uint ReadMemHBMKernel( int kernel_idx, int kernel_core_idx, uint address );
    uint ReadPwrKernel( int kernel_idx, uint address );
    uint ReadGTKernel( int kernel_idx, uint address );
    uint ReadGTMACKernel( int kernel_idx, uint address );

    void WriteKernel( int kernel_type, int kernel_idx, uint address, uint value );
    void WritePwrKernel( int kernel_idx, uint address, uint value );
    void WriteMemDDRKernel( int kernel_idx, int kernel_core_idx, uint address, uint value );
    void WriteMemHBMKernel( int kernel_idx, int kernel_core_idx, uint address, uint value );
    void WriteGTKernel( int kernel_idx, uint address, uint value );
    void WriteGTMACKernel( int kernel_idx, uint address, uint value );
    void WriteGTMACKernelCmd( int kernel_idx, uint value );
    void WriteGTMACTrafficCfg(int kernel_idx, uint32_t *traffic_cfg );
    void ReadGTMACTrafficCfg(int kernel_idx, uint32_t *read_buffer);

    bool CheckXCLBINDownloadTime();
    uint CheckClocks();

    typedef struct
    {
        unsigned mFanRpm;

        unsigned temperature[MAX_TEMP_SOURCES];

        double power[MAX_POWER_SOURCES];
        unsigned current[MAX_POWER_SOURCES];
        unsigned voltage[MAX_POWER_SOURCES];

        double   Power_uW;
        double   Power_mW;
        unsigned Power_W;

        double   Power_Calib_mW;

    } Device_Info;

    bool GetDeviceInfo( Device_Info *pInfo );

    void PrintAllMemTopology ( MemTopology_t mem_topology );
    void PrintUsedMemTopology ( MemTopology_t mem_topology );
    void PrintMemData ( uint mem_topology_idx, MemData_t mem_data );
    uint GetNumMemTopologyType( MemTopology_t mem_topology, std::string mem_type );

    bool GetXclbinDumpInfo();
    MemTopology_t GetMemoryTopology();

    typedef struct {
        std::string kernel_name;
        int major_version;
        int minor_version;
        int perforce_version;
        int component_id;
        uint scratch_pad;
        int slr;
        int mem_kernel_num_core;
        int mem_kernel_num_mem;
        int mem_kernel_inst;
        int mem_kernel_core_idx;
        int mem_kernel_dst_type;
        int mem_kernel_dst_idx;
        int gt_index;
        int num_reg;
        int num_dsp48e2;
        int num_ramb36;
        int num_uram288;
        int rst_detection;
    } Build_Info;

    std::string GetPwrKernelName( int kernel_idx );
    std::string GetMemDDRKernelName( int kernel_idx );
    std::string GetMemHBMKernelName( int kernel_idx );
    std::string GetGTKernelName( int kernel_idx );
    std::string GetGTMACKernelName( int kernel_idx );

    uint GetKrnlMemKrnlCoreOffset( int kernel_core_idx );
    Build_Info GetKrnlBI( int kernel_type, int kernel_idx, int kernel_core_idx );

    void PrintKrnlBI( Build_Info krnl_bi, int kernel_core_idx );
    void PrintPwrKrnlBI( Build_Info krnl_bi );
    void PrintMemDDRKrnlBI( Build_Info krnl_bi, int kernel_core_idx );
    void PrintMemHBMKrnlBI( Build_Info krnl_bi, int kernel_core_idx );
    void PrintGTKrnlBI( Build_Info krnl_bi );
    void PrintGTMACKrnlBI( Build_Info krnl_bi );

    cl::CommandQueue * GetCmdQueueInstance();
    cl::Context * GetContextInstance();

    void LockDevice();
    void UnlockDevice();

    void PrintCuIndexNameMap();
};

#endif /* _DEVICEINTERFACE_H */
