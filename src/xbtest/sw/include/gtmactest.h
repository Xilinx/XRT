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


#ifndef _GTMACTEST_H
#define _GTMACTEST_H

#include "xbtestcommon.h"
#include "logging.h"
#include "deviceinterface.h"
#include "testinterface.h"
#include "devicemgt.h"

class GTMACTest : public TestInterface
{

public:

	GTMACTest( Xbtest_Pfm_Def_t xbtest_pfm_def, DeviceInterface *device, DeviceMgt *device_mgt, Testcase_Parameters_t test_parameters, int kernel_idx, Global_Config_t global_config );
	~GTMACTest();

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

    // generic
    std::atomic<bool> m_abort;
    std::future<int> m_thread_future;

    int m_kernel_idx;

    std::string m_outputfile_name;
    bool m_use_outputfile       = false;
    std::ofstream m_outputfile[4];


    typedef struct LineParam_t
    {
        bool        active_mac;
        std::string line_rate;
        uint32_t    utilisation;
        bool        set_test_pat;
        std::string fec_mode;
        std::string traffic_type;
        uint32_t    packet_size;
        std::string packet_cfg;
        uint32_t    tx_mapping;
        bool        match_tx_rx;
        uint32_t    gt_tx_diffctrl;
        uint32_t    gt_tx_pre_emph;
        uint32_t    gt_tx_post_emph;
        bool        gt_rx_use_lpm;
    } LineParam_t;

    typedef struct GTMACTestcaseCfg_t
    {
        std::string test_source;

        LineParam_t line_param_a;
        LineParam_t line_param[4];

    } GTMACTestcaseCfg_t;

    GTMACTestcaseCfg_t m_Testcase_Cfg;

    typedef struct TestItConfig_t
    {
        int target_GT_MAC;
        uint duration;
        int status;
        int clr_stat;
        int conf;
        int run;
        int Speed;
        uint64_t    Src_MAC;
        uint64_t    Dest_MAC;

    } TestItConfig_t;

    typedef enum {FM_NONE, FM_CL74, FM_RS } FEC_Mode_t;
    typedef enum {TT_ZERO = 0, TT_CNT = 1, TT_4CYC = 2, TT_FF = 3 } Traffic_Type_t;

    typedef struct
    {
        bool            Active;
        uint64_t        Dest_Addr;
        uint64_t        Source_Addr;
        uint32_t        EtherType;
        uint32_t        IPG;
        FEC_Mode_t      FEC_Mode;
        bool            Rate_10;
        Traffic_Type_t  Traffic_Type;
        bool            Set_LFI;
        bool            Set_RFI;
        bool            Set_Idle;
        bool            Set_Test_Pat;
        bool            Lcl_Loopback;
        uint32_t        Script_Base;
        uint32_t        Script_Length;
        uint32_t        Packet_Size;
        bool            Packet_Sweep;
        float           Utilisation;
        bool            Match_Tx_Rx;
        uint32_t        MTU;
        uint32_t        GT_Tx_Diff;     // Range 0..31, default = 11                control = 'gt_tx_diffctrl'
        uint32_t        GT_Tx_Pre;      // Range 0..31, default = 0                 control = 'gt_tx_pre_emph'
        uint32_t        GT_Tx_Post;     // Range 0..31, default = 0                 control = 'gt_tx_post_emph'
        bool            GT_Rx_Eq_Sel;   // false = DFE, true = LPM, default = DFE   control = 'gt_rx_use_lpm'
    } MAC_Config_t;


    #define READ_BUFFER_64(Buffer, Idx_64)   ((uint64_t(Buffer[(Idx_64 * 2)+1]) << 32) | uint64_t(Buffer[Idx_64 * 2]))

    #define MAC_NUM_STATS           (43)
    #define MAC_IDX_RX_GOOD_PAC     ( 7)
    #define MAC_IDX_RX_GOOD_BYTE    ( 9)
    #define MAC_IDX_TX_SENT_PAC     (41)
    #define MAC_IDX_TX_SENT_BYTE    (42)

    const std::string   MAC_STAT_NAMES[MAC_NUM_STATS] =  {
        "CYCLE_COUNT                ",  //  0
        "RX_FRAMING_ERR             ",  //  1
        "RX_BAD_CODE                ",  //  2
        "RX_RSFEC_CORRECTED_CW_INC  ",  //  3
        "RX_RSFEC_UNCORRECTED_CW_INC",  //  4
        "RX_RSFEC_ERR_COUNT0_INC    ",  //  5
        "RX_TOTAL_PACKETS           ",  //  6
        "RX_TOTAL_GOOD_PACKETS      ",  //  7
        "RX_TOTAL_BYTES             ",  //  8
        "RX_TOTAL_GOOD_BYTES        ",  //  9
        "RX_PACKET_64_BYTES         ",  // 10
        "RX_PACKET_65_127_BYTES     ",  // 11
        "RX_PACKET_128_255_BYTES    ",  // 12
        "RX_PACKET_256_511          ",  // 13
        "RX_PACKET_512_1023_BYTES   ",  // 14
        "RX_PACKET_1024_1518_BYTES  ",  // 15
        "RX_PACKET_1519_1522_BYTES  ",  // 16
        "RX_PACKET_1523_1548_BYTES  ",  // 17
        "RX_PACKET_1549_2047_BYTES  ",  // 18
        "RX_PACKET_2048_4095_BYTES  ",  // 19
        "RX_PACKET_4096_8191_BYTES  ",  // 20
        "RX_PACKET_8192_9215_BYTES  ",  // 21
        "RX_PACKET_LARGE            ",  // 22
        "RX_PACKET_SMALL            ",  // 23
        "RX_UNDERSIZE               ",  // 24
        "RX_FRAGMENT                ",  // 25
        "RX_OVERSIZE                ",  // 26
        "RX_TOOLONG                 ",  // 27
        "RX_JABBER                  ",  // 28
        "RX_BAD_FCS                 ",  // 29
        "RX_PACKET_BAD_FCS          ",  // 30
        "RX_STOMPED_FCS             ",  // 31
        "RX_UNICAST                 ",  // 32
        "RX_MULTICAST               ",  // 33
        "RX_BROADCAST               ",  // 34
        "RX_VLAN                    ",  // 35
        "RX_INRANGEERR              ",  // 36
        "RX_TRUNCATED               ",  // 37
        "RX_TEST_PATTERN_MISMATCH   ",  // 38
        "FEC_INC_CORRECT_COUNT      ",  // 39
        "FEC_INC_CANT_CORRECT_COUNT ",  // 40
        "TX_TOTAL_PACKETS           ",  // 41
        "TX_TOTAL_BYTES             "}; // 42

    const bool   MAC_STAT_ERR_TRIG[MAC_NUM_STATS] =  {
        FALSE,  // "CYCLE_COUNT                ",
        TRUE,   // "RX_FRAMING_ERR             ",
        TRUE,   // "RX_BAD_CODE                ",
        TRUE,   // "RX_RSFEC_CORRECTED_CW_INC  ",
        TRUE,   // "RX_RSFEC_UNCORRECTED_CW_INC",
        TRUE,   // "RX_RSFEC_ERR_COUNT0_INC    ",
        FALSE,  // "RX_TOTAL_PACKETS           ",
        FALSE,  // "RX_TOTAL_GOOD_PACKETS      ",
        FALSE,  // "RX_TOTAL_BYTES             ",
        FALSE,  // "RX_TOTAL_GOOD_BYTES        ",
        FALSE,  // "RX_PACKET_64_BYTES         ",
        FALSE,  // "RX_PACKET_65_127_BYTES     ",
        FALSE,  // "RX_PACKET_128_255_BYTES    ",
        FALSE,  // "RX_PACKET_256_511          ",
        FALSE,  // "RX_PACKET_512_1023_BYTES   ",
        FALSE,  // "RX_PACKET_1024_1518_BYTES  ",
        FALSE,  // "RX_PACKET_1519_1522_BYTES  ",
        FALSE,  // "RX_PACKET_1523_1548_BYTES  ",
        FALSE,  // "RX_PACKET_1549_2047_BYTES  ",
        FALSE,  // "RX_PACKET_2048_4095_BYTES  ",
        FALSE,  // "RX_PACKET_4096_8191_BYTES  ",
        FALSE,  // "RX_PACKET_8192_9215_BYTES  ",
        FALSE,  // "RX_PACKET_LARGE            ",
        TRUE,   // "RX_PACKET_SMALL            ",
        TRUE,   // "RX_UNDERSIZE               ",
        TRUE,   // "RX_FRAGMENT                ",
        TRUE,   // "RX_OVERSIZE                ",
        TRUE,   // "RX_TOOLONG                 ",
        TRUE,   // "RX_JABBER                  ",
        TRUE,   // "RX_BAD_FCS                 ",
        TRUE,   // "RX_PACKET_BAD_FCS          ",
        TRUE,   // "RX_STOMPED_FCS             ",
        FALSE,  // "RX_UNICAST                 ",
        FALSE,  // "RX_MULTICAST               ",
        FALSE,  // "RX_BROADCAST               ",
        FALSE,  // "RX_VLAN                    ",
        TRUE,   // "RX_INRANGEERR              ",
        TRUE,   // "RX_TRUNCATED               ",
        TRUE,   // "RX_TEST_PATTERN_MISMATCH   ",
        TRUE,   // "FEC_INC_CORRECT_COUNT      ",
        TRUE,   // "FEC_INC_CANT_CORRECT_COUNT ",
        FALSE,  // "TX_TOTAL_PACKETS           ",
        FALSE}; // "TX_TOTAL_BYTES             "};


    std::list<TestItConfig_t> m_test_it_list;

    int RunThread(GTMACTestcaseCfg_t TC_Cfg, std::list<TestItConfig_t> *Tests_list);

    bool ParseTestSequenceSettings( GTMACTestcaseCfg_t TC_Cfg, std::list<TestItConfig_t> *test_list );

    const uint MIN_DURATION = 1;
    const uint MAX_DURATION = MAX_UINT_VAL;

    const std::string GT_MAC_SEQUENCE_PARAM_STATUS    = "status";
    const std::string GT_MAC_SEQUENCE_PARAM_CLR_STAT  = "clr_stat";
    const std::string GT_MAC_SEQUENCE_PARAM_CONF      = "conf";
    const std::string GT_MAC_SEQUENCE_PARAM_RUN       = "run";
    const std::set<std::string> SUPPORTED_GT_MAC_SEQUENCE_PARAM_TYPE = {
        GT_MAC_SEQUENCE_PARAM_STATUS,
        GT_MAC_SEQUENCE_PARAM_CLR_STAT,
        GT_MAC_SEQUENCE_PARAM_CONF,
        GT_MAC_SEQUENCE_PARAM_RUN
    };

    // JSON parameters
    const std::string LINE_RATE_10GBE = "10gbe";
    const std::string LINE_RATE_25GBE = "25gbe";
    const std::set<std::string> SUPPORTED_LINE_RATE = {
        LINE_RATE_10GBE,
        LINE_RATE_25GBE
    };

    const uint32_t MIN_UTILISATION = 0;
    const uint32_t NOM_UTILISATION = 50;
    const uint32_t MAX_UTILISATION = 100;

    const std::string SET_TEST_PAT_TRUE  = BOOL_TRUE_STR;
    const std::string SET_TEST_PAT_FALSE = BOOL_FALSE_STR;
    const std::set<std::string> SUPPORTED_SET_TEST_PAT = {
        SET_TEST_PAT_TRUE,
        SET_TEST_PAT_FALSE
    };

    const std::string FEC_MODE_NONE         = "none";
    const std::string FEC_MODE_CLAUSE_74    = "clause_74";
    const std::string FEC_MODE_RS_FEC       = "rs_fec";
    const std::set<std::string> SUPPORTED_FEC_MODE = {
        FEC_MODE_NONE,
        FEC_MODE_CLAUSE_74,
        FEC_MODE_RS_FEC
    };

    const std::string TRAFFIC_TYPE_0X00     = "0x00";
    const std::string TRAFFIC_TYPE_0XFF     = "0xff";
    const std::string TRAFFIC_TYPE_COUNT    = "count";
    const std::string TRAFFIC_TYPE_PATTERN  = "pattern";
    const std::set<std::string> SUPPORTED_TRAFFIC_TYPE = {
        TRAFFIC_TYPE_0X00,
        TRAFFIC_TYPE_0XFF,
        TRAFFIC_TYPE_COUNT,
        TRAFFIC_TYPE_PATTERN
    };

    const std::string PACKET_CFG_SWEEP      = "sweep";
    const std::string PACKET_CFG_NO_SWEEP   = "no_sweep";

    const uint32_t MIN_PACKET_SIZE = 64;
    const uint32_t NOM_PACKET_SIZE = 64;
    const uint32_t MAX_PACKET_SIZE = 10011;

    const uint32_t MIN_TX_MAPPING = 0;
    const uint32_t MAX_TX_MAPPING = 3;

    const uint32_t NOM_TX_MAPPING_0 = 0;
    const uint32_t NOM_TX_MAPPING_1 = 1;
    const uint32_t NOM_TX_MAPPING_2 = 2;
    const uint32_t NOM_TX_MAPPING_3 = 3;

    const uint32_t MIN_GT_TX_DIFFCTRL = 0;
    const uint32_t NOM_GT_TX_DIFFCTRL = 11;
    const uint32_t MAX_GT_TX_DIFFCTRL = 31;

    const uint32_t MIN_GT_TX_PRE_EMPH = 0;
    const uint32_t NOM_GT_TX_PRE_EMPH = 0;
    const uint32_t MAX_GT_TX_PRE_EMPH = 31;

    const uint32_t MIN_GT_TX_POST_EMPH = 0;
    const uint32_t NOM_GT_TX_POST_EMPH = 0;
    const uint32_t MAX_GT_TX_POST_EMPH = 31;

    uint32_t CalcScript(MAC_Config_t *Conf, uint32_t Packet_Size, float Utilisation);
    uint32_t ParseMACStatus( uint32_t *read_buffer_rx, uint rx_idx, uint32_t *read_buffer_tx, uint tx_idx, bool Check_Tx_Rx );
    void WriteGTMACCmd(int status, int conf, int run);

    bool GetParamPacketCfg ( Json_Val_Def_t json_val_def, uint32_t param_min, uint32_t param_nom, uint32_t param_max, uint32_t *param, std::string *param_cfg, std::string param_cfg_default );

    bool LineRateParam2Setting ( std::string param, bool *setting );
    bool FECModeParam2Setting ( std::string param, FEC_Mode_t *setting );
    bool TrafficTypeParam2Setting ( std::string param, Traffic_Type_t *setting );
    bool PacketCfgParam2Setting ( std::string param, bool *setting );

    void PrintLineParam ( LogLevel Level, LineParam_t line_param );

    void WriteOutputLine ( uint32_t line, bool test_failure, bool test_it_failure, uint32_t *read_buffer );
    void WriteFirstOutputLine ( uint32_t line );

    void ResetWatchdog();
    bool StartTestAndEnableWatchdog();
    bool StopTestAndDisableWatchdog();

};

#endif /* _GTMACTEST_H */
