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

#ifndef _DMATEST_H
#define _DMATEST_H

#include "xbtestcommon.h"
#include "logging.h"
#include "deviceinterface.h"
#include "testinterface.h"

class DMATest: public TestInterface
{

private:

    Xbtest_Pfm_Def_t m_xbtest_pfm_def;
    bool m_ddr_exists = false;
    bool m_hbm_exists = false;

    int m_page_size = 0;

    template <typename T> bool AllocateHostBuffer( int page_size, std::size_t num, T** ptr )
    {
        void* ptr_tmp = nullptr;
        if (posix_memalign(&ptr_tmp, page_size, num*sizeof(T)))
            return true;
        if (ptr_tmp == nullptr)
            return true;
        *ptr = reinterpret_cast<T*>(ptr_tmp);
        return false;
    }
    template <typename T> void DeallocateHostBuffer( T* p, std::size_t num )
    {
        free(p);
    }

    std::atomic<bool> m_abort;
    std::future<int> m_thread_future;
    DeviceInterface *m_device;

    Logging *m_log = nullptr;

    std::string m_outputfile_name;
    bool m_use_outputfile       = false;
    bool m_use_outputfile_ddr   = false;
    bool m_use_outputfile_hbm   = false;
    std::ofstream m_outputfile_all;
    std::ofstream m_outputfile_ddr;
    std::ofstream m_outputfile_hbm;
    std::vector<uint> m_map_idx_outpufile_ddr;
    std::vector<uint> m_map_idx_outpufile_hbm;

    // Get from m_device
    cl::CommandQueue *cl_cmd_queue;
    cl::Context *cl_context;
    MemTopology_t m_mem_topology;
    MemData_t m_mem_data;

    const std::string MEM_TAG       = "Memory Tag";

    const std::set<std::string> SUPPORTED_TEST_SEQUENCE_MODES = {
        TEST_SEQUENCE_MODE_ALL,
        TEST_SEQUENCE_MODE_SINGLE
    };

    const uint MIN_DDR_TOTAL_SIZE = 1024;  // MB
    const uint NOM_DDR_TOTAL_SIZE = 4096;  // MB
    uint m_nom_ddr_total_size = NOM_DDR_TOTAL_SIZE; // MB - depend on m_max_ddr_total_size value
    uint m_max_ddr_total_size = 1; // MB - defined in xbtest_pfm_def.json

    const uint MIN_HBM_TOTAL_SIZE = 1;   // MB
    const uint NOM_HBM_TOTAL_SIZE = 256; // MB
    uint m_nom_hbm_total_size = NOM_HBM_TOTAL_SIZE; // MB - depend on m_max_hbm_total_size value
    uint m_max_hbm_total_size = 1; // MB - defined in xbtest_pfm_def.json

    const uint MAX_NUM_CONFIG_PARAM   = 4;

    const uint MIN_DURATION = 1;
    const uint MAX_DURATION = MAX_UINT_VAL;

    std::set<std::string> m_supported_test_mem_type; // defined in xbtest_pfm_def.json

    const uint MIN_DDR_MEM_INDEX = 0;
    uint m_max_ddr_mem_index = MIN_DDR_MEM_INDEX; // defined in xbtest_pfm_def.json

    const uint64_t MIN_DDR_BUFFER_SIZE = 1; // MB
    const uint64_t MAX_DDR_BUFFER_SIZE = 1024; // MB
    uint64_t m_max_ddr_buffer_size = MAX_DDR_BUFFER_SIZE; // MB - depend on m_max_ddr_total_size value

    const uint MIN_HBM_MEM_INDEX = 0;
    uint m_max_hbm_mem_index = MIN_HBM_MEM_INDEX; // defined in xbtest_pfm_def.json

    const uint64_t MIN_HBM_BUFFER_SIZE = 1; // MB
    const uint64_t MAX_HBM_BUFFER_SIZE = 256; // MB
    uint64_t m_max_hbm_buffer_size = MAX_HBM_BUFFER_SIZE; // MB - depend on m_max_hbm_total_size value

    const uint64_t MAX_BUFFER_COUNT = 0x40000;

    // PASS/FAIL BW values (MB/s)
    // TODO Find correct values
    const uint MIN_LO_THRESH_WR_DDR = 1;
    uint m_nom_lo_thresh_wr_ddr = MIN_LO_THRESH_WR_DDR; // defined in xbtest_pfm_def.json
    const uint MAX_LO_THRESH_WR_DDR = MAX_UINT_VAL;

    const uint MIN_HI_THRESH_WR_DDR = 1;
    uint m_nom_hi_thresh_wr_ddr = MIN_HI_THRESH_WR_DDR; // defined in xbtest_pfm_def.json
    const uint MAX_HI_THRESH_WR_DDR = MAX_UINT_VAL;

    const uint MIN_LO_THRESH_RD_DDR = 1;
    uint m_nom_lo_thresh_rd_ddr = MIN_LO_THRESH_RD_DDR; // defined in xbtest_pfm_def.json
    const uint MAX_LO_THRESH_RD_DDR = MAX_UINT_VAL;

    const uint MIN_HI_THRESH_RD_DDR = 1;
    uint m_nom_hi_thresh_rd_ddr = MIN_HI_THRESH_RD_DDR; // defined in xbtest_pfm_def.json
    const uint MAX_HI_THRESH_RD_DDR = MAX_UINT_VAL;

    const uint MIN_LO_THRESH_WR_HBM = 1;
    uint m_nom_lo_thresh_wr_hbm = MIN_LO_THRESH_WR_HBM; // defined in xbtest_pfm_def.json
    const uint MAX_LO_THRESH_WR_HBM = MAX_UINT_VAL;

    const uint MIN_HI_THRESH_WR_HBM = 1;
    uint m_nom_hi_thresh_wr_hbm = MIN_HI_THRESH_WR_HBM; // defined in xbtest_pfm_def.json
    const uint MAX_HI_THRESH_WR_HBM = MAX_UINT_VAL;

    const uint MIN_LO_THRESH_RD_HBM = 1;
    uint m_nom_lo_thresh_rd_hbm = MIN_LO_THRESH_RD_HBM; // defined in xbtest_pfm_def.json
    const uint MAX_LO_THRESH_RD_HBM = MAX_UINT_VAL;

    const uint MIN_HI_THRESH_RD_HBM = 1;
    uint m_nom_hi_thresh_rd_hbm = MIN_HI_THRESH_RD_HBM; // defined in xbtest_pfm_def.json
    const uint MAX_HI_THRESH_RD_HBM = MAX_UINT_VAL;

    typedef struct DMATestcaseCfg_t
    {
        std::string test_source;
        uint ddr_total_size;
        uint hbm_total_size;
        uint lo_thresh_wr_ddr;
        uint hi_thresh_wr_ddr;
        uint lo_thresh_rd_ddr;
        uint hi_thresh_rd_ddr;
        uint lo_thresh_wr_hbm;
        uint hi_thresh_wr_hbm;
        uint lo_thresh_rd_hbm;
        uint hi_thresh_rd_hbm;
        bool check_bw;
    } DMATestcaseCfg_t;


    typedef struct test_it_stat_t
    {
        uint64_t wr_bw_pass_cnt;
        uint64_t rd_bw_pass_cnt;
        uint64_t wr_bw_fail_cnt;
        uint64_t rd_bw_fail_cnt;
    } test_it_stat_t;

    const test_it_stat_t RST_TEST_IT_STAT = {
        0,  // uint64_t wr_bw_pass_cnt;
        0,  // uint64_t rd_bw_pass_cnt;
        0,  // uint64_t wr_bw_fail_cnt;
        0   // uint64_t rd_bw_fail_cnt;
    };

    typedef struct TestItConfig_t
    {
        uint        duration; // sec
        std::string test_sequence_mode;
        std::string mem_type;
        uint        mem_index;
        uint64_t    buffer_size; // MB

        uint64_t    t_start; // Start timestamp
        uint64_t    elapsed; // Elapsed duration of test (us)
        uint64_t    it_idx;

        uint        mem_topol_idx;
        std::string mem_tag;

        uint64_t    total_size_bytes; // B
        uint64_t    buff_size_bytes; // B
        uint64_t    buff_size_int; // Number of int
        uint64_t    buffer_count;

        uint        min_mem_index;
        uint        max_mem_index;
        uint64_t    min_buffer_size;
        uint64_t    max_buffer_size;

        test_it_stat_t  test_it_stat;
    } TestItConfig_t;

    typedef struct rate_t
    {
        double inst;
        double min;
        double max;
        double acc;
        double avg;
    } rate_t;

public:

    DMATest( Xbtest_Pfm_Def_t xbtest_pfm_def, DeviceInterface *device, Testcase_Parameters_t test_parameters, Global_Config_t global_config );
    ~DMATest();

    // implement virtual inherited functions
    bool PreSetup();
    void Run();
    void PostTeardown();
    void Abort();

    int RunThread( DMATestcaseCfg_t TC_Cfg, std::list<TestItConfig_t> *Tests_list );

    bool ParseTestSequenceSettings( DMATestcaseCfg_t TC_Cfg, std::list<TestItConfig_t> *test_list );

    bool IsTypeUsedInMemTopology( std::string mem_type );
    bool GetInMemTopology( TestItConfig_t *test_it_cfg, MemData_t *m_mem_data );
    bool FindNextUsedInMemTopology ( TestItConfig_t *test_it_cfg, MemData_t *m_mem_data );

    bool CheckDataIntegrity ( int *host_buf, int *ref_data_buf, uint64_t buff_size_bytes );
    bool ComputeRate( TestItConfig_t test_it_cfg, uint64_t t_1, uint64_t t_0, rate_t *rate );
    bool CheckRate(DMATestcaseCfg_t TC_Cfg, TestItConfig_t *test_it_cfg, rate_t rate_wr, rate_t rate_rd, bool report_pass_fail );
    void PrintResults( LogLevel Level, DMATestcaseCfg_t TC_Cfg, TestItConfig_t test_it_cfg, rate_t rate_wr, rate_t rate_rd );

    void WriteMemOutputFirstLine( std::string mem_type );
    void WriteMemOutputLine( TestItConfig_t test_it_cfg, rate_t rate_wr, rate_t rate_rd );
    void WriteAllOutputLine( TestItConfig_t test_it_cfg, rate_t rate_wr, rate_t rate_rd );

};

#endif /* _DMATEST_H */
