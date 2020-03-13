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


#ifndef _GTTEST_H
#define _GTTEST_H

#include "xbtestcommon.h"
#include "logging.h"
#include "deviceinterface.h"
#include "testinterface.h"
#include "devicemgt.h"

class GTTest : public TestInterface
{

public:

	GTTest( DeviceInterface *device, Testcase_Parameters_t test_parameters, int kernel_idx, Global_Config_t global_config );
	~GTTest();

    // implement virtual inherited functions
    bool PreSetup();
    void Run();
    void PostTeardown();
    void Abort();

private:

    // generic
    std::atomic<bool> m_abort;
    std::future<int> m_thread_future;
    DeviceInterface *m_device;
    DeviceMgt *m_devicemgt = nullptr;

    int         m_kernel_idx;

    std::string m_outputfile_name;
    bool m_use_outputfile       = false;
    std::ofstream m_outputfile;

    typedef struct GTTestcaseCfg_t
    {
        std::string test_source;
        std::string gt_loopback;

        std::string rx_reverse;
        std::string tx_reverse;
        std::string tx_phase;
        std::string scramb_dis;
        std::string retime_dis;
        std::string align_dis;

        std::string gt_rx_pol;
        std::string gt_tx_pol;

    } GTTestcaseCfg_t;

    GTTestcaseCfg_t m_Testcase_Cfg;

    typedef struct TestItConfig_t
    {
        int target_GT;
    } TestItConfig_t;

    std::list<TestItConfig_t> m_test_it_list;

    int RunThread(GTTestcaseCfg_t TC_Cfg, std::list<TestItConfig_t> *Tests_list);

    bool ParseTestSequenceSettings( GTTestcaseCfg_t TC_Cfg, std::list<TestItConfig_t> *test_list );

    const std::string RAW_LOOPBACK_OFF  = "off";
    const std::string RAW_LOOPBACK_ON   = "on";
    const std::set<std::string> TEST_SUPPORTED_RAW_LOOPBACK = {
        RAW_LOOPBACK_OFF,
        RAW_LOOPBACK_ON
    };

    const std::string GT_LOOPBACK_OFF      = "off";
    const std::string GT_LOOPBACK_NE_PCS   = "ne_pcs";
    const std::string GT_LOOPBACK_NE_PMA   = "ne_pma";
    const std::string GT_LOOPBACK_FE_PMA   = "fe_pma";
    const std::string GT_LOOPBACK_FE_PCS   = "fe_pcs";
    const std::set<std::string> TEST_SUPPORTED_GT_LOOPBACK = {
        GT_LOOPBACK_OFF,
        GT_LOOPBACK_NE_PCS,
        GT_LOOPBACK_NE_PMA,
        GT_LOOPBACK_FE_PMA,
        GT_LOOPBACK_FE_PCS
    };

    const std::set<std::string> TEST_SUPPORTED_GT_POL = {
        "0_0_0_0",
        "0_0_0_1",
        "0_0_1_0",
        "0_0_1_1",
        "0_1_0_0",
        "0_1_0_1",
        "0_1_1_0",
        "0_1_1_1",
        "1_0_0_0",
        "1_0_0_1",
        "1_0_1_0",
        "1_0_1_1",
        "1_1_0_0",
        "1_1_0_1",
        "1_1_1_0",
        "1_1_1_1"
    };

    const std::string SET_OFF  = "off";
    const std::string SET_ON   = "on";
    const std::set<std::string> TEST_SUPPORTED_ON_OFF = {
        SET_OFF,
        SET_ON
    };


    const uint CTRL_GT_CFG_REG_ADDR = 0x10;

    const uint CTRL_GT_LOOPBACK_OFF      = 0x00000000; //0b000 => reg = 0x00000000
    const uint CTRL_GT_LOOPBACK_NE_PCS   = 0x00000001; //0b001 => reg = 0x00000249
    const uint CTRL_GT_LOOPBACK_NE_PMA   = 0x00000002; //0b010 => reg = 0x00000492
    const uint CTRL_GT_LOOPBACK_FE_PMA   = 0x00000004; //0b100 => reg = 0x00000924
    const uint CTRL_GT_LOOPBACK_FE_PCS   = 0x00000006; //0b110 => reg = 0x00000DB6
    const uint CTRL_GT_LOOPBACK_MASK     = 0x00000FFF; //MASK

    const uint CTRL_GT_RX_POL_MASK       = 0x00F00000; //MASK
    const uint CTRL_GT_TX_POL_MASK       = 0x0F000000; //MASK

    //const uint CTRL_RAW_LOOPBACK     = (0x1 << 16);

    const uint CTRL_RX_REVERSE       = (0x1 << 17);
    const uint CTRL_TX_REVERSE       = (0x1 << 18);
    const uint CTRL_TX_PHASE         = (0x1 << 19);
    const uint CTRL_SCRAMB_DIS       = (0x1 << 20);
    const uint CTRL_RETIME_DIS       = (0x1 << 21);
    const uint CTRL_ALIGN_DIS        = (0x1 << 22);


    const uint CTRL_GT_GTRST_REG_ADDR = 0x12;
    const uint CTRL_GT_RESET         = (0x1 << 0);


    uint ReadGTKernel(uint address);
    void WriteGTKernel(uint address, uint value);

    uint PolarityCfg(std::string pol);
    void SetGTCfg(GTTestcaseCfg_t TC_Cfg);
    void ResetGT();

};

#endif /* _GTTEST_H */
