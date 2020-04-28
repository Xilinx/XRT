
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

#include "dmatest.h"

DMATest::DMATest( Xbtest_Pfm_Def_t xbtest_pfm_def, DeviceInterface *device, Testcase_Parameters_t test_parameters, Global_Config_t global_config )
{
    m_state     = TestState::TS_NOT_SET;
    m_result    = TestResult::TR_PASSED;

    m_log       = Logging::getInstance();
    m_log_msg_test_type = "DMATEST    : ";
    m_abort     = false;

    m_xbtest_pfm_def = xbtest_pfm_def;
    m_device = device;
    m_test_parameters = test_parameters;
    m_global_config = global_config;
}

DMATest::~DMATest () {}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DMATest::PreSetup()
{
    bool ret = true;
    LogMessage(LOG_INFO, "PreSetup");
    m_state = TestState::TS_PRE_SETUP;
    return ret;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DMATest::PostTeardown()
{
    LogMessage(LOG_STATUS, "PostTeardown");
    m_state = TestState::TS_POST_TEARDOWN;

    m_outputfile_all.flush();
    m_outputfile_ddr.flush();
    m_outputfile_hbm.flush();

    m_outputfile_all.close();
    m_outputfile_ddr.close();
    m_outputfile_hbm.close();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DMATest::Abort()
{
    if (m_abort == false)
    {
        LogMessage(LOG_INFO, "Abort received");
        m_abort = true;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DMATest::ParseTestSequenceSettings(DMATestcaseCfg_t TC_Cfg, std::list<TestItConfig_t> *test_list )
{
    bool parse_failure = false;
    uint  parse_error_cnt = 0;
    int  test_cnt = 0;
    TestItConfig_t test_it_cfg;

    std::vector<DMA_Test_Sequence_Parameters_t> test_sequence;
    Json_Parameters_t::iterator it = FindJsonParam(&(m_test_parameters.param), TEST_SEQUENCE_MEMBER);
    if (it != m_test_parameters.param.end())
        test_sequence = TestcaseParamCast<std::vector<DMA_Test_Sequence_Parameters_t>>(it->second);

    for (auto test_seq_param : test_sequence)
    {
        if (m_abort == true) break;
        test_cnt ++;
        bool parse_it_failure = false;
        if (parse_it_failure == false)
        {
            parse_it_failure |= CheckParam<uint>(DURATION, test_seq_param.duration, MIN_DURATION, MAX_DURATION);
            test_it_cfg.duration = test_seq_param.duration;
        }
        if (parse_it_failure == false)
        {
            parse_it_failure |= CheckStringInSet(test_seq_param.mem_type, m_supported_test_mem_type);
            test_it_cfg.mem_type = test_seq_param.mem_type;

            if (StrMatchNoCase(test_it_cfg.mem_type, TEST_MEM_TYPE_BANK))
                test_it_cfg.mem_type = TEST_MEM_TYPE_DDR;

            if (StrMatchNoCase(test_it_cfg.mem_type, TEST_MEM_TYPE_HBM))
            {
                test_it_cfg.min_mem_index   = MIN_HBM_MEM_INDEX;
                test_it_cfg.max_mem_index   = m_max_hbm_mem_index;
                test_it_cfg.min_buffer_size = MIN_HBM_BUFFER_SIZE;
                test_it_cfg.max_buffer_size = m_max_hbm_buffer_size;
            }
            else
            {
                test_it_cfg.min_mem_index   = MIN_DDR_MEM_INDEX;
                test_it_cfg.max_mem_index   = m_max_ddr_mem_index;
                test_it_cfg.min_buffer_size = MIN_DDR_BUFFER_SIZE;
                test_it_cfg.max_buffer_size = m_max_ddr_buffer_size;
            }
        }
        if (parse_it_failure == false)
        {
            test_it_cfg.test_sequence_mode = test_seq_param.test_sequence_mode; // "SINGLE" or "ALL" (already set in inputparser)
            if (StrMatchNoCase(test_it_cfg.test_sequence_mode, TEST_SEQUENCE_MODE_SINGLE) == true)
            {
                parse_it_failure |= CheckParam<uint> (MEM_INDEX, test_seq_param.mem_index, test_it_cfg.min_mem_index, test_it_cfg.max_mem_index);
                test_it_cfg.mem_index = test_seq_param.mem_index;
                test_it_cfg.mem_tag = m_device->MemTypeIndexToMemTag(test_it_cfg.mem_type, test_it_cfg.mem_index);
                if (parse_it_failure == false) parse_it_failure |= GetInMemTopology(&test_it_cfg, &m_mem_data);
            }
        }
        if (parse_it_failure == false)
        {
            parse_it_failure |= CheckParam<uint64_t>  (BUFFER_SIZE, test_seq_param.buffer_size, test_it_cfg.min_buffer_size, test_it_cfg.max_buffer_size);
            test_it_cfg.buffer_size = test_seq_param.buffer_size;
        }
        uint total_size;
        std::string total_size_txt;
        if (StrMatchNoCase(test_it_cfg.mem_type, TEST_MEM_TYPE_HBM))
        {
            total_size = TC_Cfg.hbm_total_size;
            total_size_txt = HBM_TOTAL_SIZE_MEMBER.name;
        }
        else
        {
            total_size = TC_Cfg.ddr_total_size;
            total_size_txt = DDR_TOTAL_SIZE_MEMBER.name;
        }
        if (parse_it_failure == false)
        {
            if ((test_it_cfg.buffer_size > total_size) || (total_size%test_it_cfg.buffer_size != 0))
            {
                LogMessage(LOG_FAILURE, total_size_txt + " must be a multiple of " + BUFFER_SIZE);
                parse_it_failure = true;
            }
        }
        double buff_cnt_d = 0.0;
        if (parse_it_failure == false)
        {
            test_it_cfg.buff_size_bytes    = test_it_cfg.buffer_size*1024*1024;  // Bytes
            test_it_cfg.buff_size_int      = test_it_cfg.buff_size_bytes/sizeof(int);  // Bytes

            if (StrMatchNoCase(test_it_cfg.mem_type, TEST_MEM_TYPE_HBM))
            {
                buff_cnt_d = ((double)TC_Cfg.hbm_total_size)/((double)test_it_cfg.buffer_size);
                test_it_cfg.buffer_count = (uint64_t)buff_cnt_d;
            }
            else
            {
                buff_cnt_d = ((double)TC_Cfg.ddr_total_size)/((double)test_it_cfg.buffer_size);
                test_it_cfg.buffer_count = (uint64_t)buff_cnt_d;
            }

            if (test_it_cfg.buffer_count > MAX_BUFFER_COUNT)
            {
                test_it_cfg.buffer_count = MAX_BUFFER_COUNT;
                LogMessage(LOG_CRIT_WARN, "Number of buffer limited to " + std::to_string(test_it_cfg.buffer_count));
            }
            else if ((test_it_cfg.buffer_count == 0) || (((double)test_it_cfg.buffer_count) != buff_cnt_d))
            {
                parse_it_failure = true;
                LogMessage(LOG_FAILURE, "Failed to compute number of buffers, check " + BUFFER_SIZE);
            }
            test_it_cfg.total_size_bytes = test_it_cfg.buffer_count*test_it_cfg.buff_size_bytes;
        }

        if (parse_it_failure == false)
        {
            if (StrMatchNoCase(test_it_cfg.test_sequence_mode, TEST_SEQUENCE_MODE_SINGLE) == true)
            {
                test_list->push_back(test_it_cfg);
            }
            else
            {
                test_it_cfg.mem_topol_idx = 0; // Start looking at index 0
                bool mem_used_found = FindNextUsedInMemTopology(&test_it_cfg, &m_mem_data); // Find first Mem Data
                if ((mem_used_found == false) and (test_it_cfg.mem_topol_idx == 0))
                {
                    LogMessage(LOG_FAILURE, "No memory found in Memory Topology for memory type: " + test_it_cfg.mem_type);
                    parse_it_failure = true;
                }
                while (mem_used_found == true)
                {
                    test_list->push_back(test_it_cfg);
                    test_it_cfg.mem_topol_idx++;
                    mem_used_found = FindNextUsedInMemTopology(&test_it_cfg, &m_mem_data);
                }
            }
        }

        if (parse_it_failure == true)
        {
            LogMessage(LOG_FAILURE, "Test " + std::to_string(test_cnt) + ": invalid parameters" );
            parse_error_cnt ++;
            if (parse_error_cnt > MAX_NUM_PARSER_ERROR) break;
        }
        else
        {
            std::string params = "";
            params += std::to_string(test_it_cfg.duration) + ", ";
            params += "\"" + test_it_cfg.mem_type + "\", ";
            if (StrMatchNoCase(test_it_cfg.test_sequence_mode, TEST_SEQUENCE_MODE_ALL) == true)
                params += "\"" + TEST_SEQUENCE_MODE_ALL + "\", ";
            else
                params += std::to_string(test_it_cfg.mem_index) + ", " ;
            params += std::to_string(test_it_cfg.buffer_size);
            LogMessage(LOG_DEBUG, "Test " + std::to_string(test_cnt) + " parameters: " + params);
        }
        parse_failure |= parse_it_failure;
    }
    return parse_failure;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DMATest::Run()
{
    Json_Parameters_t::iterator it;

    m_state  = TestState::TS_RUNNING;
    m_result = TestResult::TR_FAILED;

    DMATestcaseCfg_t TC_Cfg;

    LogMessage(LOG_STATUS, "Run");

    // parse testcase parameters
    bool global_settings_failure = false;

    ///////////////////////////////////////////////////////////////////////
    // Get verbosity if defined in JSON, else use top level one
    int verbosity = (int)m_global_config.verbosity;
    bool verbos_ret = GetVerbosity(&(m_test_parameters.param), &verbosity);
    if (verbos_ret == true)
        LogMessage(LOG_FAILURE, VERBOSITY_FAILURE);
    global_settings_failure |= verbos_ret;
    m_global_config.verbosity = static_cast<LogLevel>(verbosity);

    ///////////////////////////////////////////////////////////////////////

    m_page_size = getpagesize();
    LogMessage(LOG_INFO, "Using memory page size: " + std::to_string(m_page_size) + " bytes");

    cl_cmd_queue    = m_device->GetCmdQueueInstance();
    cl_context      = m_device->GetContextInstance();

    ///////////////////////////////////////////////////////////////////////
    // Get parameters defined in xbtest_pfm_def.json
    m_ddr_exists = m_xbtest_pfm_def.memory.ddr_exists;
    m_hbm_exists = m_xbtest_pfm_def.memory.hbm_exists;

    if (m_ddr_exists == true)
    {
        m_max_ddr_total_size = m_xbtest_pfm_def.memory.ddr.size; // MB
        m_supported_test_mem_type.insert(TEST_MEM_TYPE_DDR);
        m_supported_test_mem_type.insert(TEST_MEM_TYPE_BANK);
        m_max_ddr_mem_index = m_xbtest_pfm_def.memory.ddr.quantity - 1;
        if (m_max_ddr_total_size < m_max_ddr_buffer_size)
            m_max_ddr_buffer_size = m_xbtest_pfm_def.memory.ddr.size;  // MB
        m_nom_lo_thresh_wr_ddr = m_xbtest_pfm_def.memory.ddr.dma_bw.write.low;
        m_nom_hi_thresh_wr_ddr = m_xbtest_pfm_def.memory.ddr.dma_bw.write.high;
        m_nom_lo_thresh_rd_ddr = m_xbtest_pfm_def.memory.ddr.dma_bw.read.low;
        m_nom_hi_thresh_rd_ddr = m_xbtest_pfm_def.memory.ddr.dma_bw.read.high;
    }
    if (m_hbm_exists == true)
    {
        m_max_hbm_total_size = m_xbtest_pfm_def.memory.hbm.size;  // MB
        m_supported_test_mem_type.insert(TEST_MEM_TYPE_HBM);
        m_max_hbm_mem_index = m_xbtest_pfm_def.memory.hbm.quantity - 1;
        if (m_max_hbm_total_size < m_max_hbm_buffer_size)
            m_max_hbm_buffer_size = m_max_hbm_total_size;  // MB
        m_nom_lo_thresh_wr_hbm = m_xbtest_pfm_def.memory.hbm.dma_bw.write.low;
        m_nom_hi_thresh_wr_hbm = m_xbtest_pfm_def.memory.hbm.dma_bw.write.high;
        m_nom_lo_thresh_rd_hbm = m_xbtest_pfm_def.memory.hbm.dma_bw.read.low;
        m_nom_hi_thresh_rd_hbm = m_xbtest_pfm_def.memory.hbm.dma_bw.read.high;
    }
    // Saturate nominal parameter values
    if (m_max_ddr_total_size < m_nom_ddr_total_size)
        m_nom_ddr_total_size = m_max_ddr_total_size;
    if (m_max_hbm_total_size < m_nom_hbm_total_size)
        m_nom_hbm_total_size = m_max_hbm_total_size;

    ///////////////////////////////////////////////////////////////////////
    m_mem_topology = m_device->GetMemoryTopology();
    m_device->PrintUsedMemTopology(m_mem_topology);

    // Lock device during test
    m_device->LockDevice();

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // Overwrite PASS/FAIL BW threshold if present in JSON file
    if (m_ddr_exists == true)
    {
        global_settings_failure |= GetJsonParamNum<uint>(LO_THRESH_WR_DDR_MEMBER, MIN_LO_THRESH_WR_DDR, m_nom_lo_thresh_wr_ddr, MAX_LO_THRESH_WR_DDR, &(TC_Cfg.lo_thresh_wr_ddr));
        global_settings_failure |= GetJsonParamNum<uint>(HI_THRESH_WR_DDR_MEMBER, MIN_HI_THRESH_WR_DDR, m_nom_hi_thresh_wr_ddr, MAX_HI_THRESH_WR_DDR, &(TC_Cfg.hi_thresh_wr_ddr));
        global_settings_failure |= CheckThresholdLoVsHi<uint>(LO_THRESH_WR_DDR_MEMBER, TC_Cfg.lo_thresh_wr_ddr, HI_THRESH_WR_DDR_MEMBER, TC_Cfg.hi_thresh_wr_ddr);

        global_settings_failure |= GetJsonParamNum<uint>(LO_THRESH_RD_DDR_MEMBER, MIN_LO_THRESH_RD_DDR, m_nom_lo_thresh_rd_ddr, MAX_LO_THRESH_RD_DDR, &(TC_Cfg.lo_thresh_rd_ddr));
        global_settings_failure |= GetJsonParamNum<uint>(HI_THRESH_RD_DDR_MEMBER, MIN_HI_THRESH_RD_DDR, m_nom_hi_thresh_rd_ddr, MAX_HI_THRESH_RD_DDR, &(TC_Cfg.hi_thresh_rd_ddr));
        global_settings_failure |= CheckThresholdLoVsHi<uint>(LO_THRESH_RD_DDR_MEMBER, TC_Cfg.lo_thresh_rd_ddr, HI_THRESH_RD_DDR_MEMBER, TC_Cfg.hi_thresh_rd_ddr);
    }
    if (m_hbm_exists == true)
    {
        global_settings_failure |= GetJsonParamNum<uint>(LO_THRESH_WR_HBM_MEMBER, MIN_LO_THRESH_WR_HBM, m_nom_lo_thresh_wr_hbm, MAX_LO_THRESH_WR_HBM, &(TC_Cfg.lo_thresh_wr_hbm));
        global_settings_failure |= GetJsonParamNum<uint>(HI_THRESH_WR_HBM_MEMBER, MIN_HI_THRESH_WR_HBM, m_nom_hi_thresh_wr_hbm, MAX_HI_THRESH_WR_HBM, &(TC_Cfg.hi_thresh_wr_hbm));
        global_settings_failure |= CheckThresholdLoVsHi<uint>(LO_THRESH_WR_HBM_MEMBER, TC_Cfg.lo_thresh_wr_hbm, HI_THRESH_WR_HBM_MEMBER, TC_Cfg.hi_thresh_wr_hbm);

        global_settings_failure |= GetJsonParamNum<uint>(LO_THRESH_RD_HBM_MEMBER, MIN_LO_THRESH_RD_HBM, m_nom_lo_thresh_rd_hbm, MAX_LO_THRESH_RD_HBM, &(TC_Cfg.lo_thresh_rd_hbm));
        global_settings_failure |= GetJsonParamNum<uint>(HI_THRESH_RD_HBM_MEMBER, MIN_HI_THRESH_RD_HBM, m_nom_hi_thresh_rd_hbm, MAX_HI_THRESH_RD_HBM, &(TC_Cfg.hi_thresh_rd_hbm));
        global_settings_failure |= CheckThresholdLoVsHi<uint>(LO_THRESH_RD_HBM_MEMBER, TC_Cfg.lo_thresh_rd_hbm, HI_THRESH_RD_HBM_MEMBER, TC_Cfg.hi_thresh_rd_hbm);
    }

    // Test source
    global_settings_failure |= GetJsonParamStr(TEST_SOURCE_MEMBER, SUPPORTED_TEST_SOURCE, &(TC_Cfg.test_source), TEST_SOURCE_MEMBER_JSON);

    if (m_ddr_exists == true)
    {
        // Get test total number of GB to transfer between host and target DDR memory
        global_settings_failure |= GetJsonParamNum<uint>(DDR_TOTAL_SIZE_MEMBER, MIN_DDR_TOTAL_SIZE, m_nom_ddr_total_size, m_max_ddr_total_size, &(TC_Cfg.ddr_total_size));
    }
    if (m_hbm_exists == true)
    {
        // Get test total number of GB to transfer between host and target DDR memory
        global_settings_failure |= GetJsonParamNum<uint>(HBM_TOTAL_SIZE_MEMBER, MIN_HBM_TOTAL_SIZE, m_nom_hbm_total_size, m_max_hbm_total_size, &(TC_Cfg.hbm_total_size));
    }

    // Get output file name and test source if defined
    it = FindJsonParam(&(m_test_parameters.param), OUTPUT_FILE_MEMBER);
    if (it != m_test_parameters.param.end())
    {
        m_outputfile_name = TestcaseParamCast<std::string>(it->second);
        m_use_outputfile  = true;
        global_settings_failure |= OpenOutputFile(m_outputfile_name + ".csv", &m_outputfile_all );
        m_outputfile_all << "Tag,Buffer Size,Iteration,Write BW,Read BW" << "\n";
        m_outputfile_all.flush();

        if (m_ddr_exists == true)
        {
            if (IsTypeUsedInMemTopology (TEST_MEM_TYPE_DDR) || IsTypeUsedInMemTopology (TEST_MEM_TYPE_BANK))
            {
                m_use_outputfile_ddr = true;
                global_settings_failure |= OpenOutputFile(m_outputfile_name + "_ddr.csv", &m_outputfile_ddr );
                WriteMemOutputFirstLine(TEST_MEM_TYPE_DDR);
            }
        }
        if (m_hbm_exists == true)
        {
            if (IsTypeUsedInMemTopology (TEST_MEM_TYPE_HBM))
            {
                m_use_outputfile_hbm = true;
                global_settings_failure |= OpenOutputFile(m_outputfile_name + "_hbm.csv", &m_outputfile_hbm );
                WriteMemOutputFirstLine(TEST_MEM_TYPE_HBM);
            }
        }
    }
    // Mode BW checked
    global_settings_failure |= GetJsonParamBool(CHECK_BW_MEMBER, &(TC_Cfg.check_bw),  false);

    if (global_settings_failure == true)
        m_abort = true;

    int thread_state = 1;
    bool parse_failure = false;

    if (m_abort == false)
    {
        LogMessage(LOG_INFO, "Test parameters:"                                                                                   );
        LogMessage(LOG_INFO, "\t- " + std::string(TEST_SOURCE_MEMBER.name)         + ": " + TC_Cfg.test_source                         );
        if (m_ddr_exists == true)
        {
            LogMessage(LOG_INFO, "\t- " + std::string(DDR_TOTAL_SIZE_MEMBER.name)      + ": " + std::to_string(TC_Cfg.ddr_total_size)      );
            LogMessage(LOG_INFO, "\t- " + std::string(LO_THRESH_WR_DDR_MEMBER.name)    + ": " + std::to_string(TC_Cfg.lo_thresh_wr_ddr)    );
            LogMessage(LOG_INFO, "\t- " + std::string(HI_THRESH_WR_DDR_MEMBER.name)    + ": " + std::to_string(TC_Cfg.hi_thresh_wr_ddr)    );
            LogMessage(LOG_INFO, "\t- " + std::string(LO_THRESH_RD_DDR_MEMBER.name)    + ": " + std::to_string(TC_Cfg.lo_thresh_rd_ddr)    );
            LogMessage(LOG_INFO, "\t- " + std::string(HI_THRESH_RD_DDR_MEMBER.name)    + ": " + std::to_string(TC_Cfg.hi_thresh_rd_ddr)    );
        }
        if (m_hbm_exists == true)
        {
            LogMessage(LOG_INFO, "\t- " + std::string(HBM_TOTAL_SIZE_MEMBER.name)      + ": " + std::to_string(TC_Cfg.hbm_total_size)      );
            LogMessage(LOG_INFO, "\t- " + std::string(LO_THRESH_WR_HBM_MEMBER.name)    + ": " + std::to_string(TC_Cfg.lo_thresh_wr_hbm)    );
            LogMessage(LOG_INFO, "\t- " + std::string(HI_THRESH_WR_HBM_MEMBER.name)    + ": " + std::to_string(TC_Cfg.hi_thresh_wr_hbm)    );
            LogMessage(LOG_INFO, "\t- " + std::string(LO_THRESH_RD_HBM_MEMBER.name)    + ": " + std::to_string(TC_Cfg.lo_thresh_rd_hbm)    );
            LogMessage(LOG_INFO, "\t- " + std::string(HI_THRESH_RD_HBM_MEMBER.name)    + ": " + std::to_string(TC_Cfg.hi_thresh_rd_hbm)    );
        }
        LogMessage(LOG_INFO,     "\t- " + std::string(CHECK_BW_MEMBER.name)            + ": " + BoolToStr(TC_Cfg.check_bw)                 );

        LogMessage(LOG_INFO, "Start checking test sequence parameters" );
        std::list<TestItConfig_t> test_it_list;
        parse_failure = ParseTestSequenceSettings(TC_Cfg, &test_it_list);

        if (m_abort == false)
        {
            if (parse_failure == false)
            {
                LogMessage(LOG_PASS,"Checking test parameters finished");
            }
            else
            {
                LogMessage(LOG_FAILURE, "Some test parameters are not valid, check error messages above" );
                m_abort = true;
            }
        }
        if (m_abort == false)
        {
            // run thread async, block & wait for completion
            m_thread_future = std::async(std::launch::async, &DMATest::RunThread, this, TC_Cfg, &test_it_list);
            m_thread_future.wait();
            // check on completion if it has been aborted
            thread_state = m_thread_future.get();
        }
    }

    // Unlock device after test
    m_device->UnlockDevice();

    if ((thread_state < 0) || (m_abort == true))
    {
        LogMessage(LOG_FAILURE, "Aborted");
        m_result = TestResult::TR_ABORTED;
    }
    else if(thread_state > 0)
    {
        m_result = TestResult::TR_FAILED;
    }
    else
    {
        m_result = TestResult::TR_PASSED;
    }

    return;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int DMATest::RunThread(DMATestcaseCfg_t TC_Cfg, std::list<TestItConfig_t> *Tests_list)
{
    int ret = 0;
    cl_int cl_err = CL_SUCCESS;
    ChkClErr_t chk_cl_err = CHK_CL_ERR_SUCCESS;
    bool test_failure = false;
    bool test_it_failure = false;
    int test_it_cnt = 1;

    LogMessage(LOG_DEBUG,"Number of test iterations: " + std::to_string(Tests_list->size()));

    for (auto test_it: *Tests_list)
    {
        if (m_abort == true)
            break;

        test_it_failure = false;
        LogMessage(LOG_INFO, "Start Test: " + std::to_string(test_it_cnt));
        LogMessage(LOG_INFO, "\t " + DURATION       + ":    " + std::to_string(test_it.duration) + "s");
        LogMessage(LOG_INFO, "\t " + MEM_TAG        + ":  " + test_it.mem_tag);
        LogMessage(LOG_INFO, "\t " + BUFFER_SIZE    + ": " + std::to_string(test_it.buffer_size));

        // Initialize reference buffer pointer before allocation
        int *ref_data_buf = nullptr;
        std::vector<cl::Event> waitEvent;

        uint64_t t_0 = 0, t_1 = 0;
        rate_t rate_wr, rate_rd;

        int *host_buf[test_it.buffer_count];
        std::vector<cl::Buffer> cl_buffer_list;

        // Initialize host buffer pointers before allocation
        for (uint buff_idx = 0; buff_idx < test_it.buffer_count && (m_abort == false) && (test_it_failure == false); buff_idx++)
            host_buf[buff_idx] = nullptr;

        if ((m_abort == false) && (test_it_failure == false))
        {
            LogMessage(LOG_DEBUG,"Number of buffer: " + std::to_string(test_it.buffer_count));
            LogMessage(LOG_DEBUG,"Buffer size (MB): " + std::to_string(test_it.buffer_size));
            LogMessage(LOG_DEBUG,"Buffer size (B):  " + std::to_string(test_it.buff_size_bytes));
            LogMessage(LOG_DEBUG,"Total size (MB):  " + std::to_string(((double)test_it.total_size_bytes)/1024.0/1024.0));
        }

        uint64_t ref_t;
        GetTimestamp(&ref_t);
        uint ref_cnt = (uint)ref_t;

        if ((m_abort == false) && (test_it_failure == false))
        {
            LogMessage(LOG_DEBUG, "Initialize reference data with counter, start value: " + std::to_string(ref_cnt));
            test_it_failure |= AllocateHostBuffer<int>(m_page_size, test_it.buff_size_int, &ref_data_buf);
            if ((m_abort == false) && (test_it_failure == true))
                LogMessage(LOG_FAILURE, "Failed to allocate reference data host buffer");
        }

        for (uint ii = 0; ii < test_it.buff_size_int && (m_abort == false) && (test_it_failure == false); ii++)
            ref_data_buf[ii] = ref_cnt++; // 32-bits counter

        if ((m_abort == false) && (test_it_failure == false)) LogMessage(LOG_DEBUG, "Allocate host buffers");
        for (uint buff_idx = 0; buff_idx < test_it.buffer_count && (m_abort == false) && (test_it_failure == false); buff_idx++)
        {
            test_it_failure |= AllocateHostBuffer<int>(m_page_size, test_it.buff_size_int, &(host_buf[buff_idx]));
            if ((m_abort == false) && (test_it_failure == true))
                LogMessage(LOG_FAILURE, "Failed to allocate host buffer " + std::to_string(buff_idx));
        }

        if ((m_abort == false) && (test_it_failure == false)) LogMessage(LOG_DEBUG, "Initialize host buffers");
        for (uint buff_idx = 0; buff_idx < test_it.buffer_count && (m_abort == false) && (test_it_failure == false); buff_idx++)
            memmove(host_buf[buff_idx], ref_data_buf, test_it.buff_size_bytes);

        if ((m_abort == false) && (test_it_failure == false)) LogMessage(LOG_DEBUG, "Create OpenCL buffers");
        for (uint buff_idx = 0; buff_idx < test_it.buffer_count && (m_abort == false) && (test_it_failure == false); buff_idx++)
        {
            cl_mem_ext_ptr_t cl_mem_ext_ptr = {0};
            cl_mem_ext_ptr.param    = 0;
            cl_mem_ext_ptr.obj      = host_buf[buff_idx];
            cl_mem_ext_ptr.flags    = ((unsigned)test_it.mem_topol_idx) | XCL_MEM_TOPOLOGY;

            cl_buffer_list.push_back(cl::Buffer(*cl_context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX, test_it.buff_size_bytes, &cl_mem_ext_ptr, &cl_err));
            CheckClBufferConstructor(cl_err, "cl_buffer_list[" + std::to_string(buff_idx) + "]", "CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX", &chk_cl_err); CHK_CL_ERR_FAILURE(chk_cl_err, test_it_failure);
        }

        // Used to display elapsed duration
        uint duration_divider = 0;

        if ((test_it.duration >= 10) && (test_it.duration < 100))
            duration_divider =  5;
        else if (test_it.duration >= 100)
            duration_divider = 20;
        else
            duration_divider =  1;

        uint64_t elapsed_d = 0; // us

        GetTimestamp(&test_it.t_start);
        test_it.it_idx = 0;
        test_it.elapsed = 0;
        test_it.test_it_stat = RST_TEST_IT_STAT;

        if (test_it_failure == false)
        {
            while ((test_it.elapsed < ((uint64_t)test_it.duration)*((uint64_t)1000000)) && (m_abort == false))
            {
                if (m_abort == false)
                {
                    GetTimestamp(&t_0);
                    for (auto buff : cl_buffer_list)
                    {
                        cl::Event Event;
                        cl_err = cl_cmd_queue->enqueueMigrateMemObjects({buff}, 0, NULL, &Event);
                        CheckClEnqueueMigrateMemObjects(cl_err, "(Host->Device)", &chk_cl_err); CHK_CL_ERR_FAILURE(chk_cl_err, test_it_failure);
                        waitEvent.push_back(Event);
                    }
                    cl_err = cl::WaitForEvents(waitEvent);
                    GetTimestamp(&t_1);
                    CheckClWaitForEvents(cl_err, &chk_cl_err); CHK_CL_ERR_FAILURE(chk_cl_err, test_it_failure);

                    // block till complete
                    cl_cmd_queue->finish();

                    bool compute_failure = ComputeRate(test_it, t_1, t_0, &rate_wr);
                    if (compute_failure == true)
                        LogMessage(LOG_ERROR,"Failed to compute Write BW");
                    test_it_failure |= compute_failure;
                }
                for (uint buff_idx = 0; buff_idx < test_it.buffer_count && (m_abort == false); buff_idx++)
                    memset(host_buf[buff_idx], 0, test_it.buff_size_bytes);
                if (m_abort == false)
                {
                    // CL_MIGRATE_MEM_OBJECT_HOST indicates that the specified set of memory objects are to be migrated to the host,
                    // regardless of the target command-queue.
                    GetTimestamp(&t_0);
                    for (auto buff : cl_buffer_list)
                    {
                        cl::Event Event;
                        cl_err = cl_cmd_queue->enqueueMigrateMemObjects({buff}, CL_MIGRATE_MEM_OBJECT_HOST, NULL, &Event);
                        CheckClEnqueueMigrateMemObjects(cl_err, "(Device->Host)", &chk_cl_err); CHK_CL_ERR_FAILURE(chk_cl_err, test_it_failure);
                        waitEvent.push_back(Event);
                    }
                    cl_err = cl::WaitForEvents(waitEvent);
                    GetTimestamp(&t_1);
                    CheckClWaitForEvents(cl_err, &chk_cl_err); CHK_CL_ERR_FAILURE(chk_cl_err, test_it_failure);

                    // block till complete
                    cl_cmd_queue->finish();

                    bool compute_failure = ComputeRate(test_it, t_1, t_0, &rate_rd);
                    if (compute_failure == true)
                        LogMessage(LOG_ERROR,"Failed to compute Read BW");
                    test_it_failure |= compute_failure;
                }

                for (uint buff_idx = 0; buff_idx < test_it.buffer_count && (m_abort == false); buff_idx++)
                {
                    bool test_integ_failure = CheckDataIntegrity(host_buf[buff_idx], ref_data_buf, test_it.buff_size_bytes);
                    if (test_integ_failure == true)
                        LogMessage(LOG_ERROR,test_it.mem_tag + " - Data Integrity fail for buffer: " + std::to_string(buff_idx));
                    test_it_failure |= test_integ_failure;
                }
                if ((m_abort == false) && (TC_Cfg.check_bw == true))
                {
                    CheckRate(TC_Cfg, &test_it, rate_wr, rate_rd, false);
                }

                WriteAllOutputLine(test_it, rate_wr, rate_rd);
                WriteMemOutputLine(test_it, rate_wr, rate_rd);

                test_it.it_idx++;

                if (((test_it.elapsed-elapsed_d) >= duration_divider*1000000) || (test_it.it_idx == 0))
                {
                    LogMessage(LOG_STATUS, "\t" + std::to_string((uint64_t)test_it.duration - (test_it.elapsed/1000000)) + " Seconds Remaining of DMA Test");
                    if (test_it.it_idx != 0)
                    {
                        LogMessage(LOG_DEBUG, std::to_string(test_it.it_idx) + " iterations performed");
                        PrintResults(LOG_DEBUG, TC_Cfg, test_it, rate_wr, rate_rd);
                    }
                    elapsed_d = test_it.elapsed;
                }

                uint64_t t_curr;  //current timestamp
                GetTimestamp(&t_curr);
                test_it.elapsed = (t_curr - test_it.t_start);
            }

            if (m_abort == false)
            {
                if (TC_Cfg.check_bw == true)
                    test_it_failure |= CheckRate(TC_Cfg, &test_it, rate_wr, rate_rd, true);
                PrintResults(LOG_INFO, TC_Cfg, test_it, rate_wr, rate_rd);
            }
        }

        std::string msg;
        if (TC_Cfg.check_bw == true)
            msg = test_it.mem_tag + " - Data Integrity and BW test";
        else
            msg = test_it.mem_tag + " - Data Integrity test";
        if ((test_it_failure == false) && (m_abort == false))
            LogMessage(LOG_PASS, msg + " pass");
        else
            LogMessage(LOG_ERROR, msg + " fail");

        // Release memory
        cl_buffer_list.clear();
        if (ref_data_buf != nullptr) DeallocateHostBuffer<int>(ref_data_buf,test_it.buff_size_int);
        for (uint buff_idx = 0; buff_idx < test_it.buffer_count; buff_idx++)
            if (host_buf[buff_idx] != nullptr) DeallocateHostBuffer<int>(host_buf[buff_idx],test_it.buff_size_int);

        LogMessage(LOG_DEBUG,"Total test duration: " + std::to_string(((double)test_it.elapsed)/1000000.0) + " sec. Expected duration: " + std::to_string(test_it.duration));
        LogMessage(LOG_DEBUG,"Total iterations performed: " + std::to_string(test_it.it_idx));
        LogMessage(LOG_INFO, "End Test: " + std::to_string(test_it_cnt));

        test_failure |= (test_it_failure || m_abort);

        test_it_cnt++;
    }

    if (m_abort == true)
    {
        ret = -1;
    }
    else if (test_failure == true)
    {
        ret = 1;
        LogMessage(LOG_ERROR,"Test failed");
    }
    else
    {
        LogMessage(LOG_PASS,"Test passed");
    }

    return ret;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DMATest::IsTypeUsedInMemTopology ( std::string mem_type )
{
    bool mem_used_found = false;

    for (uint i = 0; i < m_mem_topology.mem_count; i++ )
    {
        if (m_mem_topology.mem_data[i].enabled == true)
        {
            if (m_device->IsTagOfType(m_mem_topology.mem_data[i].tag,mem_type))
            {
                mem_used_found = true;
                break;
            }
        }
    }
    return mem_used_found;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DMATest::GetInMemTopology ( TestItConfig_t *test_it_cfg, MemData_t *m_mem_data )
{
    return m_device->GetInMemTopology(m_mem_topology, test_it_cfg->mem_type, test_it_cfg->mem_tag, test_it_cfg->mem_index, m_mem_data, &(test_it_cfg->mem_topol_idx));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DMATest::FindNextUsedInMemTopology ( TestItConfig_t *test_it_cfg, MemData_t *m_mem_data )
{
    return m_device->FindNextUsedInMemTopology(m_mem_topology, test_it_cfg->mem_type, &(test_it_cfg->mem_topol_idx), &(test_it_cfg->mem_tag), m_mem_data);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DMATest::CheckDataIntegrity ( int *host_buf, int *ref_data_buf, uint64_t buff_size_bytes )
{
    int test_failure = false;
    int memcmp_ret = memcmp(host_buf, ref_data_buf, buff_size_bytes);
    if(memcmp_ret != 0)
    {
        LogMessage(LOG_ERROR,"Read data does not match write data");
        for (uint64_t i=0; i<buff_size_bytes; i++)
        {
            if (host_buf[i] != ref_data_buf[i])
            {
                LogMessage(LOG_INFO,"First error at index : " + std::to_string(i));
                LogMessage(LOG_INFO,"\t - Write data: " + std::to_string(ref_data_buf[i]));
                LogMessage(LOG_INFO,"\t - Read data:  " + std::to_string(host_buf[i]));
                break;
            }
        }
        test_failure = true;
    }
    return test_failure;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DMATest::ComputeRate( TestItConfig_t test_it_cfg, uint64_t t_1, uint64_t t_0, rate_t *rate )
{
    bool test_failure = false;
    uint64_t duration = t_1 - t_0; // us
    double rate_inst = 0.0;
    if (duration == 0)
    {
        test_failure = true;
    }
    else
    {
        rate_inst = ((double)test_it_cfg.total_size_bytes)/1024.0/1024.0; // MBytes
        rate_inst *= 1000000.0; // MBytes/s
        rate_inst /= (double)duration;
    }
    rate->inst = rate_inst;
    if (test_it_cfg.it_idx == 0)
    {
        rate->min = rate->inst;
        rate->max = rate->inst;
        rate->acc = rate->inst;
        rate->avg = rate->inst;
    }
    else
    {
        if (rate->inst < rate->min)
            rate->min = rate->inst;
        if (rate->max < rate->inst)
            rate->max = rate->inst;
        rate->acc += rate->inst;
    }
    rate->avg = rate->acc/(test_it_cfg.it_idx + 1);

    return test_failure;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DMATest::CheckRate (DMATestcaseCfg_t TC_Cfg, TestItConfig_t *test_it_cfg, rate_t rate_wr, rate_t rate_rd, bool report_pass_fail )
{
    bool test_failure = false;

    uint lo_thresh_wr, hi_thresh_wr, lo_thresh_rd, hi_thresh_rd;
    std::string result_str = "";

    LogLevel log_level_fail = LOG_DEBUG; // Message severity for result
    LogLevel log_level_pass = LOG_DEBUG; // Message severity for result
    if (report_pass_fail == true) log_level_fail = LOG_ERROR;
    if (report_pass_fail == true) log_level_pass = LOG_PASS;

    uint rate_wr_chk = (uint)rate_wr.inst;
    uint rate_rd_chk = (uint)rate_rd.inst;
    if (report_pass_fail == true) rate_wr_chk = (uint)rate_wr.avg;
    if (report_pass_fail == true) rate_rd_chk = (uint)rate_rd.avg;

    if (StrMatchNoCase(test_it_cfg->mem_type, TEST_MEM_TYPE_HBM))
    {
        lo_thresh_wr = TC_Cfg.lo_thresh_wr_hbm;
        hi_thresh_wr = TC_Cfg.hi_thresh_wr_hbm;

        lo_thresh_rd = TC_Cfg.lo_thresh_rd_hbm;
        hi_thresh_rd = TC_Cfg.hi_thresh_rd_hbm;
    }
    else
    {
        lo_thresh_wr = TC_Cfg.lo_thresh_wr_ddr;
        hi_thresh_wr = TC_Cfg.hi_thresh_wr_ddr;

        lo_thresh_rd = TC_Cfg.lo_thresh_rd_ddr;
        hi_thresh_rd = TC_Cfg.hi_thresh_rd_ddr;
    }
    std::string wr_bw_range = "[" + std::to_string(lo_thresh_wr) + " MBps; " + std::to_string(hi_thresh_wr) + " MBps]";
    std::string rd_bw_range = "[" + std::to_string(lo_thresh_rd) + " MBps; " + std::to_string(hi_thresh_rd) + " MBps]";

    result_str = "Host -> PCIe -> FPGA (" + test_it_cfg->mem_tag + ")";
    if (report_pass_fail == false)  result_str += " instantaneous ";
    else                            result_str += " average ";
    result_str += "write BW: " + std::to_string(rate_wr_chk) + " MB/s";
    if (rate_wr_chk < lo_thresh_wr)
    {
        LogMessage(log_level_fail,result_str + " below expected: " + std::to_string(lo_thresh_wr) + " MB/s");
        test_failure = true;
        if (report_pass_fail == false) test_it_cfg->test_it_stat.wr_bw_fail_cnt++;
    }
    else if (rate_wr_chk > hi_thresh_wr)
    {
        LogMessage(log_level_fail,result_str + " above expected: " + std::to_string(hi_thresh_wr) + " MB/s");
        test_failure = true;
        if (report_pass_fail == false) test_it_cfg->test_it_stat.wr_bw_fail_cnt++;
    }
    else
    {
        LogMessage(log_level_pass,result_str + " inside " + wr_bw_range);
        if (report_pass_fail == false) test_it_cfg->test_it_stat.wr_bw_pass_cnt++;
    }

    result_str = "Host <- PCIe <- FPGA (" + test_it_cfg->mem_tag + ")";
    if (report_pass_fail == false)  result_str += " instantaneous ";
    else                            result_str += " average ";
    result_str += "read BW: " + std::to_string(rate_rd_chk) + " MB/s";
    if (rate_rd_chk < lo_thresh_rd)
    {
        LogMessage(log_level_fail,result_str + " below expected: " + std::to_string(lo_thresh_rd) + " MB/s");
        test_failure = true;
        if (report_pass_fail == false) test_it_cfg->test_it_stat.rd_bw_fail_cnt++;
    }
    else if (rate_rd_chk > hi_thresh_rd)
    {
        LogMessage(log_level_fail,result_str + " above expected: " + std::to_string(hi_thresh_rd) + " MB/s");
        test_failure = true;
        if (report_pass_fail == false) test_it_cfg->test_it_stat.rd_bw_fail_cnt++;
    }
    else
    {
        LogMessage(log_level_pass,result_str + " inside " + rd_bw_range);
        if (report_pass_fail == false) test_it_cfg->test_it_stat.rd_bw_pass_cnt++;
    }

    if (report_pass_fail == false) test_failure = false;


    return test_failure;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DMATest::PrintResults( LogLevel Level, DMATestcaseCfg_t TC_Cfg, TestItConfig_t test_it_cfg, rate_t rate_wr, rate_t rate_rd )
{
    uint lo_thresh_wr, hi_thresh_wr, lo_thresh_rd, hi_thresh_rd;
    if (StrMatchNoCase(test_it_cfg.mem_type, TEST_MEM_TYPE_HBM))
    {
        lo_thresh_wr = TC_Cfg.lo_thresh_wr_hbm;
        hi_thresh_wr = TC_Cfg.hi_thresh_wr_hbm;

        lo_thresh_rd = TC_Cfg.lo_thresh_rd_hbm;
        hi_thresh_rd = TC_Cfg.hi_thresh_rd_hbm;
    }
    else
    {
        lo_thresh_wr = TC_Cfg.lo_thresh_wr_ddr;
        hi_thresh_wr = TC_Cfg.hi_thresh_wr_ddr;

        lo_thresh_rd = TC_Cfg.lo_thresh_rd_ddr;
        hi_thresh_rd = TC_Cfg.hi_thresh_rd_ddr;
    }
    std::string wr_bw_range = "[" + std::to_string(lo_thresh_wr) + " MBps; " + std::to_string(hi_thresh_wr) + " MBps]";
    std::string rd_bw_range = "[" + std::to_string(lo_thresh_rd) + " MBps; " + std::to_string(hi_thresh_rd) + " MBps]";

    LogMessage(Level, "Host -> PCIe -> FPGA (" + test_it_cfg.mem_tag + ") write BW: ");
    LogMessage(Level, "\t - Minimum: " + std::to_string((uint)rate_wr.min) + " MB/s");
    LogMessage(Level, "\t - Average: " + std::to_string((uint)rate_wr.avg) + " MB/s");
    LogMessage(Level, "\t - Maximum: " + std::to_string((uint)rate_wr.max) + " MB/s");
    if (TC_Cfg.check_bw == true)
    {
        LogMessage(LOG_DEBUG, "\t - " + std::to_string(test_it_cfg.test_it_stat.wr_bw_pass_cnt) + " measurement(s)  inside " + wr_bw_range);
        LogMessage(LOG_DEBUG, "\t - " + std::to_string(test_it_cfg.test_it_stat.wr_bw_fail_cnt) + " measurement(s) outside " + wr_bw_range);
    }
    LogMessage(Level, "Host <- PCIe <- FPGA (" + test_it_cfg.mem_tag + ") read BW: ");
    LogMessage(Level, "\t - Minimum: " + std::to_string((uint)rate_rd.min) + " MB/s");
    LogMessage(Level, "\t - Average: " + std::to_string((uint)rate_rd.avg) + " MB/s");
    LogMessage(Level, "\t - Maximum: " + std::to_string((uint)rate_rd.max) + " MB/s");
    if (TC_Cfg.check_bw == true)
    {
        LogMessage(LOG_DEBUG, "\t - " + std::to_string(test_it_cfg.test_it_stat.rd_bw_pass_cnt) + " measurement(s)  inside " + rd_bw_range);
        LogMessage(LOG_DEBUG, "\t - " + std::to_string(test_it_cfg.test_it_stat.rd_bw_fail_cnt) + " measurement(s) outside " + rd_bw_range);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DMATest::WriteMemOutputFirstLine ( std::string mem_type )
{
    std::string line = "";
    std::vector<uint> map_idx_outpufile;
    bool use_outputfile = false;

    if ((StrMatchNoCase(mem_type, TEST_MEM_TYPE_DDR)) || (StrMatchNoCase(mem_type, TEST_MEM_TYPE_BANK)))
        use_outputfile = m_use_outputfile_ddr;
    else if (StrMatchNoCase(mem_type, TEST_MEM_TYPE_HBM))
        use_outputfile = m_use_outputfile_hbm;

    if (use_outputfile == true)
    {
        for (uint i = 0; i < m_mem_topology.mem_count; i++ )
        {
            if (m_mem_topology.mem_data[i].enabled == true)
            {
                std::string mem_topology_tag = m_mem_topology.mem_data[i].tag;
                if (m_device->IsTagOfType(mem_topology_tag,mem_type))
                {
                    map_idx_outpufile.push_back(i);
                    line += mem_topology_tag + " Buffer Size"   + ",";
                    line += mem_topology_tag + " Iteration"     + ",";
                    line += mem_topology_tag + " Write BW"      + ",";
                    line += mem_topology_tag + " Read BW"       + ",";
                }
            }
        }
        line += "\n";

        if ((StrMatchNoCase(mem_type, TEST_MEM_TYPE_DDR)) || (StrMatchNoCase(mem_type, TEST_MEM_TYPE_BANK)))
        {
            m_map_idx_outpufile_ddr = map_idx_outpufile;
            m_outputfile_ddr << line;
            m_outputfile_ddr.flush();
        }
        else if (StrMatchNoCase(mem_type, TEST_MEM_TYPE_HBM))
        {
            m_map_idx_outpufile_hbm = map_idx_outpufile;
            m_outputfile_hbm << line;
            m_outputfile_hbm.flush();
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DMATest::WriteMemOutputLine ( TestItConfig_t test_it_cfg, rate_t rate_wr, rate_t rate_rd )
{
    std::string line = "";
    std::vector<uint> map_idx_outpufile;
    bool use_outputfile = false;

    if ((StrMatchNoCase(test_it_cfg.mem_type, TEST_MEM_TYPE_DDR)) || (StrMatchNoCase(test_it_cfg.mem_type, TEST_MEM_TYPE_BANK)))
    {
        map_idx_outpufile   = m_map_idx_outpufile_ddr;
        use_outputfile      = m_use_outputfile_ddr;
    }
    else if (StrMatchNoCase(test_it_cfg.mem_type, TEST_MEM_TYPE_HBM))
    {
        map_idx_outpufile   = m_map_idx_outpufile_hbm;
        use_outputfile      = m_use_outputfile_hbm;
    }

    if (use_outputfile == true)
    {
        for (auto map_idx : map_idx_outpufile)
        {
            if (test_it_cfg.mem_topol_idx == map_idx)
            {
                line += std::to_string(  test_it_cfg.buffer_size    )   += ",";
                line += std::to_string(  test_it_cfg.it_idx         )   += ",";
                line += std::to_string(  rate_wr.inst               )   += ",";
                line += std::to_string(  rate_rd.inst               )   += ",";
            }
            else
            {
                line += ",";
                line += ",";
                line += ",";
                line += ",";
            }
        }
        line += "\n";

        if ((StrMatchNoCase(test_it_cfg.mem_type, TEST_MEM_TYPE_DDR)) || (StrMatchNoCase(test_it_cfg.mem_type, TEST_MEM_TYPE_BANK)))
        {
            m_outputfile_ddr << line;
            m_outputfile_ddr.flush();
        }
        else if (StrMatchNoCase(test_it_cfg.mem_type, TEST_MEM_TYPE_HBM))
        {
            m_outputfile_hbm << line;
            m_outputfile_hbm.flush();
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DMATest::WriteAllOutputLine ( TestItConfig_t test_it_cfg, rate_t rate_wr, rate_t rate_rd)
{
    if (m_use_outputfile == true)
    {
        m_outputfile_all
            <<                  test_it_cfg.mem_tag         << ","
            << std::to_string(  test_it_cfg.buffer_size )   << ","
            << std::to_string(  test_it_cfg.it_idx      )   << ","
            << std::to_string(  rate_wr.inst            )   << ","
            << std::to_string(  rate_rd.inst            )   << ","
            << "\n";
        m_outputfile_all.flush();
    }
}
