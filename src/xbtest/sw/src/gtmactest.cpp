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

#include "gtmactest.h"

GTMACTest::GTMACTest(Xbtest_Pfm_Def_t xbtest_pfm_def, DeviceInterface *device, DeviceMgt *device_mgt, Testcase_Parameters_t test_parameters, int kernel_idx, Global_Config_t global_config)
{
	m_state = TestState::TS_NOT_SET;
	m_result = TestResult::TR_PASSED;

    m_log = Logging::getInstance();
    m_log_msg_test_type = "GTMAC_TEST" + std::to_string(kernel_idx) + ": ";

    m_abort = false;

    m_xbtest_pfm_def = xbtest_pfm_def;
    m_device = device;
	m_devicemgt = device_mgt;
    m_test_parameters = test_parameters;
    m_kernel_idx = kernel_idx;
    m_global_config = global_config;
}

GTMACTest::~GTMACTest() {}

bool GTMACTest::PreSetup()
{
    bool ret = true;
    LogMessage(LOG_STATUS, "PreSetup");
    m_state = TestState::TS_PRE_SETUP;
    return ret;
}

void GTMACTest::WaitSecTick(uint quantity)
{
    for (uint i=0; i<quantity && (m_abort == false); i++)
    {
        m_devicemgt->WaitFor1sTick();
    }
}

void GTMACTest::PostTeardown()
{
    LogMessage(LOG_STATUS, "PostTeardown");
    m_state = TestState::TS_POST_TEARDOWN;

    for(uint32_t n = 0; n < 4 ; n++)
    {
        m_outputfile[n].flush();
        m_outputfile[n].close();
    }
}

void GTMACTest::Abort()
{
    if (m_abort == false)
    {
        LogMessage(LOG_INFO, "Abort received");
        m_abort = true;
    }
}

uint32_t GTMACTest::CalcScript(MAC_Config_t *Conf, uint32_t Packet_Size, float Utilisation)
{
    const float FUDGE   = 0.996;    // Adjustment to match rate on Viavi analyser
    uint32_t    Enc_Size;

    // Check the packet size is an allowed value, and convert to the hw range of 0..2047
    // Zero and invalid returns no packet and maximum delay

    if (Packet_Size == 0) {
        return  0xFFFFF800;
    } else if ((Packet_Size >= 64) && (Packet_Size <= 1535)) {
        Enc_Size    = Packet_Size;
    } else if ((Packet_Size >= 9500) && (Packet_Size <= 10011)) {
        Enc_Size    = Packet_Size - 7964;
    } else {
        LogMessage(LOG_ERROR, "Invalid Packet Size of " + std::to_string(Packet_Size) + " specified" );
        return  0xFFFFF800;
    }

    // For max utilisation, set the delay to 0

    if (Utilisation >= 100.0) {
        return Enc_Size & 0x000007FF;
    }

    if (Utilisation <= 0.0) {

        // For Zero or negative Utilisation set the Delay to max and size to 0

        return  0xFFFFF800;

    } else {
        float       BytesPerClk = (Conf->Rate_10 ? 10000.0 : 25000.0) / (300.0 * 8.0);
        float       Delay;
        uint32_t    Bytes;

        // Add Preamble and IFG, round up to the next 4 byte boundary, Scale by 100 as Utilisation is %
        // Calculate the number of clock cycles delay required, turnaround is 3 cycles, use 2.5 to allow truncation to integer

        Bytes   = (((Packet_Size + 8 + Conf->IPG + 3) / 4) * 4) * 100;
        Delay   = ((float(Bytes) * FUDGE) / (Utilisation * BytesPerClk)) - 2.5;

        // Check if the required utilisation exceeds the counter range, if it does simply set the counter to Max

        if (Delay >= (1 << 21)) {

            return 0xFFFFF800 | (Enc_Size & 0x000007FF);

        } else {

            return ((uint32_t(Delay) << 11) & 0xFFFFF800) | (Enc_Size & 0x000007FF);
        }
    }
}

uint32_t  GTMACTest::ParseMACStatus( uint32_t *read_buffer_rx, uint rx_idx, uint32_t *read_buffer_tx, uint tx_idx, bool Check_Tx_Rx )
{
    uint32_t    Res = 0;

    for (uint32_t n = 0; n < MAC_NUM_STATS; n++) {
        uint64_t Stats_var;

        Stats_var   = READ_BUFFER_64(read_buffer_rx, n);

        if (Stats_var > 0) {

            // Check if this is a Counter that should fail the test

            if (MAC_STAT_ERR_TRIG[n])
            {
                if (m_global_config.verbosity <= 0) {
                    LogMessage(LOG_ERROR, MAC_STAT_NAMES[n] + " = \t" + std::to_string(Stats_var));
                } else {
                    LogMessage(LOG_ERROR, "MAC Instance " + std::to_string(rx_idx) + " " + MAC_STAT_NAMES[n] + " = \t" + std::to_string(Stats_var));
                }
                Res |= 1;

            } else {
                LogMessage(LOG_STATUS, MAC_STAT_NAMES[n] + " = \t" + std::to_string(Stats_var));
            }

        }
    }

    if (0 == READ_BUFFER_64(read_buffer_rx, MAC_IDX_RX_GOOD_PAC)) {
        if (m_global_config.verbosity <= 0) {
            LogMessage(LOG_ERROR, "No Good Packets received.");
        } else {
            LogMessage(LOG_ERROR, "MAC Instance " + std::to_string(rx_idx) + " No Good Packets received.");
        }
        Res |= 1;
    }

    if (Check_Tx_Rx) {

        // Check if Rx Total Good Packets = Tx Packets

        if (READ_BUFFER_64(read_buffer_rx, MAC_IDX_RX_GOOD_PAC) != READ_BUFFER_64(read_buffer_tx, MAC_IDX_TX_SENT_PAC)) {
            LogMessage(LOG_ERROR, "MAC Instance " + std::to_string(tx_idx) + " Tx vs MAC Instance " + std::to_string(rx_idx) + " Rx Packets mismatch.");
            Res |= 2;

            uint64_t Stats_rx_var = READ_BUFFER_64(read_buffer_rx, MAC_IDX_RX_GOOD_PAC);
            uint64_t Stats_tx_var = READ_BUFFER_64(read_buffer_tx, MAC_IDX_TX_SENT_PAC);
            LogMessage(LOG_DEBUG, MAC_STAT_NAMES[MAC_IDX_RX_GOOD_PAC] + " = \t" + std::to_string(Stats_rx_var));
            LogMessage(LOG_DEBUG, MAC_STAT_NAMES[MAC_IDX_TX_SENT_PAC] + " = \t" + std::to_string(Stats_tx_var));
        }

        // Check if Rx Total Good Bytes = Tx Bytes

        if (READ_BUFFER_64(read_buffer_rx, MAC_IDX_RX_GOOD_BYTE) != READ_BUFFER_64(read_buffer_tx, MAC_IDX_TX_SENT_BYTE)) {
            LogMessage(LOG_ERROR, "MAC Instance " + std::to_string(tx_idx) + " Tx vs MAC Instance " + std::to_string(rx_idx) + " Rx Bytes mismatch.");
            Res |= 2;

            uint64_t Stats_rx_var = READ_BUFFER_64(read_buffer_rx, MAC_IDX_RX_GOOD_BYTE);
            uint64_t Stats_tx_var = READ_BUFFER_64(read_buffer_tx, MAC_IDX_TX_SENT_BYTE);
            LogMessage(LOG_DEBUG, MAC_STAT_NAMES[MAC_IDX_RX_GOOD_BYTE] + " = \t" + std::to_string(Stats_rx_var));
            LogMessage(LOG_DEBUG, MAC_STAT_NAMES[MAC_IDX_TX_SENT_BYTE] + " = \t" + std::to_string(Stats_tx_var));
        }
    }

    return Res;
}

void GTMACTest::WriteGTMACCmd(int status, int conf, int run)
{
    std::string msg = "Send MAC command:";
    if (status == 1)
        msg += " status = " + std::to_string((status));
    if (conf == 1)
        msg += " conf = " + std::to_string((conf));
    if (run == 1)
        msg += " run = " + std::to_string((run));
    LogMessage(LOG_DEBUG, msg);

    uint MACCmd = 0x0003 << 19;
    MACCmd |= (status & 0x0001) << 16;
    MACCmd |= (conf   & 0x0001) << 17;
    MACCmd |= (run    & 0x0001) << 18;
    m_device->WriteGTMACKernelCmd(m_kernel_idx, MACCmd);
}

void GTMACTest::ResetWatchdog()
{
    uint read_data;

    // if a reset is requested, it also means that the watchdog is enabled
    //  don't read the current value of the CMN_WATCHDOG_ADDR to save access
    read_data = CMN_WATCHDOG_RST | CMN_WATCHDOG_EN;
    m_device->WriteGTMACKernel(m_kernel_idx, CMN_WATCHDOG_ADDR,read_data);

}


int GTMACTest::RunThread(GTMACTestcaseCfg_t TC_Cfg, std::list<TestItConfig_t> *Tests_list)
{
	int ret = 0;

//  uint read_data;

    int             test_it_cnt             = 1;
    bool            test_setting_failure    = false;
    bool            test_failure            = false;
    bool            test_it_failure         = false;
    bool            line_it_failure[4]      = {false, false, false, false};
    bool            line_failure[4]         = {false, false, false, false};
//  uint rst_flag;
    std::string     temp_str;
    MAC_Config_t    MAC_Config[4];

    uint32_t Traffic_Cfg[DeviceInterface::GT_MAC_BUF_SIZE] = {};
    uint32_t MAC_Status[DeviceInterface::GT_MAC_STATUS_SIZE] = {};

    LogMessage(LOG_INFO, "Load Cfg into PLRAM");
    for(uint32_t n = 0; n < 4 ; n++)
    {
        // Fixed MAC configuration
        MAC_Config[n].Active        = TRUE;
        MAC_Config[n].Dest_Addr     = 0x001122334450 | uint64_t(n);
        MAC_Config[n].Source_Addr   = 0x00bbccddeef0 | uint64_t(n);
        MAC_Config[n].EtherType     = 0x0800;
        MAC_Config[n].IPG           = 12;
        MAC_Config[n].Set_LFI       = FALSE;
        MAC_Config[n].Set_RFI       = FALSE;
        MAC_Config[n].Set_Idle      = FALSE;
        MAC_Config[n].Lcl_Loopback  = FALSE;
        MAC_Config[n].Script_Base   = 1904 + (n * 36);
        MAC_Config[n].Script_Length = 1;
        MAC_Config[n].MTU           = 1518;
        // Transceiver Configuration
        MAC_Config[n].GT_Tx_Diff    = 11;
        MAC_Config[n].GT_Tx_Pre     = 0;
        MAC_Config[n].GT_Tx_Post    = 0;
        MAC_Config[n].GT_Rx_Eq_Sel  = 0;    // 0 = DFE, 1 = LPM

        // MAC configuration JSON override
        MAC_Config[n].Active = TC_Cfg.line_param[n].active_mac;
        test_setting_failure |= LineRateParam2Setting(TC_Cfg.line_param[n].line_rate, &(MAC_Config[n].Rate_10));
        MAC_Config[n].Utilisation   = (float)TC_Cfg.line_param[n].utilisation;
        MAC_Config[n].Set_Test_Pat  = TC_Cfg.line_param[n].set_test_pat;
        test_setting_failure |= FECModeParam2Setting(TC_Cfg.line_param[n].fec_mode, &(MAC_Config[n].FEC_Mode));
        test_setting_failure |= TrafficTypeParam2Setting(TC_Cfg.line_param[n].traffic_type, &(MAC_Config[n].Traffic_Type));
        MAC_Config[n].Packet_Size = TC_Cfg.line_param[n].packet_size;
        test_setting_failure |= PacketCfgParam2Setting(TC_Cfg.line_param[n].packet_cfg, &(MAC_Config[n].Packet_Sweep));
        MAC_Config[n].Match_Tx_Rx   = TC_Cfg.line_param[n].match_tx_rx;
        MAC_Config[n].GT_Tx_Diff    = TC_Cfg.line_param[n].gt_tx_diffctrl;
        MAC_Config[n].GT_Tx_Pre     = TC_Cfg.line_param[n].gt_tx_pre_emph;
        MAC_Config[n].GT_Tx_Post    = TC_Cfg.line_param[n].gt_tx_post_emph;
        MAC_Config[n].GT_Rx_Eq_Sel  = TC_Cfg.line_param[n].gt_rx_use_lpm;

        // Set up single channel scripts

        Traffic_Cfg[MAC_Config[n].Script_Base] = CalcScript(&MAC_Config[n], MAC_Config[n].Packet_Size, MAC_Config[n].Utilisation);

        // Set up conditions for Packet Sweep

        if (MAC_Config[n].Packet_Sweep)
        {
            MAC_Config[n].Script_Base   = 448;
            MAC_Config[n].Script_Length = 1518 - 64 + 1;
        }

        // For Inactive channels, set the Script length to zero (Disable Packet Generator)

        if (MAC_Config[n].Active == FALSE) {
            MAC_Config[n].Script_Length = 0;
        }

        // Adjust the MTU to match Tx Jumbo frame generation

        if (MAC_Config[n].Packet_Size > 9600) {
            MAC_Config[n].MTU   = 10000;
        } else if (MAC_Config[n].Packet_Size > 1518) {
            MAC_Config[n].MTU   = 9600;
        } else {
            MAC_Config[n].MTU   = 1518;
        }

        // Internal Registers

        Traffic_Cfg[(n*16)+ 1]  = ((MAC_Config[n].Script_Length & 0x0000FFFF) << 16) | (MAC_Config[n].Script_Base & 0x0000FFFF);    // Script Length / Base
        Traffic_Cfg[(n*16)+ 2]  = ((MAC_Config[n].Dest_Addr   <<  8) & 0xFF000000) | ((MAC_Config[n].Dest_Addr   >>  8) & 0x00FF0000) | ((MAC_Config[n].Dest_Addr   >> 24) & 0x0000FF00) | ((MAC_Config[n].Dest_Addr   >> 40) & 0x000000FF);
        Traffic_Cfg[(n*16)+ 3]  = ((MAC_Config[n].Source_Addr >>  8) & 0xFF000000) | ((MAC_Config[n].Source_Addr >> 24) & 0x00FF0000) | ((MAC_Config[n].Dest_Addr   <<  8) & 0x0000FF00) | ((MAC_Config[n].Dest_Addr   >>  8) & 0x000000FF);
        Traffic_Cfg[(n*16)+ 4]  = ((MAC_Config[n].Source_Addr << 24) & 0xFF000000) | ((MAC_Config[n].Source_Addr <<  8) & 0x00FF0000) | ((MAC_Config[n].Source_Addr >>  8) & 0x0000FF00) | ((MAC_Config[n].Source_Addr >> 24) & 0x000000FF);
        Traffic_Cfg[(n*16)+ 5]  = ((MAC_Config[n].Traffic_Type & 0x00000003) << 16)                                                | ((MAC_Config[n].EtherType   <<  8) & 0x0000FF00) | ((MAC_Config[n].EtherType   >>  8) & 0x000000FF);          // Test Traffic Type / EtherType
        Traffic_Cfg[(n*16)+ 6]  = ((MAC_Config[n].GT_Tx_Post & 0x001F) << 17) | ((MAC_Config[n].GT_Tx_Pre & 0x001F) << 5) | ((MAC_Config[n].GT_Tx_Diff & 0x001F) << 0);
        Traffic_Cfg[(n*16)+ 7]  = MAC_Config[n].GT_Rx_Eq_Sel ? 0x00000001 : 0x00000000;

        // XXV Ethernet configuration Registers

        Traffic_Cfg[(n*16)+ 8]  = MAC_Config[n].Lcl_Loopback ? 0x80000000 : 0x00000000; // 0x0008 : MODE_REG
        Traffic_Cfg[(n*16)+ 9]  = 0x00000003 | ((MAC_Config[n].IPG & 0x0000000F) << 10) | (MAC_Config[n].Set_LFI ? 0x00000008 : 0x00000000) |
                                  (MAC_Config[n].Set_RFI ? 0x00000010 : 0x00000000) | (MAC_Config[n].Set_Idle ? 0x00000020 : 0x00000000) | (MAC_Config[n].Set_Test_Pat ? 0x00008000 : 0x00000000);  // TX REG1
        Traffic_Cfg[(n*16)+10]  = 0x00000033 | (MAC_Config[n].Set_Test_Pat ? 0x00000100 : 0x00000000); // 0x0014 : RX REG1
        Traffic_Cfg[(n*16)+11]  = 0x00000040 | ((MAC_Config[n].MTU & 0x00007FFF) << 16);    // 0x0018 : RX MTU
        Traffic_Cfg[(n*16)+12]  = 0x4FFF4FFF; // 0x001C : VL Length
        Traffic_Cfg[(n*16)+13]  = (FM_RS   == MAC_Config[n].FEC_Mode) ? 0x0000000D : 0x00000000;                                                                                                    // 0x00D0 : RSFEC REG   - RS-FEC Enabled
        Traffic_Cfg[(n*16)+14]  = (FM_CL74 == MAC_Config[n].FEC_Mode) ? 0x00000007 : 0x00000000;                                                                                                    // 0x00D4 : FEC REG     -- Clause 74 FEC Enable
        Traffic_Cfg[(n*16)+15]  = (MAC_Config[n].Rate_10) ? 0x00000001 : 0x00000000;                                                                                                                // 0x0138 : SWITCH SPEED REG (10G)
    }

    // Generate a sweep of all packets between 64 and 1536 bytes
    for(uint32_t n = 64; n <= 1518 ; n++) {
        Traffic_Cfg[448 + n - 64] = CalcScript(&MAC_Config[0], n, MAC_Config[0].Utilisation);
    }

    if (test_setting_failure ==  false)
        m_device->WriteGTMACTrafficCfg(m_kernel_idx, &Traffic_Cfg[0]);

//  LogMessage(LOG_INFO, "Readback configuration from Kernel");
//  m_device->ReadGTMACTrafficCfg(m_kernel_idx,   0, 563);

    for (auto test_it: *Tests_list)
    {
        if (m_abort == true)
            break;

        test_it_failure = false;
        LogMessage(LOG_INFO, "Start Test: " + std::to_string(test_it_cnt));

        // Check if we are doing a Status Read

        if (test_it.status)
        {
            // Stop Traffic Running and wait for 1 ms

            WriteGTMACCmd(0, 0, 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            // Read the Status and wait for 1 ms

            WriteGTMACCmd(1, 0, 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            // Read the Status Report

            m_device->ReadGTMACTrafficCfg(m_kernel_idx, &MAC_Status[0]);

            // And Parse each of the MACs

            for (uint32_t n = 0; n < 4; n++) {

                if (MAC_Config[n].Active) {

                    LogMessage(LOG_STATUS, "MAC Instance " + std::to_string(n));

                    // Fail if we had bad counters, Only check Tx vs Rx if enabled for this MAC

                    if (ParseMACStatus(&MAC_Status[64 + (n * 96)], n, &MAC_Status[64 + (TC_Cfg.line_param[n].tx_mapping * 96)], TC_Cfg.line_param[n].tx_mapping, MAC_Config[n].Match_Tx_Rx)) {
                        test_it_failure = TRUE;
                        line_it_failure[n] = TRUE;
                    }
                    if (line_it_failure[n] == false)
                        LogMessage(LOG_PASS, "MAC status parsing passed for line: " + std::to_string(n));
                    else
                        LogMessage(LOG_ERROR, "MAC status parsing failed for line: " + std::to_string(n));
                    line_failure[n] |= line_it_failure[n];
                    WriteOutputLine(n, line_failure[n], line_it_failure[n], &MAC_Status[64 + (n * 96)]);
                }
            }

            // If Conf or Run was set, issue an additional command

            if (test_it.conf || test_it.run)
            {
                WriteGTMACCmd(0, test_it.conf, test_it.run);
            }
        } else {
            // send MAC command
            WriteGTMACCmd(test_it.status | test_it.clr_stat, test_it.conf, test_it.run);
        }

        uint duration_divider = 1;
        if ((test_it.duration >= 10) && (test_it.duration < 100))
            duration_divider =  5;
        else if (test_it.duration >= 100)
            duration_divider = 20;

        // Loop until done or abort
        for (uint i = test_it.duration; i>=1 && (m_abort == false); i--)
        {
            m_devicemgt->WaitFor1sTick();

            if (((i % duration_divider == 0) || (i == test_it.duration)) && (m_abort == false))
            {
                temp_str = "\t" + std::to_string(i) + " sec. remaining" ;
                LogMessage(LOG_STATUS, temp_str);
            }
            if (i % 5 == 0) ResetWatchdog();
        }

        // If we have just configured the kernel, perform a number of status reads to clear the counters

        if (test_it.conf)
        {
            for (uint32_t n = 0; n < 5; n++) {

                // Wait for 1 ms

                std::this_thread::sleep_for(std::chrono::milliseconds(1));

                // Issue a Status read, preserve Run

                WriteGTMACCmd(1, 0, test_it.run);
            }
        }

        if (test_it_failure == false)
            LogMessage(LOG_PASS, "Test passed for all lines");
        else
            LogMessage(LOG_ERROR, "Test failed for some lines");

        LogMessage(LOG_INFO, "End Test: " + std::to_string(test_it_cnt));

        test_failure |= (test_it_failure || m_abort);

        test_it_cnt++;
    }

    // check for overall test failure
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

bool GTMACTest::StartTestAndEnableWatchdog()
{
    uint read_data;
    bool krnl_already_started = false;

    // start the kernel and check if the "already started" is received back
    // if it received back, check if the watchdog trigered before, meaning that previous test was abruptly stopped (kill process or terminal closed)
    //      if watchdog is enabled and alarm is present, we can safely
    //          1) clear watchdog
    //          2) start the test
    //      if no alarm or not enable, the previous run of xbtest was left in an unknown state so ask for user to "xbutil validate" it (aka remove xbtest xclbin)
    //
    // the watchdog doesn't clear the start bit
    // the watchdog is always disabled at teh of the test

    m_device->WriteGTMACKernel(m_kernel_idx, CMN_CTRL_STATUS_ADDR, CMN_STATUS_START);
    read_data = m_device->ReadGTMACKernel(m_kernel_idx, CMN_CTRL_STATUS_ADDR);
    if ((read_data & CMN_STATUS_ALREADY_START) == CMN_STATUS_ALREADY_START)
    {

        read_data = m_device->ReadGTMACKernel(m_kernel_idx, CMN_WATCHDOG_ADDR);
        // check if watchdog is already enable and error is detected
        if ( ((read_data & CMN_WATCHDOG_EN) == CMN_WATCHDOG_EN) && ((read_data & CMN_WATCHDOG_ALARM) == CMN_WATCHDOG_ALARM) )
        {
            LogMessage(LOG_WARN,"Watchdog has been triggered during previous test but start this test");
            // it's safe to restart the kernel, but first clear the start bit and the watchdog
            // stop the kernel
            m_device->WriteGTMACKernel(m_kernel_idx, CMN_CTRL_STATUS_ADDR, 0x0); // this also clears the alreay_start bit
            // stop watchdog and clear error
            m_device->WriteGTMACKernel(m_kernel_idx, CMN_WATCHDOG_ADDR, CMN_WATCHDOG_ALARM);

            // start the test
            m_device->WriteGTMACKernel(m_kernel_idx, CMN_CTRL_STATUS_ADDR, CMN_STATUS_START);
        }
        else
        {
            LogMessage(LOG_ERROR,"Test already running on GTMac CU. . By trying to start another test, this may cause error(s) in currently running test. If no tests are running, you card is maybe in unkwown state, first re-validate it, then try xbtest again");
            krnl_already_started = true;
        }

    }

    read_data = m_device->ReadGTMACKernel(m_kernel_idx, CMN_WATCHDOG_ADDR);
    if ((read_data & CMN_WATCHDOG_ALARM) == CMN_WATCHDOG_ALARM)
    {
        LogMessage(LOG_WARN,"Watchdog has been triggered during previous test.");
    }

    // enable the watchdog if the kernel was't started
    if (krnl_already_started == false)
    {
        read_data = m_device->ReadGTMACKernel(m_kernel_idx, CMN_WATCHDOG_ADDR);
        if ((read_data & CMN_WATCHDOG_ALARM) == CMN_WATCHDOG_ALARM)
        {
            LogMessage(LOG_WARN,"Watchdog has been triggered during previous test.");
        }
        // start watchdog and clear any previous alarm
        read_data = CMN_WATCHDOG_EN | CMN_WATCHDOG_ALARM;
        m_device->WriteGTMACKernel(m_kernel_idx, CMN_WATCHDOG_ADDR, read_data);
    }

    return krnl_already_started;
}

bool GTMACTest::StopTestAndDisableWatchdog()
{
    uint read_data;
    bool error = false;

    // stop the kernel and check if the "already started" is present,
    // meanign that another test tried to start teh kernl too

    read_data = m_device->ReadGTMACKernel(m_kernel_idx, CMN_CTRL_STATUS_ADDR);
    if ((read_data & CMN_STATUS_ALREADY_START) == CMN_STATUS_ALREADY_START)
    {
        LogMessage(LOG_ERROR,"Another test tried to access GTMac CU. This may have caused error to this test");
        error = true;
    }
    // stop the kernel
    m_device->WriteGTMACKernel(m_kernel_idx, CMN_CTRL_STATUS_ADDR, 0x0);


    // disable the watchdog
    read_data = m_device->ReadGTMACKernel(m_kernel_idx, CMN_WATCHDOG_ADDR);
    if ((read_data & CMN_WATCHDOG_ALARM) == CMN_WATCHDOG_ALARM)
    {
        LogMessage(LOG_ERROR,"Watchdog alarm detected. This may have caused error to this test");
        error = true;
    }
    // disable watchdog and clear any alarm detected
    m_device->WriteGTMACKernel(m_kernel_idx, CMN_WATCHDOG_ADDR, CMN_WATCHDOG_ALARM);

    return error;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void GTMACTest::Run()
{
    Json_Parameters_t::iterator it;

    m_state     = TestState::TS_RUNNING;
    m_result    = TestResult::TR_FAILED;

    GTMACTestcaseCfg_t TC_Cfg;

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

    // configuration test source. test_source = "file" not supported
    global_settings_failure |= GetJsonParamStr(TEST_SOURCE_MEMBER, SUPPORTED_TEST_SOURCE, &(TC_Cfg.test_source), TEST_SOURCE_MEMBER_JSON);

    // Get line configurations from JSON. For each parameter:
    //      1) Get TC_Cfg.line_param_a, the configuration overwrite for all lines.
    //          Note TC_Cfg.line_param_a is then set as default/nominal configuration to the line configuration overwrite.
    //      2) Get TC_Cfg.line_param[n], n = 0..3, the configuration overwrite for line n

    // configuration active mac
    global_settings_failure |= GetJsonParamBool(ACTIVE_MAC_MEMBER,   &(TC_Cfg.line_param_a.active_mac),  true);
    global_settings_failure |= GetJsonParamBool(ACTIVE_MAC_0_MEMBER, &(TC_Cfg.line_param[0].active_mac), TC_Cfg.line_param_a.active_mac);
    global_settings_failure |= GetJsonParamBool(ACTIVE_MAC_1_MEMBER, &(TC_Cfg.line_param[1].active_mac), TC_Cfg.line_param_a.active_mac);
    global_settings_failure |= GetJsonParamBool(ACTIVE_MAC_2_MEMBER, &(TC_Cfg.line_param[2].active_mac), TC_Cfg.line_param_a.active_mac);
    global_settings_failure |= GetJsonParamBool(ACTIVE_MAC_3_MEMBER, &(TC_Cfg.line_param[3].active_mac), TC_Cfg.line_param_a.active_mac);

    // configuration line rate
    global_settings_failure |= GetJsonParamStr(LINE_RATE_MEMBER,   SUPPORTED_LINE_RATE, &(TC_Cfg.line_param_a.line_rate),  LINE_RATE_25GBE);
    global_settings_failure |= GetJsonParamStr(LINE_RATE_0_MEMBER, SUPPORTED_LINE_RATE, &(TC_Cfg.line_param[0].line_rate), TC_Cfg.line_param_a.line_rate);
    global_settings_failure |= GetJsonParamStr(LINE_RATE_1_MEMBER, SUPPORTED_LINE_RATE, &(TC_Cfg.line_param[1].line_rate), TC_Cfg.line_param_a.line_rate);
    global_settings_failure |= GetJsonParamStr(LINE_RATE_2_MEMBER, SUPPORTED_LINE_RATE, &(TC_Cfg.line_param[2].line_rate), TC_Cfg.line_param_a.line_rate);
    global_settings_failure |= GetJsonParamStr(LINE_RATE_3_MEMBER, SUPPORTED_LINE_RATE, &(TC_Cfg.line_param[3].line_rate), TC_Cfg.line_param_a.line_rate);

    // configuration utilization
    global_settings_failure |= GetJsonParamNum<uint32_t>(UTILISATION_MEMBER,   MIN_UTILISATION, NOM_UTILISATION,                 MAX_UTILISATION, &(TC_Cfg.line_param_a.utilisation));
    global_settings_failure |= GetJsonParamNum<uint32_t>(UTILISATION_0_MEMBER, MIN_UTILISATION, TC_Cfg.line_param_a.utilisation, MAX_UTILISATION, &(TC_Cfg.line_param[0].utilisation));
    global_settings_failure |= GetJsonParamNum<uint32_t>(UTILISATION_1_MEMBER, MIN_UTILISATION, TC_Cfg.line_param_a.utilisation, MAX_UTILISATION, &(TC_Cfg.line_param[1].utilisation));
    global_settings_failure |= GetJsonParamNum<uint32_t>(UTILISATION_2_MEMBER, MIN_UTILISATION, TC_Cfg.line_param_a.utilisation, MAX_UTILISATION, &(TC_Cfg.line_param[2].utilisation));
    global_settings_failure |= GetJsonParamNum<uint32_t>(UTILISATION_3_MEMBER, MIN_UTILISATION, TC_Cfg.line_param_a.utilisation, MAX_UTILISATION, &(TC_Cfg.line_param[3].utilisation));

    // configuration set_test_pat
    global_settings_failure |= GetJsonParamBool(SET_TEST_PAT_MEMBER,   &(TC_Cfg.line_param_a.set_test_pat),  false);
    global_settings_failure |= GetJsonParamBool(SET_TEST_PAT_0_MEMBER, &(TC_Cfg.line_param[0].set_test_pat), TC_Cfg.line_param_a.set_test_pat);
    global_settings_failure |= GetJsonParamBool(SET_TEST_PAT_1_MEMBER, &(TC_Cfg.line_param[1].set_test_pat), TC_Cfg.line_param_a.set_test_pat);
    global_settings_failure |= GetJsonParamBool(SET_TEST_PAT_2_MEMBER, &(TC_Cfg.line_param[2].set_test_pat), TC_Cfg.line_param_a.set_test_pat);
    global_settings_failure |= GetJsonParamBool(SET_TEST_PAT_3_MEMBER, &(TC_Cfg.line_param[3].set_test_pat), TC_Cfg.line_param_a.set_test_pat);

    // configuration fec_mode
    global_settings_failure |= GetJsonParamStr(FEC_MODE_MEMBER,   SUPPORTED_FEC_MODE, &(TC_Cfg.line_param_a.fec_mode),  FEC_MODE_NONE);
    global_settings_failure |= GetJsonParamStr(FEC_MODE_0_MEMBER, SUPPORTED_FEC_MODE, &(TC_Cfg.line_param[0].fec_mode), TC_Cfg.line_param_a.fec_mode);
    global_settings_failure |= GetJsonParamStr(FEC_MODE_1_MEMBER, SUPPORTED_FEC_MODE, &(TC_Cfg.line_param[1].fec_mode), TC_Cfg.line_param_a.fec_mode);
    global_settings_failure |= GetJsonParamStr(FEC_MODE_2_MEMBER, SUPPORTED_FEC_MODE, &(TC_Cfg.line_param[2].fec_mode), TC_Cfg.line_param_a.fec_mode);
    global_settings_failure |= GetJsonParamStr(FEC_MODE_3_MEMBER, SUPPORTED_FEC_MODE, &(TC_Cfg.line_param[3].fec_mode), TC_Cfg.line_param_a.fec_mode);

    // configuration traffic_type
    global_settings_failure |= GetJsonParamStr(TRAFFIC_TYPE_MEMBER,   SUPPORTED_TRAFFIC_TYPE, &(TC_Cfg.line_param_a.traffic_type),  TRAFFIC_TYPE_COUNT);
    global_settings_failure |= GetJsonParamStr(TRAFFIC_TYPE_0_MEMBER, SUPPORTED_TRAFFIC_TYPE, &(TC_Cfg.line_param[0].traffic_type), TC_Cfg.line_param_a.traffic_type);
    global_settings_failure |= GetJsonParamStr(TRAFFIC_TYPE_1_MEMBER, SUPPORTED_TRAFFIC_TYPE, &(TC_Cfg.line_param[1].traffic_type), TC_Cfg.line_param_a.traffic_type);
    global_settings_failure |= GetJsonParamStr(TRAFFIC_TYPE_2_MEMBER, SUPPORTED_TRAFFIC_TYPE, &(TC_Cfg.line_param[2].traffic_type), TC_Cfg.line_param_a.traffic_type);
    global_settings_failure |= GetJsonParamStr(TRAFFIC_TYPE_3_MEMBER, SUPPORTED_TRAFFIC_TYPE, &(TC_Cfg.line_param[3].traffic_type), TC_Cfg.line_param_a.traffic_type);

    // configuration packet_cfg
    global_settings_failure |= GetParamPacketCfg(PACKET_CFG_MEMBER,   MIN_PACKET_SIZE, NOM_PACKET_SIZE,                 MAX_PACKET_SIZE, &(TC_Cfg.line_param_a.packet_size),  &(TC_Cfg.line_param_a.packet_cfg),  PACKET_CFG_SWEEP);
    global_settings_failure |= GetParamPacketCfg(PACKET_CFG_0_MEMBER, MIN_PACKET_SIZE, TC_Cfg.line_param_a.packet_size, MAX_PACKET_SIZE, &(TC_Cfg.line_param[0].packet_size), &(TC_Cfg.line_param[0].packet_cfg), TC_Cfg.line_param_a.packet_cfg);
    global_settings_failure |= GetParamPacketCfg(PACKET_CFG_1_MEMBER, MIN_PACKET_SIZE, TC_Cfg.line_param_a.packet_size, MAX_PACKET_SIZE, &(TC_Cfg.line_param[1].packet_size), &(TC_Cfg.line_param[1].packet_cfg), TC_Cfg.line_param_a.packet_cfg);
    global_settings_failure |= GetParamPacketCfg(PACKET_CFG_2_MEMBER, MIN_PACKET_SIZE, TC_Cfg.line_param_a.packet_size, MAX_PACKET_SIZE, &(TC_Cfg.line_param[2].packet_size), &(TC_Cfg.line_param[2].packet_cfg), TC_Cfg.line_param_a.packet_cfg);
    global_settings_failure |= GetParamPacketCfg(PACKET_CFG_3_MEMBER, MIN_PACKET_SIZE, TC_Cfg.line_param_a.packet_size, MAX_PACKET_SIZE, &(TC_Cfg.line_param[3].packet_size), &(TC_Cfg.line_param[3].packet_cfg), TC_Cfg.line_param_a.packet_cfg);

    // configuration tx_mapping - no common value
    global_settings_failure |= GetJsonParamNum<uint32_t>(TX_MAPPING_0_MEMBER, MIN_TX_MAPPING, NOM_TX_MAPPING_0, MAX_TX_MAPPING, &(TC_Cfg.line_param[0].tx_mapping));
    global_settings_failure |= GetJsonParamNum<uint32_t>(TX_MAPPING_1_MEMBER, MIN_TX_MAPPING, NOM_TX_MAPPING_1, MAX_TX_MAPPING, &(TC_Cfg.line_param[1].tx_mapping));
    global_settings_failure |= GetJsonParamNum<uint32_t>(TX_MAPPING_2_MEMBER, MIN_TX_MAPPING, NOM_TX_MAPPING_2, MAX_TX_MAPPING, &(TC_Cfg.line_param[2].tx_mapping));
    global_settings_failure |= GetJsonParamNum<uint32_t>(TX_MAPPING_3_MEMBER, MIN_TX_MAPPING, NOM_TX_MAPPING_3, MAX_TX_MAPPING, &(TC_Cfg.line_param[3].tx_mapping));

    // configuration active mac
    global_settings_failure |= GetJsonParamBool(MATCH_TX_RX_MEMBER,   &(TC_Cfg.line_param_a.match_tx_rx),  false);
    global_settings_failure |= GetJsonParamBool(MATCH_TX_RX_0_MEMBER, &(TC_Cfg.line_param[0].match_tx_rx), TC_Cfg.line_param_a.match_tx_rx);
    global_settings_failure |= GetJsonParamBool(MATCH_TX_RX_1_MEMBER, &(TC_Cfg.line_param[1].match_tx_rx), TC_Cfg.line_param_a.match_tx_rx);
    global_settings_failure |= GetJsonParamBool(MATCH_TX_RX_2_MEMBER, &(TC_Cfg.line_param[2].match_tx_rx), TC_Cfg.line_param_a.match_tx_rx);
    global_settings_failure |= GetJsonParamBool(MATCH_TX_RX_3_MEMBER, &(TC_Cfg.line_param[3].match_tx_rx), TC_Cfg.line_param_a.match_tx_rx);

    // configuration gt_tx_diffctrl
    global_settings_failure |= GetJsonParamNum<uint32_t>(GT_TX_DIFFCTRL_MEMBER,   MIN_GT_TX_DIFFCTRL, NOM_GT_TX_DIFFCTRL,                 MAX_GT_TX_DIFFCTRL, &(TC_Cfg.line_param_a.gt_tx_diffctrl));
    global_settings_failure |= GetJsonParamNum<uint32_t>(GT_TX_DIFFCTRL_0_MEMBER, MIN_GT_TX_DIFFCTRL, TC_Cfg.line_param_a.gt_tx_diffctrl, MAX_GT_TX_DIFFCTRL, &(TC_Cfg.line_param[0].gt_tx_diffctrl));
    global_settings_failure |= GetJsonParamNum<uint32_t>(GT_TX_DIFFCTRL_1_MEMBER, MIN_GT_TX_DIFFCTRL, TC_Cfg.line_param_a.gt_tx_diffctrl, MAX_GT_TX_DIFFCTRL, &(TC_Cfg.line_param[1].gt_tx_diffctrl));
    global_settings_failure |= GetJsonParamNum<uint32_t>(GT_TX_DIFFCTRL_2_MEMBER, MIN_GT_TX_DIFFCTRL, TC_Cfg.line_param_a.gt_tx_diffctrl, MAX_GT_TX_DIFFCTRL, &(TC_Cfg.line_param[2].gt_tx_diffctrl));
    global_settings_failure |= GetJsonParamNum<uint32_t>(GT_TX_DIFFCTRL_3_MEMBER, MIN_GT_TX_DIFFCTRL, TC_Cfg.line_param_a.gt_tx_diffctrl, MAX_GT_TX_DIFFCTRL, &(TC_Cfg.line_param[3].gt_tx_diffctrl));

    // configuration gt_tx_pre_emph
    global_settings_failure |= GetJsonParamNum<uint32_t>(GT_TX_PRE_EMPH_MEMBER,   MIN_GT_TX_PRE_EMPH, NOM_GT_TX_PRE_EMPH,                 MAX_GT_TX_PRE_EMPH, &(TC_Cfg.line_param_a.gt_tx_pre_emph));
    global_settings_failure |= GetJsonParamNum<uint32_t>(GT_TX_PRE_EMPH_0_MEMBER, MIN_GT_TX_PRE_EMPH, TC_Cfg.line_param_a.gt_tx_pre_emph, MAX_GT_TX_PRE_EMPH, &(TC_Cfg.line_param[0].gt_tx_pre_emph));
    global_settings_failure |= GetJsonParamNum<uint32_t>(GT_TX_PRE_EMPH_1_MEMBER, MIN_GT_TX_PRE_EMPH, TC_Cfg.line_param_a.gt_tx_pre_emph, MAX_GT_TX_PRE_EMPH, &(TC_Cfg.line_param[1].gt_tx_pre_emph));
    global_settings_failure |= GetJsonParamNum<uint32_t>(GT_TX_PRE_EMPH_2_MEMBER, MIN_GT_TX_PRE_EMPH, TC_Cfg.line_param_a.gt_tx_pre_emph, MAX_GT_TX_PRE_EMPH, &(TC_Cfg.line_param[2].gt_tx_pre_emph));
    global_settings_failure |= GetJsonParamNum<uint32_t>(GT_TX_PRE_EMPH_3_MEMBER, MIN_GT_TX_PRE_EMPH, TC_Cfg.line_param_a.gt_tx_pre_emph, MAX_GT_TX_PRE_EMPH, &(TC_Cfg.line_param[3].gt_tx_pre_emph));

    // configuration gt_tx_post_emph
    global_settings_failure |= GetJsonParamNum<uint32_t>(GT_TX_POST_EMPH_MEMBER,   MIN_GT_TX_POST_EMPH, NOM_GT_TX_POST_EMPH,                 MAX_GT_TX_POST_EMPH, &(TC_Cfg.line_param_a.gt_tx_post_emph));
    global_settings_failure |= GetJsonParamNum<uint32_t>(GT_TX_POST_EMPH_0_MEMBER, MIN_GT_TX_POST_EMPH, TC_Cfg.line_param_a.gt_tx_post_emph, MAX_GT_TX_POST_EMPH, &(TC_Cfg.line_param[0].gt_tx_post_emph));
    global_settings_failure |= GetJsonParamNum<uint32_t>(GT_TX_POST_EMPH_1_MEMBER, MIN_GT_TX_POST_EMPH, TC_Cfg.line_param_a.gt_tx_post_emph, MAX_GT_TX_POST_EMPH, &(TC_Cfg.line_param[1].gt_tx_post_emph));
    global_settings_failure |= GetJsonParamNum<uint32_t>(GT_TX_POST_EMPH_2_MEMBER, MIN_GT_TX_POST_EMPH, TC_Cfg.line_param_a.gt_tx_post_emph, MAX_GT_TX_POST_EMPH, &(TC_Cfg.line_param[2].gt_tx_post_emph));
    global_settings_failure |= GetJsonParamNum<uint32_t>(GT_TX_POST_EMPH_3_MEMBER, MIN_GT_TX_POST_EMPH, TC_Cfg.line_param_a.gt_tx_post_emph, MAX_GT_TX_POST_EMPH, &(TC_Cfg.line_param[3].gt_tx_post_emph));

    // configuration traffic_type
    global_settings_failure |= GetJsonParamBool(GT_RX_USE_LPM_MEMBER,   &(TC_Cfg.line_param_a.gt_rx_use_lpm),  false);
    global_settings_failure |= GetJsonParamBool(GT_RX_USE_LPM_0_MEMBER, &(TC_Cfg.line_param[0].gt_rx_use_lpm), TC_Cfg.line_param_a.gt_rx_use_lpm);
    global_settings_failure |= GetJsonParamBool(GT_RX_USE_LPM_1_MEMBER, &(TC_Cfg.line_param[1].gt_rx_use_lpm), TC_Cfg.line_param_a.gt_rx_use_lpm);
    global_settings_failure |= GetJsonParamBool(GT_RX_USE_LPM_2_MEMBER, &(TC_Cfg.line_param[2].gt_rx_use_lpm), TC_Cfg.line_param_a.gt_rx_use_lpm);
    global_settings_failure |= GetJsonParamBool(GT_RX_USE_LPM_3_MEMBER, &(TC_Cfg.line_param[3].gt_rx_use_lpm), TC_Cfg.line_param_a.gt_rx_use_lpm);


    // measurement output file
    it = FindJsonParam(&(m_test_parameters.param), OUTPUT_FILE_MEMBER);
    if (it != m_test_parameters.param.end())
    {
        m_outputfile_name = TestcaseParamCast<std::string>(it->second);
        m_use_outputfile  = true;
        for(uint32_t n = 0; n < 4 ; n++)
        {
            global_settings_failure |= OpenOutputFile(m_outputfile_name + "_gt" + std::to_string(m_kernel_idx) + "_" + std::to_string(n) + ".csv", &(m_outputfile[n]) );
            WriteFirstOutputLine(n);
        }
    }
    if (global_settings_failure == true)
        m_abort = true;

    int thread_state = 1;
    bool parse_failure = false;

    if (m_abort == false)
    {
        LogMessage(LOG_INFO, "Test parameters:");
        LogMessage(LOG_INFO, "\t- " + std::string(TEST_SOURCE_MEMBER.name) + ": " + TC_Cfg.test_source);

        LogMessage(LOG_INFO, "\t- Line parameters");
        PrintLineParam(LOG_INFO, TC_Cfg.line_param_a);
        for(uint32_t n = 0; n < 4 ; n++)
        {
            LogMessage(LOG_INFO, "\t\t-tx_mapping " + std::to_string(n) + ":       " + std::to_string(TC_Cfg.line_param[n].tx_mapping));
        }

        for(uint32_t n = 0; n < 4 ; n++)
        {
            LogMessage(LOG_DEBUG, "\t-Line " + std::to_string(n) + " parameters");
            PrintLineParam(LOG_DEBUG, TC_Cfg.line_param[n]);
            LogMessage(LOG_DEBUG, "\t\t-tx_mapping:         " + std::to_string(TC_Cfg.line_param[n].tx_mapping));
        }

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
            m_abort = StartTestAndEnableWatchdog();
            if (m_abort == false)
            {
                // run thread async, block & wait for completion
                m_thread_future = std::async(std::launch::async, &GTMACTest::RunThread, this, TC_Cfg, &m_test_it_list);
                m_thread_future.wait();
                // check on completion if it has been aborted
                thread_state = m_thread_future.get();
                StopTestAndDisableWatchdog();
            }
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

bool GTMACTest::ParseTestSequenceSettings( GTMACTestcaseCfg_t TC_Cfg, std::list<TestItConfig_t> *test_list )
{

    bool parse_failure = false;
    uint  parse_error_cnt = 0;
    int  test_cnt = 0;
    TestItConfig_t test_it_cfg;

    std::vector<GTMAC_Test_Sequence_Parameters_t> test_sequence;
    Json_Parameters_t::iterator it = FindJsonParam(&(m_test_parameters.param), TEST_SEQUENCE_MEMBER);
    if (it != m_test_parameters.param.end())
        test_sequence = TestcaseParamCast<std::vector<GTMAC_Test_Sequence_Parameters_t>>(it->second);

    for (auto test_seq_param : test_sequence)
    {
        if (m_abort == true) break;
        test_cnt ++;
        bool parse_it_failure = false;

        if (parse_it_failure == false)
        {
            parse_it_failure |= CheckParam<uint>("duration", test_seq_param.duration, MIN_DURATION, MAX_DURATION);
            test_it_cfg.duration = test_seq_param.duration;
        }
        std::string gt_mac_sequence_param;
        if (parse_it_failure == false)
        {
            parse_it_failure |= CheckStringInSet(test_seq_param.mode, SUPPORTED_GT_MAC_SEQUENCE_PARAM_TYPE);
            gt_mac_sequence_param = test_seq_param.mode;
            test_it_cfg.status = 0;
            test_it_cfg.clr_stat = 0;
            test_it_cfg.conf = 0;
            test_it_cfg.run = 0;
            if (StrMatchNoCase(gt_mac_sequence_param, GT_MAC_SEQUENCE_PARAM_STATUS))
                test_it_cfg.status = 1;
            else if (StrMatchNoCase(gt_mac_sequence_param, GT_MAC_SEQUENCE_PARAM_CLR_STAT))
                test_it_cfg.clr_stat = 1;
            else if (StrMatchNoCase(gt_mac_sequence_param, GT_MAC_SEQUENCE_PARAM_CONF))
                test_it_cfg.conf = 1;
            else if (StrMatchNoCase(gt_mac_sequence_param, GT_MAC_SEQUENCE_PARAM_RUN))
                test_it_cfg.run = 1;
        }

        parse_failure |= parse_it_failure;

        if (parse_it_failure == true)
        {
            LogMessage(LOG_FAILURE, "Test "+ std::to_string(test_cnt) + ": invalid parameters" );
            parse_error_cnt ++;
            if (parse_error_cnt > MAX_NUM_PARSER_ERROR) break;
        }
        else
        {
            test_list->push_back(test_it_cfg);
            std::string params = "";
            params += std::to_string(test_it_cfg.duration) + ", ";
            params += "\"" + gt_mac_sequence_param + "\"";
            LogMessage(LOG_DEBUG, "Test " + std::to_string(test_cnt) + " parameters: " + params);
        }

    }
    return parse_failure;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool GTMACTest::GetParamPacketCfg ( Json_Val_Def_t json_val_def, uint32_t param_min, uint32_t param_nom, uint32_t param_max, uint32_t *param, std::string *param_cfg, std::string param_cfg_default )
{
    bool test_failure = false;
    Json_Parameters_t::iterator it;

    *param      = param_nom; // Default value
    *param_cfg  = param_cfg_default; // Default value

    it = FindJsonParam(&(m_test_parameters.param), json_val_def);
    if (it != m_test_parameters.param.end())
    {
        std::string param_cfg_str = TestcaseParamCast<std::string>(it->second);
        if (StrMatchNoCase(param_cfg_str, PACKET_CFG_SWEEP) == false)
        {
            *param_cfg = PACKET_CFG_NO_SWEEP;
            test_failure = ConvertStringToNum<uint32_t> (json_val_def.name, param_cfg_str, param);
            if (test_failure == true) return true;
            test_failure = CheckParam<uint32_t> (json_val_def.name, *param, param_min, param_max);
            if (test_failure == true) return true;
        }
        else
        {
            *param_cfg = PACKET_CFG_SWEEP;
        }
    }
    else
    {
        if (json_val_def.hidden == HIDDEN_FALSE)
            LogMessage(LOG_INFO, "Setting to default " + std::string(json_val_def.name) + ": " + std::to_string(*param));
    }
    return test_failure;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool GTMACTest::LineRateParam2Setting ( std::string param, bool *setting )
{
    bool ret_failure = false;

    if      (StrMatchNoCase(param, LINE_RATE_10GBE))    *setting = true;
    else if (StrMatchNoCase(param, LINE_RATE_25GBE))    *setting = false;
    else ret_failure = true;

    if (ret_failure == true)
        LogMessage(LOG_FAILURE, "Unknown Line rate parameter: " + param);
    return ret_failure;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool GTMACTest::FECModeParam2Setting ( std::string param, FEC_Mode_t *setting )
{
    bool ret_failure = false;

    if      (StrMatchNoCase(param, FEC_MODE_NONE))      *setting = FM_NONE;
    else if (StrMatchNoCase(param, FEC_MODE_CLAUSE_74)) *setting = FM_CL74;
    else if (StrMatchNoCase(param, FEC_MODE_RS_FEC))    *setting = FM_RS;
    else ret_failure = true;

    if (ret_failure == true)
        LogMessage(LOG_FAILURE, "Unknown FEC mode parameter: " + param);
    return ret_failure;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool GTMACTest::TrafficTypeParam2Setting ( std::string param, Traffic_Type_t *setting )
{
    bool ret_failure = false;

    if      (StrMatchNoCase(param, TRAFFIC_TYPE_0X00))      *setting = TT_ZERO;
    else if (StrMatchNoCase(param, TRAFFIC_TYPE_0XFF))      *setting = TT_FF;
    else if (StrMatchNoCase(param, TRAFFIC_TYPE_COUNT))     *setting = TT_CNT;
    else if (StrMatchNoCase(param, TRAFFIC_TYPE_PATTERN))   *setting = TT_4CYC;
    else ret_failure = true;

    if (ret_failure == true)
        LogMessage(LOG_FAILURE, "Unknown traffic type parameter: " + param);
    return ret_failure;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool GTMACTest::PacketCfgParam2Setting ( std::string param, bool *setting )
{
    bool ret_failure = false;

    if      (StrMatchNoCase(param, PACKET_CFG_SWEEP))       *setting = true;
    else if (StrMatchNoCase(param, PACKET_CFG_NO_SWEEP))    *setting = false;
    else ret_failure = true;

    if (ret_failure == true)
        LogMessage(LOG_FAILURE, "Unknown packet configuration parameter: " + param);
    return ret_failure;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void GTMACTest::PrintLineParam ( LogLevel Level, LineParam_t line_param )
{
    LogMessage(Level, "\t\t-" + ACTIVE_MAC_MEMBER.name      + ": " + BoolToStr(line_param.active_mac));
    LogMessage(Level, "\t\t-" + LINE_RATE_MEMBER.name       + ": " + line_param.line_rate);
    LogMessage(Level, "\t\t-" + UTILISATION_MEMBER.name     + ": " + std::to_string(line_param.utilisation));
    LogMessage(Level, "\t\t-" + SET_TEST_PAT_MEMBER.name    + ": " + BoolToStr(line_param.set_test_pat));
    LogMessage(Level, "\t\t-" + FEC_MODE_MEMBER.name        + ": " + line_param.fec_mode);
    LogMessage(Level, "\t\t-" + TRAFFIC_TYPE_MEMBER.name    + ": " + line_param.traffic_type);
    if (StrMatchNoCase(line_param.packet_cfg, PACKET_CFG_SWEEP) == false)
        LogMessage(Level, "\t\t-" + PACKET_CFG_MEMBER.name  + ": " + std::to_string(line_param.packet_size));
    else
        LogMessage(Level, "\t\t-" + PACKET_CFG_MEMBER.name  + ": " + line_param.packet_cfg);
    LogMessage(Level, "\t\t-" + MATCH_TX_RX_MEMBER.name     + ": " + BoolToStr(line_param.match_tx_rx));
    LogMessage(Level, "\t\t-" + GT_TX_DIFFCTRL_MEMBER.name  + ": " + std::to_string(line_param.gt_tx_diffctrl));
    LogMessage(Level, "\t\t-" + GT_TX_PRE_EMPH_MEMBER.name  + ": " + std::to_string(line_param.gt_tx_pre_emph));
    LogMessage(Level, "\t\t-" + GT_TX_POST_EMPH_MEMBER.name + ": " + std::to_string(line_param.gt_tx_post_emph));
    LogMessage(Level, "\t\t-" + GT_RX_USE_LPM_MEMBER.name    + ": " + BoolToStr(line_param.gt_rx_use_lpm));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void GTMACTest::WriteOutputLine ( uint32_t line, bool test_failure, bool test_it_failure, uint32_t *read_buffer )
{
    if (m_use_outputfile == true)
    {
        if (test_failure == true)
            m_outputfile[line] << "FAIL,";
        else
            m_outputfile[line] << "PASS,";
        if (test_it_failure == true)
            m_outputfile[line] << "FAIL,";
        else
            m_outputfile[line] << "PASS,";

        for (uint32_t n = 0; n < MAC_NUM_STATS; n++) {
            uint64_t Stats_var = READ_BUFFER_64(read_buffer, n);
            m_outputfile[line] << std::to_string( Stats_var )   << ",";
        }
        m_outputfile[line] << "\n";
        m_outputfile[line].flush();
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void GTMACTest::WriteFirstOutputLine ( uint32_t line )
{
    if (m_use_outputfile == true)
    {
        m_outputfile[line] << "Overall result,";
        m_outputfile[line] << "Test result,";
        for (uint32_t n = 0; n < MAC_NUM_STATS; n++) {
            // Remove blank spaces after name
            std::string name = MAC_STAT_NAMES[n];
            std::string name_nopad = "";
            bool end_param = false;
            for ( std::string::reverse_iterator rit=name.rbegin(); rit!=name.rend(); ++rit)
            {
                if (*rit != ' ')
                    end_param = true;
                if (end_param == true)
                    name_nopad = *rit + name_nopad;
            }
            m_outputfile[line] << name_nopad << ",";
        }
        m_outputfile[line] << "\n";
        m_outputfile[line].flush();
    }
}
