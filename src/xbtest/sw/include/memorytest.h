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

#ifndef _MEMORYTEST_H
#define _MEMORYTEST_H

#include "xbtestcommon.h"
#include "logging.h"
#include "deviceinterface.h"
#include "testinterface.h"
#include "devicemgt.h"

class MemoryTest : public TestInterface
{

public:

    MemoryTest( Xbtest_Pfm_Def_t xbtest_pfm_def, DeviceInterface *device, DeviceMgt *device_mgt, Testcase_Parameters_t test_parameters, TestType kernel_type, int kernel_idx, Global_Config_t global_config );
    ~MemoryTest();

    // implement virtual inherited functions
    bool PreSetup();
    void Run();
    void PostTeardown();
    void Abort();

private:

    DeviceInterface *m_device;
    Xbtest_Pfm_Def_t m_xbtest_pfm_def;
    DeviceMgt *m_devicemgt = nullptr;

    void WaitSecTick(uint quantity);

    TestType    m_kernel_type;
    int         m_kernel_idx;
    int         m_num_kernel_core;
    int         m_min_kernel_core_idx;
    int         m_max_kernel_core_idx;
    int         m_kernel_num_mem;
    int         m_kernel_inst;

    bool m_use_outputfile = false;
    std::ofstream m_outputfile_detail[MAX_NUM_KERNEL_CORE];
    std::ofstream m_outputfile_result[MAX_NUM_KERNEL_CORE];
    std::ofstream m_outputfile_detail_total;
    std::ofstream m_outputfile_result_total;
    uint m_err_qty[MAX_NUM_KERNEL_CORE];

    // generic
    std::atomic<bool> m_abort;
    std::future<int> m_thread_future;


    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Memory kernel parameters
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // Addresses
    const uint MEM_CTRL_ADDR                         = 0x20;
    const uint MEM_CTRL_WR_CTRL_ADDR_0               = 0x21;
    const uint MEM_CTRL_WR_CTRL_ADDR_1               = 0x22;
    const uint MEM_CTRL_RD_CTRL_ADDR_0               = 0x23;
    const uint MEM_CTRL_RD_CTRL_ADDR_1               = 0x24;
    const uint MEM_CTRL_WR_CTRL_XFER_BYTES_ADDR      = 0x25;
    const uint MEM_CTRL_RD_CTRL_XFER_BYTES_ADDR      = 0x26;
    const uint MEM_CTRL_WR_CTRL_NUM_XFER_ADDR        = 0x27;
    const uint MEM_CTRL_RD_CTRL_NUM_XFER_ADDR        = 0x28;
    const uint MEM_STAT_WR_TRANSFER_CNT_ADDR         = 0x29;
    const uint MEM_STAT_RD_TRANSFER_CNT_ADDR         = 0x2A;
    const uint MEM_STAT_TERM_ERROR_COUNT_ADDR        = 0x32;
    const uint MEM_STAT_AXI_ADDR_PTR_ADDR_0          = 0x33;
    const uint MEM_STAT_AXI_ADDR_PTR_ADDR_1          = 0x34;

    // Values
    const uint MEM_CTRL_START       = (0x1 << 0);
    const uint MEM_CTRL_UPDATE_CFG  = (0x1 << 1);

    const uint MEM_CTRL_RESET       = (0x1 << 8);

    enum Mem_Test_Mode_t {
        CTRL_TEST_MODE_ALTERNATE_WR_RD_TEST_VAL  = 0,
        CTRL_TEST_MODE_ONLY_WR_TEST_VAL          = 1,
        CTRL_TEST_MODE_ONLY_RD_TEST_VAL          = 2,
        CTRL_TEST_MODE_STOP_TEST_VAL             = 3,
        CTRL_TEST_MODE_UNKNOWN_VAL               = 4
    };
    const uint MEM_TEST_MODE_MASK   = 0x00000030;

    const uint MEM_CTRL_CLEAR_ERR   = (0x1 << 12);
    const uint MEM_CTRL_INSERT_ERR  = (0x1 << 13);

    const uint MEM_STAT_ERR         = (0x1 << 16);

    const uint NUM_SEC_WATCHDOG      = 5; // max number of sec to wait between watch dog reset


    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Boundaries of the test parameters for DDR type
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    const uint MAX_NUM_CONFIG_PARAM = 8;

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Boundaries of the test parameters
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Test parameters

    const uint MIN_DURATION = 1;
    const uint MAX_DURATION = MAX_UINT_VAL;

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    const uint64_t  MIN_CTRL_ADDR       = 0;
    const uint      MIN_BURST_SIZE      = 2;
    //              MIN_CTRL_NUM_XFER           // num_xfer must be multiple of burst_size, will be set to m_min_burst_size or test_it_cfg.wr_burst_size

    //              MAX_CTRL_ADDR               // will depend on memory size
    const uint      MAX_BURST_SIZE = 64;
    //              MAX_CTRL_NUM_XFER           // will depend on memory size

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    uint64_t    m_min_ctrl_addr;
    uint        m_min_burst_size;
    uint        m_min_ctrl_num_xfer;    // num_xfer must be multiple of burst_size, will be set to m_min_burst_size or test_it_cfg.wr_burst_size

    uint64_t    m_max_ctrl_addr;        // will depend on memory size
    uint        m_max_burst_size;
    uint        m_max_ctrl_num_xfer;    // will depend on memory size and on start address

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Thresholds JSON overwrite limits

    const uint MIN_LO_THRESH_ALT_WR_BW = 1;
    const uint MAX_LO_THRESH_ALT_WR_BW = MAX_UINT_VAL;

    const uint MIN_HI_THRESH_ALT_WR_BW = 1;
    const uint MAX_HI_THRESH_ALT_WR_BW = MAX_UINT_VAL;

    const uint MIN_LO_THRESH_ALT_RD_BW = 1;
    const uint MAX_LO_THRESH_ALT_RD_BW = MAX_UINT_VAL;

    const uint MIN_HI_THRESH_ALT_RD_BW = 1;
    const uint MAX_HI_THRESH_ALT_RD_BW = MAX_UINT_VAL;

    const uint MIN_LO_THRESH_ONLY_WR_BW = 1;
    const uint MAX_LO_THRESH_ONLY_WR_BW = MAX_UINT_VAL;

    const uint MIN_HI_THRESH_ONLY_WR_BW = 1;
    const uint MAX_HI_THRESH_ONLY_WR_BW = MAX_UINT_VAL;

    const uint MIN_LO_THRESH_ONLY_RD_BW = 1;
    const uint MAX_LO_THRESH_ONLY_RD_BW = MAX_UINT_VAL;

    const uint MIN_HI_THRESH_ONLY_RD_BW = 1;
    const uint MAX_HI_THRESH_ONLY_RD_BW = MAX_UINT_VAL;

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    typedef struct MemoryTestcaseCfg_t
    {
        std::string test_source;
        bool error_insertion;
        Xbtest_Pfm_Def_Mem_Thresh_CU_t cu_bw;
        bool check_bw;
        uint64_t memory_size;
    } MemoryTestcaseCfg_t;

    typedef struct TestItConfig_t
    {
        uint        test_mode;
        uint        duration;
        uint64_t    wr_start_addr;
        uint        wr_burst_size;
        uint        wr_num_xfer;
        uint64_t    rd_start_addr;
        uint        rd_burst_size;
        uint        rd_num_xfer;
        Xbtest_Pfm_Def_Mem_Thresh_WrRd_t thresh_wr_rd;
        uint64_t    cfg_update_time_us;
    } TestItConfig_t;

    typedef struct
    {
        double live;
        double acc;
        double average;
    } Meas;

    const Meas RESET_MEAS = {
        0.0,    // double live;
        0.0,    // double acc;
        0.0     // double average;
    };

    const uint UPDATE_MAX_DURATION = 10; // in sec
    const uint UPDATE_THRESHOLD_DURATION = 5; // in sec

    uint ReadMemKernel( int kernel_core_idx, uint address );
    void WriteMemKernel( int kernel_core_idx, uint address, uint value );
    std::string GetMemKernelName();
    int GetMemKernelNumCore();
    int GetMemKernelNumMem();
    std::string GetMemKernelTag( int kernel_core_idx );
    std::string GetMemKernelTag2( int kernel_core_idx );
    int GetMemKernelInst();
    std::string GetMemTypeStr();

    void PrintRegHex( int kernel_core_idx, uint reg_addr, std::string reg_name );
    void PrintConfig( int kernel_core_idx );
    void PrintConfigCores();

    void InsertError( int kernel_core_idx );
    void InsertErrorCores();

    void ClearError( int kernel_core_idx );
    void ClearErrorCores();

    uint GetErrCnt( int kernel_core_idx );

    void SetTestMode( uint value );

    void StartKernel();
    void StopKernel();

    void ActivateReset();
    void ClearReset();

    void UpdateCfgKernel( int kernel_core_idx );
    void UpdateCfgKernelCores();

    uint GetStatCfgUpdatedLatch( int kernel_core_idx );
    bool WaitCfgUpdated( TestItConfig_t test_it );
    bool GetConfigurationUpdateTime ( MemoryTestcaseCfg_t TC_Cfg, TestItConfig_t *test_it );

    void SetWrCtrlAddr( int kernel_core_idx, uint64_t value );
    void SetWrCtrlAddrCores( uint64_t value );

    void SetWrCtrlXferBytes( int kernel_core_idx, uint value );
    void SetWrCtrlXferBytesCores( uint value );

    void SetWrCtrlNumXfer( int kernel_core_idx, uint value );
    void SetWrCtrlNumXferCores( uint value );

    void SetRdCtrlAddr( int kernel_core_idx, uint64_t value );
    void SetRdCtrlAddrCores( uint64_t value );

    void SetRdCtrlXferBytes( int kernel_core_idx, uint value );
    void SetRdCtrlXferBytesCores( uint value );

    void SetRdCtrlNumXfer( int kernel_core_idx, uint value );
    void SetRdCtrlNumXferCores( uint value );

    uint GetStatWrTransferCnt( int kernel_core_idx );
    uint GetStatRdTransferCnt( int kernel_core_idx );

    uint GetHW1SecToggle( int kernel_core_idx );
    bool CheckStatErrorEnLatch( int kernel_core_idx );

    uint64_t GetAxiAddrPtr( int kernel_core_idx );

    bool CheckXferModBurst( TestItConfig_t test_it );

    void SetSequenceCores( TestItConfig_t test_it );

    template<typename T>  T SelectParamDDRorHBM( T sel_val_ddr, T sel_val_hbm );

    void WriteToMeasurementFileDetail( std::ofstream *measurement_file, int test_idx, TestItConfig_t test_it, Meas Wr_bw, Meas Rd_bw);
    void WriteToMeasurementFileResult( std::ofstream *measurement_file, int test_idx, TestItConfig_t test_it, Meas Wr_bw, Meas Rd_bw);

    void PrintTestItConfig( TestItConfig_t test_it );

    int RunThread( MemoryTestcaseCfg_t TC_Cfg, std::list<TestItConfig_t> *Tests_list );

    bool CheckBWInRange( LogLevel log_level_pass, LogLevel log_level_fail, double value, double min, double max, std::string msg );
    void GetBWRange ( MemoryTestcaseCfg_t TC_Cfg, TestItConfig_t *test_it );

    bool ParseTestSequenceSettings( MemoryTestcaseCfg_t TC_Cfg, std::list<TestItConfig_t> *test_list );

    bool GetErrorInsertionConfig( MemoryTestcaseCfg_t TC_Cfg, TestItConfig_t *test_it );

    bool CheckResult( LogLevel log_level_pass, LogLevel log_level_fail, std::string direction, MemoryTestcaseCfg_t TC_Cfg, TestItConfig_t test_it, int kernel_core_idx, Meas meas_bw);

    std::string TestModeEnumToString( uint type );
    uint TestModeStringToEnum( std::string type );

    void ResetWatchdog();
    bool StartTestAndEnableWatchdog();
    bool StopTestAndDisableWatchdog();
};


#endif /* _MEMORYTEST_H */
