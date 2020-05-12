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

#include "gttest.h"

GTTest::GTTest( DeviceInterface *device, Testcase_Parameters_t test_parameters, int kernel_idx, Global_Config_t global_config )
{
	m_state = TestState::TS_NOT_SET;
	m_result = TestResult::TR_PASSED;

	m_log = Logging::getInstance();
    m_log_msg_test_type = "GT_TEST" + std::to_string(kernel_idx) + "    : ";
    m_abort = false;

    m_device = device;
    m_test_parameters = test_parameters;
    m_kernel_idx = kernel_idx;
    m_global_config = global_config;
}

GTTest::~GTTest() {}

bool GTTest::PreSetup()
{
    bool ret = true;
    LogMessage(LOG_STATUS, "PreSetup");
    m_state = TestState::TS_PRE_SETUP;
    return ret;
}

void GTTest::PostTeardown()
{
    LogMessage(LOG_STATUS, "PostTeardown");
    m_state = TestState::TS_POST_TEARDOWN;

    m_outputfile.flush();
    m_outputfile.close();
}

void GTTest::Abort()
{
    LogMessage(LOG_INFO, "Signal abort");
    m_abort = true;
}

uint GTTest::ReadGTKernel(uint address)
{
    uint read_data;
    read_data = m_device->ReadGTKernel(m_kernel_idx, address);

    return read_data;
}

void GTTest::WriteGTKernel(uint address, uint value)
{
    m_device->WriteGTKernel(m_kernel_idx, address, value);
}

uint GTTest::PolarityCfg(std::string pol)
{
    uint pol_i = 0;

    if      (StrMatchNoCase(pol, "0_0_0_1")) pol_i = 1;
    else if (StrMatchNoCase(pol, "0_0_1_0")) pol_i = 2;
    else if (StrMatchNoCase(pol, "0_0_1_1")) pol_i = 3;
    else if (StrMatchNoCase(pol, "0_1_0_0")) pol_i = 4;
    else if (StrMatchNoCase(pol, "0_1_0_1")) pol_i = 5;
    else if (StrMatchNoCase(pol, "0_1_1_0")) pol_i = 6;
    else if (StrMatchNoCase(pol, "0_1_1_1")) pol_i = 7;
    else if (StrMatchNoCase(pol, "1_0_0_0")) pol_i = 8;
    else if (StrMatchNoCase(pol, "1_0_0_1")) pol_i = 9;
    else if (StrMatchNoCase(pol, "1_0_1_0")) pol_i = 10;
    else if (StrMatchNoCase(pol, "1_0_1_1")) pol_i = 11;
    else if (StrMatchNoCase(pol, "1_1_0_0")) pol_i = 12;
    else if (StrMatchNoCase(pol, "1_1_0_1")) pol_i = 13;
    else if (StrMatchNoCase(pol, "1_1_1_0")) pol_i = 14;
    else if (StrMatchNoCase(pol, "1_1_1_1")) pol_i = 15;

    return pol_i;
}

void GTTest::SetGTCfg(GTTestcaseCfg_t TC_Cfg)
{
    uint read_data;
    read_data = ReadGTKernel(CTRL_GT_CFG_REG_ADDR);

    uint reg_lpbk = CTRL_GT_LOOPBACK_OFF;

    // Select loopback
    if      (StrMatchNoCase(TC_Cfg.gt_loopback, GT_LOOPBACK_NE_PCS))    reg_lpbk = CTRL_GT_LOOPBACK_NE_PCS;
    else if (StrMatchNoCase(TC_Cfg.gt_loopback, GT_LOOPBACK_NE_PMA))    reg_lpbk = CTRL_GT_LOOPBACK_NE_PMA;
    else if (StrMatchNoCase(TC_Cfg.gt_loopback, GT_LOOPBACK_FE_PMA))    reg_lpbk = CTRL_GT_LOOPBACK_FE_PMA;
    else if (StrMatchNoCase(TC_Cfg.gt_loopback, GT_LOOPBACK_FE_PCS))    reg_lpbk = CTRL_GT_LOOPBACK_FE_PCS;

    // Apply loopback to 4 GTs
    reg_lpbk = (reg_lpbk << 0) | (reg_lpbk << 3) | (reg_lpbk << 6) | (reg_lpbk << 9);
    read_data = (read_data & ~CTRL_GT_LOOPBACK_MASK) | (reg_lpbk & CTRL_GT_LOOPBACK_MASK);

    // remove previous polarity settings
    read_data = read_data & ~ (CTRL_GT_TX_POL_MASK | CTRL_GT_RX_POL_MASK);
    read_data |= (PolarityCfg(TC_Cfg.gt_tx_pol) << 24) + (PolarityCfg(TC_Cfg.gt_rx_pol) << 20);

    if (StrMatchNoCase(TC_Cfg.rx_reverse, SET_ON))
        read_data |= CTRL_RX_REVERSE;
    else
        read_data &= ~CTRL_RX_REVERSE;

    if (StrMatchNoCase(TC_Cfg.tx_reverse, SET_ON))
        read_data |= CTRL_TX_REVERSE;
    else
        read_data &= ~CTRL_TX_REVERSE;

    if (StrMatchNoCase(TC_Cfg.tx_phase, SET_ON))
        read_data |= CTRL_TX_PHASE;
    else
        read_data &= ~CTRL_TX_PHASE;

    if (StrMatchNoCase(TC_Cfg.scramb_dis, SET_ON))
        read_data |= CTRL_SCRAMB_DIS;
    else
        read_data &= ~CTRL_SCRAMB_DIS;

    if (StrMatchNoCase(TC_Cfg.retime_dis, SET_ON))
        read_data |= CTRL_RETIME_DIS;
    else
        read_data &= ~CTRL_RETIME_DIS;

    if (StrMatchNoCase(TC_Cfg.align_dis, SET_ON))
        read_data |= CTRL_ALIGN_DIS;
    else
        read_data &= ~CTRL_ALIGN_DIS;

    WriteGTKernel(CTRL_GT_CFG_REG_ADDR, read_data );
}

void GTTest::ResetGT()
{
    uint read_data;

    read_data = ReadGTKernel(CTRL_GT_GTRST_REG_ADDR);
    read_data |= CTRL_GT_RESET;
    WriteGTKernel(CTRL_GT_GTRST_REG_ADDR, read_data );

    read_data = ReadGTKernel(CTRL_GT_GTRST_REG_ADDR);
    read_data &= ~CTRL_GT_RESET;
    WriteGTKernel(CTRL_GT_GTRST_REG_ADDR, read_data );
}

int GTTest::RunThread(GTTestcaseCfg_t TC_Cfg, std::list<TestItConfig_t> *Tests_list)
{
	int ret = 0;

    SetGTCfg(TC_Cfg);
    ResetGT();

    uint read_data;
    read_data = ReadGTKernel(CTRL_GT_CFG_REG_ADDR);
    LogMessage(LOG_INFO, "read CTRL_GT_CFG_REG_ADDR: 0x" + NumToStrHex<uint>(read_data));
    read_data = ReadGTKernel(CTRL_GT_GTRST_REG_ADDR);
    LogMessage(LOG_INFO, "read CTRL_GT_GTRST_REG_ADDR: 0x" + NumToStrHex<uint>(read_data));

    bool test_failure       = false;

    // bool test_it_failure    = false;
    // int test_it_cnt         = 1;
    //
    // for (auto test_it: *Tests_list)
    // {
    //     if (m_abort == true)
    //         break;
    //
    //     test_it_failure = false;
    //     LogMessage(LOG_INFO, "Start Test: " + std::to_string(test_it_cnt));
    //     // ADD your TEST sequnce here
    //     LogMessage(LOG_INFO, "End Test: " + std::to_string(test_it_cnt));
    //
    //     test_failure |= (test_it_failure || m_abort);
    //
    //     test_it_cnt++;
    // }

    // check for overall test failure
    if (m_abort == true)
    {
        ret = -1;
    }
    else if (test_failure == true)
    {
        ret = 1;
    }

    return ret;

}

void GTTest::Run()
{
    Json_Parameters_t::iterator it;

    m_state     = TestState::TS_RUNNING;
    m_result    = TestResult::TR_FAILED;

    GTTestcaseCfg_t TC_Cfg;

    LogMessage(LOG_STATUS, "Run");

    // parse testcase parameters
    bool global_settings_failure = false;

    // Get verbosity if defined in JSON, else use top level one
    int verbosity = (int)m_global_config.verbosity;
    bool verbos_ret = GetVerbosity(&(m_test_parameters.param), &verbosity);
    if (verbos_ret == true)
        LogMessage(LOG_FAILURE, VERBOSITY_FAILURE);
    global_settings_failure |= verbos_ret;
    m_global_config.verbosity = static_cast<LogLevel>(verbosity);

    // configuration test source
    global_settings_failure |= GetJsonParamStr(TEST_SOURCE_MEMBER,      SUPPORTED_TEST_SOURCE,      &(TC_Cfg.test_source),  TEST_SOURCE_MEMBER_JSON);
    global_settings_failure |= GetJsonParamStr(GT_LOOPBACK_MEMBER,      TEST_SUPPORTED_GT_LOOPBACK, &(TC_Cfg.gt_loopback),  GT_LOOPBACK_OFF);
    global_settings_failure |= GetJsonParamStr(GT_RX_REVERSE_MEMBER,    TEST_SUPPORTED_ON_OFF,      &(TC_Cfg.rx_reverse),   SET_ON);
    global_settings_failure |= GetJsonParamStr(GT_TX_REVERSE_MEMBER,    TEST_SUPPORTED_ON_OFF,      &(TC_Cfg.tx_reverse),   SET_ON);
    global_settings_failure |= GetJsonParamStr(GT_TX_PHASE_MEMBER,      TEST_SUPPORTED_ON_OFF,      &(TC_Cfg.tx_phase),     SET_ON);
    global_settings_failure |= GetJsonParamStr(GT_SCRAMB_DIS_MEMBER,    TEST_SUPPORTED_ON_OFF,      &(TC_Cfg.scramb_dis),   SET_OFF);
    global_settings_failure |= GetJsonParamStr(GT_RETIME_DIS_MEMBER,    TEST_SUPPORTED_ON_OFF,      &(TC_Cfg.retime_dis),   SET_OFF);
    global_settings_failure |= GetJsonParamStr(GT_ALIGN_DIS_MEMBER,     TEST_SUPPORTED_ON_OFF,      &(TC_Cfg.align_dis),    SET_OFF);
    global_settings_failure |= GetJsonParamStr(GT_RX_POL_MEMBER,        TEST_SUPPORTED_GT_POL,      &(TC_Cfg.gt_rx_pol),    "0_0_0_0");
    global_settings_failure |= GetJsonParamStr(GT_TX_POL_MEMBER,        TEST_SUPPORTED_GT_POL,      &(TC_Cfg.gt_tx_pol),    "0_0_0_0");

    // measurement output file
    it = FindJsonParam(&(m_test_parameters.param), OUTPUT_FILE_MEMBER);
    if (it != m_test_parameters.param.end())
    {
        m_outputfile_name = TestcaseParamCast<std::string>(it->second);
        m_use_outputfile  = true;
        global_settings_failure |= OpenOutputFile(m_outputfile_name + ".csv", &m_outputfile );
        // m_outputfile << "TODO" << "\n";
        m_outputfile.flush();
    }

    int thread_state = 1;
    bool parse_failure = false;

    if ((global_settings_failure == false) && (m_abort == false))
    {
        LogMessage(LOG_INFO, "Test parameters:"                                                                   );
        LogMessage(LOG_INFO, "\t- " + TEST_SOURCE_MEMBER.name   + ": " + TC_Cfg.test_source);
        LogMessage(LOG_INFO, "\t- " + GT_LOOPBACK_MEMBER.name   + ": " + TC_Cfg.gt_loopback);
        LogMessage(LOG_INFO, "\t- " + GT_RX_REVERSE_MEMBER.name + ": " + TC_Cfg.rx_reverse);
        LogMessage(LOG_INFO, "\t- " + GT_TX_REVERSE_MEMBER.name + ": " + TC_Cfg.tx_reverse);
        LogMessage(LOG_INFO, "\t- " + GT_TX_PHASE_MEMBER.name   + ": " + TC_Cfg.tx_phase);
        LogMessage(LOG_INFO, "\t- " + GT_SCRAMB_DIS_MEMBER.name + ": " + TC_Cfg.scramb_dis);
        LogMessage(LOG_INFO, "\t- " + GT_RETIME_DIS_MEMBER.name + ": " + TC_Cfg.retime_dis);
        LogMessage(LOG_INFO, "\t- " + GT_ALIGN_DIS_MEMBER.name  + ": " + TC_Cfg.align_dis);

        LogMessage(LOG_INFO, "Start checking test sequence parameters" );
        parse_failure = ParseTestSequenceSettings(TC_Cfg, &m_test_it_list);

        if (m_abort == false)
        {
            if (parse_failure == false)
            {
                LogMessage(LOG_PASS, "Checking test parameters finished");
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
            m_thread_future = std::async(std::launch::async, &GTTest::RunThread, this, TC_Cfg, &m_test_it_list);
            m_thread_future.wait();
            // check on completion if it has been aborted
            thread_state = m_thread_future.get();
        }
    }

    if ((thread_state < 0) || (m_abort == true))
    {
        LogMessage(LOG_FAILURE, "Aborted");
        m_result = TestResult::TR_ABORTED;
    }
    else if (thread_state > 0)
    {
        m_result = TestResult::TR_FAILED;
    }
    else
    {
        m_result = TestResult::TR_PASSED;
    }

    return;
}

bool GTTest::ParseTestSequenceSettings(GTTestcaseCfg_t TC_Cfg, std::list<TestItConfig_t> *test_list )
{
    bool parse_failure = false;
    return parse_failure;
}
