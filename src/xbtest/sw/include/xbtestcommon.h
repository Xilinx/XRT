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

#ifndef _XBTESTCOMMON_H
#define _XBTESTCOMMON_H

#include <iostream>
#include <iomanip>
#include <stdio.h>
#include <csignal>
#include <ctype.h>
#include <cmath>
#include <unistd.h>

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <string>

#include <atomic>
#include <mutex>
#include <future>
#include <thread>
#include <chrono>
#include <time.h>

#include <set>
#include <map>
#include <vector>
#include <list>

#include <glib-object.h>
#include <json-glib/json-glib.h>

#include "xclhal2.h"
#include "xcl2.h"
#include <sys/mman.h>

const bool RET_FAILURE = true;
const bool RET_SUCCESS = false;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


const int XBTEST_VERSION_MAJOR  = 3;
const int XBTEST_VERSION_MINOR  = 3;
const int XBTEST_VERSION_PATCH  = 0;
const std::string XBTEST_VERSION_STR = std::to_string(XBTEST_VERSION_MAJOR) + "." + std::to_string(XBTEST_VERSION_MINOR) + "." + std::to_string(XBTEST_VERSION_PATCH);

const int DMA_SW_VERSION_MAJOR  = 2;
const int DMA_SW_VERSION_MINOR  = 2;
const int DMA_SW_COMPONENT_ID   = -1; // Not applicable as it's purely SW

const int BI_PWR_HW_VERSION_MAJOR  = 1;
const int BI_PWR_HW_VERSION_MINOR  = 5;
const int BI_PWR_HW_COMPONENT_ID   = 0;

const int BI_MEM_HW_VERSION_MAJOR  = 1;
const int BI_MEM_HW_VERSION_MINOR  = 4;
const int BI_MEM_HW_COMPONENT_ID   = 1;

const int BI_GT_HW_VERSION_MAJOR    = 1;
const int BI_GT_HW_VERSION_MINOR    = 0;
const int BI_GT_10_HW_COMPONENT_ID  = 2;
const int BI_GT_25_HW_COMPONENT_ID  = 3;

const int BI_GT_MAC_HW_VERSION_MAJOR = 1;
const int BI_GT_MAC_HW_VERSION_MINOR = 5;
const int BI_GT_MAC_HW_COMPONENT_ID  = 4;

const uint BI_MAJOR_MINOR_VERSION_ADDR        = 0x0000;
const uint BI_PERFORCE_VERSION_ADDR           = 0x0001;
const uint BI_COMPONENT_ID_ADDR               = 0x0002;
const uint BI_RESERVED                        = 0x0003;
const uint BI_INFO_1_2_ADDR                   = 0x0004;
const uint BI_INFO_3_4_ADDR                   = 0x0005;
const uint BI_INFO_5_6_ADDR                   = 0x0006;
const uint BI_INFO_7_8_ADDR                   = 0x0007;

const uint CMN_SCRATCH_PAD_ADDR               = 0x0010;
const uint CMN_RESET_DETECTION_ADDR           = 0x0011;
const uint CMN_CTRL_STATUS_ADDR               = 0x0012;
const uint CMN_WATCHDOG_ADDR                  = 0x0013;


const uint CMN_STATUS_START                   = (0x1 << 0);
const uint CMN_STATUS_ALREADY_START           = (0x1 << 4);

const uint CMN_WATCHDOG_EN                    = (0x1 << 4);
const uint CMN_WATCHDOG_ALARM                 = (0x1 << 8);
const uint CMN_WATCHDOG_RST                   = (0x1 << 12);


const int SW_PERFORCE_VERSION    = PERFORCE_VERSION; // Preprocessor marco defined at compilation time
#define preprop_xstr(s) preprop_str(s)
#define preprop_str(s) #s
const std::string SW_BUILD_DATE  = preprop_xstr(BUILD_DATE); // Preprocessor marco defined at compilation time

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Parameter required or not (error message when not found in JSON)
enum Required_t {
    REQUIRED_TRUE,
    REQUIRED_FALSE
};
// Parameter hidden (no message displayed) or visible
enum Hidden_t {
    HIDDEN_TRUE,
    HIDDEN_FALSE
};
// Type IDs of JSON parameter values.
// TYPE_ID_TEST_SEQUENCE is used for testcase TEST_SEQUENCE array of a structure different for each testcase
enum Type_ID_t {
    TYPE_ID_INT,
    TYPE_ID_UINT,
    TYPE_ID_UINT64_T,
    TYPE_ID_FLOAT,
    TYPE_ID_DOUBLE,
    TYPE_ID_BOOL,
    TYPE_ID_STRING,
    TYPE_ID_TEST_SEQUENCE
};
// Structure defining a JSON parameter value
struct Json_Val_Def_t {
    Required_t      required;
    Hidden_t        hidden;
    JsonNodeType    node_type;
    Type_ID_t       typeId;
    std::string     name;
};
using Json_Params_Def_t = std::vector<Json_Val_Def_t>;

// Define structure containing parsed JSON parameters
//TestcaseParamBase is a base structure that will enable to build a std::map with different parameter types
class TestcaseParamBase
{
public:
    virtual ~TestcaseParamBase() = 0;
};
inline TestcaseParamBase::~TestcaseParamBase() {}
// TestcaseParam will be use to cast TestcaseParamBase to the correct type of JSON parameter
template<class T>
class TestcaseParam : public TestcaseParamBase
{
public:
    typedef T Type;
    explicit TestcaseParam(const Type& data) : data(data) {}
    TestcaseParam() {}
    Type data;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T> inline T TestcaseParamCast ( TestcaseParamBase *teacase_param ) {
    return dynamic_cast<TestcaseParam<T>&>(*teacase_param).data;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using Json_Parameter_t  = std::pair<std::string, TestcaseParamBase*>;
using Json_Parameters_t = std::map<std::string, TestcaseParamBase*>;

typedef struct
{
    bool                test_exists;
    Json_Parameters_t   param;
} Testcase_Parameters_t;

// Structure of test sequence for each testcase
typedef struct
{
    uint        duration;
    std::string mem_type;
    uint        mem_index;
    uint64_t    buffer_size;
    std::string test_sequence_mode;
} DMA_Test_Sequence_Parameters_t;

typedef struct
{
    std::string test_mode;
    uint        duration;
    uint64_t    wr_start_addr;
    uint        wr_burst_size;
    uint        wr_num_xfer;
    uint64_t    rd_start_addr;
    uint        rd_burst_size;
    uint        rd_num_xfer;

    uint num_param;
} Memory_Test_Sequence_Parameters_t;

typedef struct
{
    uint        duration;
    uint        power_toggle;
} Power_Test_Sequence_Parameters_t;

typedef struct
{
    uint        duration;
    std::string mode;
} GTMAC_Test_Sequence_Parameters_t;

// Type used to check JSON parameter definition (node type, not node value type!)
using Definition_t      = std::pair<std::vector<std::string>, JsonNodeType>;
using Json_Definition_t = std::map<std::vector<std::string>, JsonNodeType>;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const int NUM_KERNEL_TYPE   = 6; // quantity of kernel type + 1 for unknown kernel: pwr, ddr, hbm, gt

const std::string TEST_MEM_TYPE_BANK  = "bank";
const std::string TEST_MEM_TYPE_DDR   = "DDR";
const std::string TEST_MEM_TYPE_HBM   = "HBM";
const uint MAX_NUM_KERNEL_CORE      = 32;
const int  KERNEL_CORE_IDX_UNUSED   = -1;

const int BI_MEM_KERNEL_DST_TYPE_DDR   = 0;
const int BI_MEM_KERNEL_DST_TYPE_HBM   = 1;

const std::string TIMESTAMP_MODE_NONE           = "none";
const std::string TIMESTAMP_MODE_ABSOLUTE       = "absolute";
const std::string TIMESTAMP_MODE_DIFFERENTIAL   = "differential";
const std::set<std::string> SUPPORTED_TIMESTAMP_MODE = {
    TIMESTAMP_MODE_NONE,
    TIMESTAMP_MODE_ABSOLUTE,
    TIMESTAMP_MODE_DIFFERENTIAL
};

// Test type JSON values
const std::string TYPE_MEMBER_DEVICE      = "device";
const std::string TYPE_MEMBER_DEVICE_MGT  = "device_mgt";
const std::string TYPE_MEMBER_DMA         = "dma";
const std::string TYPE_MEMBER_VERIFY      = "verify";
const std::string TYPE_MEMBER_POWER       = "power";
const std::string TYPE_MEMBER_MEMORY_DDR  = "memory_ddr";
const std::string TYPE_MEMBER_MEMORY_HBM  = "memory_hbm";
const std::string TYPE_MEMBER_GT          = "gt";
const std::string TYPE_MEMBER_GT_MAC      = "gt_mac";
const std::set<std::string> TEST_SUPPORTED_JSON_TYPE_VALUES = {
    TYPE_MEMBER_DEVICE_MGT,
    TYPE_MEMBER_DMA,
    TYPE_MEMBER_POWER,
    TYPE_MEMBER_MEMORY_DDR,
    TYPE_MEMBER_MEMORY_HBM,
    TYPE_MEMBER_GT,
    TYPE_MEMBER_GT_MAC
};

// Common test source JSON values
const std::string TEST_SOURCE_MEMBER_JSON  = "json";
const std::string TEST_SOURCE_MEMBER_FILE  = "file";
const std::set<std::string> SUPPORTED_TEST_SOURCE = {
    TEST_SOURCE_MEMBER_JSON,
    TEST_SOURCE_MEMBER_FILE
};

// Memory test modes
const std::string MEM_CTRL_TEST_MODE_ALTERNATE_WR_RD_TEST    = "Alternate_Wr_Rd";
const std::string MEM_CTRL_TEST_MODE_ONLY_WR_TEST            = "Only_Wr";
const std::string MEM_CTRL_TEST_MODE_ONLY_RD_TEST            = "Only_Rd";
const std::string MEM_CTRL_TEST_MODE_STOP_TEST               = "Stop";
const std::set<std::string> SUPPORTED_MEM_TEST_MODE = {
    MEM_CTRL_TEST_MODE_ALTERNATE_WR_RD_TEST,
    MEM_CTRL_TEST_MODE_ONLY_WR_TEST,
    MEM_CTRL_TEST_MODE_ONLY_RD_TEST,
    MEM_CTRL_TEST_MODE_STOP_TEST
};

// DMA test sequence modes
const std::string TEST_SEQUENCE_MODE_ALL      = "all";
const std::string TEST_SEQUENCE_MODE_SINGLE   = "single";

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Test sequence parameters names
const std::string DURATION      = "duration";
// DMA
const std::string MEM_TYPE      = "mem_type";
const std::string MEM_INDEX     = "mem_index";
const std::string BUFFER_SIZE   = "buffer_size";
// Memory
const std::string TEST_MODE     = "test_mode";
const std::string WR_START_ADDR = "wr_start_addr";
const std::string WR_BURST_SIZE = "wr_burst_size";
const std::string WR_NUM_XFER   = "wr_num_xfer";
const std::string RD_START_ADDR = "rd_start_addr";
const std::string RD_BURST_SIZE = "rd_burst_size";
const std::string RD_NUM_XFER   = "rd_num_xfer";
// Power
const std::string POWER_TOGGLE = "power_toggle";
// GT MAC
const std::string MODE = "mode";

// Test sequence number of parameters
const uint NUM_TEST_SEQ_PARAM_DMA           = 4;
const uint NUM_TEST_SEQ_PARAM_MEMORY_ALT    = 8;
const uint NUM_TEST_SEQ_PARAM_MEMORY_ONLY   = 5;
const uint NUM_TEST_SEQ_PARAM_MEMORY_DEF    = 2;
const uint NUM_TEST_SEQ_PARAM_POWER         = 2;
const uint NUM_TEST_SEQ_PARAM_GTMAC         = 2;

const uint MAX_NUM_PARSER_ERROR = 20;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// JSON members
// Not defined as Json_Val_Def_t as it is defining the structure of the test JSON
const std::string TESTCASES_MEMBER    = "testcases";
const std::string PARAMETERS_MEMBER   = "parameters";
const std::string TYPE_MEMBER         = "type";
// Device parameters
const Json_Val_Def_t VERBOSITY_MEMBER      = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_INT,    "verbosity"};
const Json_Val_Def_t LOGGING_MEMBER        = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_STRING, "logging"};
const Json_Val_Def_t TIMESTAMP_MEMBER      = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_STRING, "timestamp"};
const Json_Val_Def_t DEVICE_MEMBER         = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_STRING, "device"};
const Json_Val_Def_t DEVICE_IDX_MEMBER     = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_UINT,   "device_idx"};
const Json_Val_Def_t XBTEST_PFM_DEF_MEMBER = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_STRING, "xbtest_pfm_def"};
const Json_Val_Def_t XCLBIN_MEMBER         = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_STRING, "xclbin"};
const Json_Params_Def_t DEVICE_PARAMETERS_DEFINITION = {
    VERBOSITY_MEMBER,
    LOGGING_MEMBER,
    TIMESTAMP_MEMBER,
    DEVICE_MEMBER,
    DEVICE_IDX_MEMBER,
    XBTEST_PFM_DEF_MEMBER,
    XCLBIN_MEMBER
};

// Common test JSON members
const Json_Val_Def_t TEST_SEQUENCE_MEMBER      = {REQUIRED_TRUE,  HIDDEN_FALSE, JSON_NODE_ARRAY, TYPE_ID_TEST_SEQUENCE,    "test_sequence"};
const Json_Val_Def_t TEST_SEQUENCE_MODE_MEMBER = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_STRING,           "test_sequence_mode"};
const Json_Val_Def_t OUTPUT_FILE_MEMBER        = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_STRING,           "output_file"};
const Json_Val_Def_t TEST_SOURCE_MEMBER        = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_STRING,           "test_source"};
// DMA + Memory test JSON members
const Json_Val_Def_t CHECK_BW_MEMBER           = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_BOOL, "check_bw"};
// DMA test JSON members
const Json_Val_Def_t DDR_TOTAL_SIZE_MEMBER     = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_UINT, "ddr_total_size"};
const Json_Val_Def_t HBM_TOTAL_SIZE_MEMBER     = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_UINT, "hbm_total_size"};
const Json_Val_Def_t LO_THRESH_WR_DDR_MEMBER   = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_UINT, "lo_thresh_wr_ddr"};
const Json_Val_Def_t HI_THRESH_WR_DDR_MEMBER   = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_UINT, "hi_thresh_wr_ddr"};
const Json_Val_Def_t LO_THRESH_RD_DDR_MEMBER   = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_UINT, "lo_thresh_rd_ddr"};
const Json_Val_Def_t HI_THRESH_RD_DDR_MEMBER   = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_UINT, "hi_thresh_rd_ddr"};
const Json_Val_Def_t LO_THRESH_WR_HBM_MEMBER   = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_UINT, "lo_thresh_wr_hbm"};
const Json_Val_Def_t HI_THRESH_WR_HBM_MEMBER   = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_UINT, "hi_thresh_wr_hbm"};
const Json_Val_Def_t LO_THRESH_RD_HBM_MEMBER   = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_UINT, "lo_thresh_rd_hbm"};
const Json_Val_Def_t HI_THRESH_RD_HBM_MEMBER   = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_UINT, "hi_thresh_rd_hbm"};
const Json_Params_Def_t DMA_PARAMETERS_DEFINITION = {
    VERBOSITY_MEMBER,
    TEST_SOURCE_MEMBER,
    TEST_SEQUENCE_MEMBER,
    OUTPUT_FILE_MEMBER,
    DDR_TOTAL_SIZE_MEMBER,
    HBM_TOTAL_SIZE_MEMBER,
    LO_THRESH_WR_DDR_MEMBER,
    HI_THRESH_WR_DDR_MEMBER,
    LO_THRESH_RD_DDR_MEMBER,
    HI_THRESH_RD_DDR_MEMBER,
    LO_THRESH_WR_HBM_MEMBER,
    HI_THRESH_WR_HBM_MEMBER,
    LO_THRESH_RD_HBM_MEMBER,
    HI_THRESH_RD_HBM_MEMBER,
    CHECK_BW_MEMBER
};
// Memory test JSON members
const Json_Val_Def_t ERROR_INSERTION_MEMBER        = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_BOOL, "error_insertion"};
const Json_Val_Def_t LO_THRESH_ALT_WR_BW_MEMBER    = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_UINT, "lo_thresh_alt_wr_bw"};
const Json_Val_Def_t HI_THRESH_ALT_WR_BW_MEMBER    = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_UINT, "hi_thresh_alt_wr_bw"};
const Json_Val_Def_t LO_THRESH_ALT_RD_BW_MEMBER    = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_UINT, "lo_thresh_alt_rd_bw"};
const Json_Val_Def_t HI_THRESH_ALT_RD_BW_MEMBER    = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_UINT, "hi_thresh_alt_rd_bw"};
const Json_Val_Def_t LO_THRESH_ONLY_WR_BW_MEMBER   = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_UINT, "lo_thresh_only_wr_bw"};
const Json_Val_Def_t HI_THRESH_ONLY_WR_BW_MEMBER   = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_UINT, "hi_thresh_only_wr_bw"};
const Json_Val_Def_t LO_THRESH_ONLY_RD_BW_MEMBER   = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_UINT, "lo_thresh_only_rd_bw"};
const Json_Val_Def_t HI_THRESH_ONLY_RD_BW_MEMBER   = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_UINT, "hi_thresh_only_rd_bw"};
const Json_Params_Def_t MEMORY_PARAMETERS_DEFINITION = {
    VERBOSITY_MEMBER,
    TEST_SOURCE_MEMBER,
    TEST_SEQUENCE_MEMBER,
    OUTPUT_FILE_MEMBER,
    ERROR_INSERTION_MEMBER,
    LO_THRESH_ALT_WR_BW_MEMBER,
    HI_THRESH_ALT_WR_BW_MEMBER,
    LO_THRESH_ALT_RD_BW_MEMBER,
    HI_THRESH_ALT_RD_BW_MEMBER,
    LO_THRESH_ONLY_WR_BW_MEMBER,
    HI_THRESH_ONLY_WR_BW_MEMBER,
    LO_THRESH_ONLY_RD_BW_MEMBER,
    HI_THRESH_ONLY_RD_BW_MEMBER,
    CHECK_BW_MEMBER
};
// Power test JSON members
const Json_Val_Def_t POWER_ENABLE_REG_MEMBER                       = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_BOOL,   "enable_REG"};
const Json_Val_Def_t POWER_ENABLE_DSP_MEMBER                       = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_BOOL,   "enable_DSP"};
const Json_Val_Def_t POWER_ENABLE_BRAM_MEMBER                      = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_BOOL,   "enable_BRAM"};
const Json_Val_Def_t POWER_ENABLE_URAM_MEMBER                      = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_BOOL,   "enable_URAM"};
const Json_Val_Def_t POWER_TOLERANCE_MEMBER                        = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_UINT,   "power_tolerance"};
const Json_Val_Def_t POWER_STABILITY_TOL_MEMBER                    = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_INT,    "power_stability_tol"};
const Json_Val_Def_t POWER_TARGET_REACH_TIME_MEMBER                = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_UINT,   "power_target_reach_time"};
const Json_Val_Def_t POWER_NUM_LEAKAGE_CALIBRATION_MEMBER          = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_UINT,   "num_leakage_calibration"};
const Json_Val_Def_t POWER_LEAKAGE_CALIBRATION_TIMEOUT_MEMBER      = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_UINT,   "leakage_calibration_timeout"};
const Json_Val_Def_t POWER_LEAKAGE_CALIBRATION_RESULT_FILE_MEMBER  = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_STRING, "leakage_calibration_output_file"};
const Json_Val_Def_t POWER_LEAKAGE_CALIBRATION_LOW_TEMP_MEMBER     = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_INT,    "leakage_calibration_low_temp"};
const Json_Val_Def_t POWER_LEAKAGE_CALIBRATION_HIGH_TEMP_MEMBER    = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_INT,    "leakage_calibration_high_temp"};
const Json_Val_Def_t POWER_USE_LEAKAGE_MODEL_MEMBER                = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_BOOL,   "use_leakage_model"};
const Json_Val_Def_t POWER_OPEN_LOOP_MEMBER                        = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_BOOL,   "open_loop"};
const Json_Val_Def_t POWER_SET_FAN_MAX_FILE_MEMBER                 = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_STRING, "set_fan_max_file"};
const Json_Val_Def_t POWER_SET_FAN_MIN_FILE_MEMBER                 = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_STRING, "set_fan_min_file"};
const Json_Val_Def_t POWER_PWR_CALIBRATION_MEMBER                  = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_UINT,   "power_calibration"};
const Json_Val_Def_t POWER_PWR_FILTER_ALPHA_MEMBER                 = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_UINT,   "power_filter_alpha"};
const Json_Params_Def_t POWER_PARAMETERS_DEFINITION = {
    VERBOSITY_MEMBER,
    TEST_SOURCE_MEMBER,
    TEST_SEQUENCE_MODE_MEMBER,
    TEST_SEQUENCE_MEMBER,
    OUTPUT_FILE_MEMBER,
    POWER_TOLERANCE_MEMBER,
    POWER_STABILITY_TOL_MEMBER,
    POWER_TARGET_REACH_TIME_MEMBER,
    POWER_ENABLE_REG_MEMBER,
    POWER_ENABLE_DSP_MEMBER,
    POWER_ENABLE_BRAM_MEMBER,
    POWER_ENABLE_URAM_MEMBER,
    POWER_NUM_LEAKAGE_CALIBRATION_MEMBER,
    POWER_LEAKAGE_CALIBRATION_TIMEOUT_MEMBER,
    POWER_LEAKAGE_CALIBRATION_RESULT_FILE_MEMBER,
    POWER_LEAKAGE_CALIBRATION_LOW_TEMP_MEMBER,
    POWER_LEAKAGE_CALIBRATION_HIGH_TEMP_MEMBER,
    POWER_USE_LEAKAGE_MODEL_MEMBER,
    POWER_OPEN_LOOP_MEMBER,
    POWER_SET_FAN_MAX_FILE_MEMBER,
    POWER_SET_FAN_MIN_FILE_MEMBER,
    POWER_PWR_CALIBRATION_MEMBER,
    POWER_PWR_FILTER_ALPHA_MEMBER
};
// Device mgt test JSON members
const Json_Params_Def_t DEVICE_MGT_PARAMETERS_DEFINITION = {
    VERBOSITY_MEMBER,
    OUTPUT_FILE_MEMBER
};
// GT test JSON members
const Json_Val_Def_t GT_LOOPBACK_MEMBER            = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_STRING, "mode_gt_loopback"};
const Json_Val_Def_t GT_RX_POL_MEMBER              = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_STRING, "gt_rx_polarity"};
const Json_Val_Def_t GT_TX_POL_MEMBER              = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_STRING, "gt_tx_polarity"};
const Json_Val_Def_t GT_RX_REVERSE_MEMBER          = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_STRING, "gt_rx_reverse"};
const Json_Val_Def_t GT_TX_REVERSE_MEMBER          = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_STRING, "gt_tx_reverse"};
const Json_Val_Def_t GT_TX_PHASE_MEMBER            = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_STRING, "gt_tx_phase"};
const Json_Val_Def_t GT_SCRAMB_DIS_MEMBER          = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_STRING, "gt_scramb_dis"};
const Json_Val_Def_t GT_RETIME_DIS_MEMBER          = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_STRING, "gt_retime_dis"};
const Json_Val_Def_t GT_ALIGN_DIS_MEMBER           = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_STRING, "gt_align_dis"};
const Json_Params_Def_t GT_PARAMETERS_DEFINITION = {
    VERBOSITY_MEMBER,
    TEST_SOURCE_MEMBER,
    TEST_SEQUENCE_MEMBER,
    OUTPUT_FILE_MEMBER,
    GT_LOOPBACK_MEMBER,
    GT_RX_POL_MEMBER,
    GT_TX_POL_MEMBER,
    GT_RX_REVERSE_MEMBER,
    GT_TX_REVERSE_MEMBER,
    GT_TX_PHASE_MEMBER,
    GT_SCRAMB_DIS_MEMBER,
    GT_RETIME_DIS_MEMBER,
    GT_ALIGN_DIS_MEMBER
};
// GT MAC test JSON members
const Json_Val_Def_t ACTIVE_MAC_MEMBER     = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_BOOL, "active_mac"};
const Json_Val_Def_t ACTIVE_MAC_0_MEMBER   = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_BOOL, "active_mac_0"};
const Json_Val_Def_t ACTIVE_MAC_1_MEMBER   = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_BOOL, "active_mac_1"};
const Json_Val_Def_t ACTIVE_MAC_2_MEMBER   = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_BOOL, "active_mac_2"};
const Json_Val_Def_t ACTIVE_MAC_3_MEMBER   = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_BOOL, "active_mac_3"};

const Json_Val_Def_t LINE_RATE_MEMBER      = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_STRING,"line_rate"};
const Json_Val_Def_t LINE_RATE_0_MEMBER    = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_STRING,"line_rate_0"};
const Json_Val_Def_t LINE_RATE_1_MEMBER    = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_STRING,"line_rate_1"};
const Json_Val_Def_t LINE_RATE_2_MEMBER    = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_STRING,"line_rate_2"};
const Json_Val_Def_t LINE_RATE_3_MEMBER    = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_STRING,"line_rate_3"};

const Json_Val_Def_t UTILISATION_MEMBER    = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_UINT, "utilisation"};
const Json_Val_Def_t UTILISATION_0_MEMBER  = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_UINT, "utilisation_0"};
const Json_Val_Def_t UTILISATION_1_MEMBER  = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_UINT, "utilisation_1"};
const Json_Val_Def_t UTILISATION_2_MEMBER  = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_UINT, "utilisation_2"};
const Json_Val_Def_t UTILISATION_3_MEMBER  = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_UINT, "utilisation_3"};

const Json_Val_Def_t SET_TEST_PAT_MEMBER   = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_BOOL, "set_test_pat"};
const Json_Val_Def_t SET_TEST_PAT_0_MEMBER = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_BOOL, "set_test_pat_0"};
const Json_Val_Def_t SET_TEST_PAT_1_MEMBER = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_BOOL, "set_test_pat_1"};
const Json_Val_Def_t SET_TEST_PAT_2_MEMBER = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_BOOL, "set_test_pat_2"};
const Json_Val_Def_t SET_TEST_PAT_3_MEMBER = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_BOOL, "set_test_pat_3"};

const Json_Val_Def_t FEC_MODE_MEMBER       = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_STRING, "fec_mode"};
const Json_Val_Def_t FEC_MODE_0_MEMBER     = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_STRING, "fec_mode_0"};
const Json_Val_Def_t FEC_MODE_1_MEMBER     = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_STRING, "fec_mode_1"};
const Json_Val_Def_t FEC_MODE_2_MEMBER     = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_STRING, "fec_mode_2"};
const Json_Val_Def_t FEC_MODE_3_MEMBER     = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_STRING, "fec_mode_3"};

const Json_Val_Def_t TRAFFIC_TYPE_MEMBER   = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_STRING, "traffic_type"};
const Json_Val_Def_t TRAFFIC_TYPE_0_MEMBER = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_STRING, "traffic_type_0"};
const Json_Val_Def_t TRAFFIC_TYPE_1_MEMBER = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_STRING, "traffic_type_1"};
const Json_Val_Def_t TRAFFIC_TYPE_2_MEMBER = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_STRING, "traffic_type_2"};
const Json_Val_Def_t TRAFFIC_TYPE_3_MEMBER = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_STRING, "traffic_type_3"};

const Json_Val_Def_t PACKET_CFG_MEMBER     = {REQUIRED_FALSE, HIDDEN_FALSE,  JSON_NODE_VALUE, TYPE_ID_STRING, "packet_cfg"};
const Json_Val_Def_t PACKET_CFG_0_MEMBER   = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_STRING, "packet_cfg_0"};
const Json_Val_Def_t PACKET_CFG_1_MEMBER   = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_STRING, "packet_cfg_1"};
const Json_Val_Def_t PACKET_CFG_2_MEMBER   = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_STRING, "packet_cfg_2"};
const Json_Val_Def_t PACKET_CFG_3_MEMBER   = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_STRING, "packet_cfg_3"};

const Json_Val_Def_t TX_MAPPING_0_MEMBER   = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_UINT, "tx_mapping_0"};
const Json_Val_Def_t TX_MAPPING_1_MEMBER   = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_UINT, "tx_mapping_1"};
const Json_Val_Def_t TX_MAPPING_2_MEMBER   = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_UINT, "tx_mapping_2"};
const Json_Val_Def_t TX_MAPPING_3_MEMBER   = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_UINT, "tx_mapping_3"};

const Json_Val_Def_t MATCH_TX_RX_MEMBER     = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_BOOL, "match_tx_rx"};
const Json_Val_Def_t MATCH_TX_RX_0_MEMBER   = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_BOOL, "match_tx_rx_0"};
const Json_Val_Def_t MATCH_TX_RX_1_MEMBER   = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_BOOL, "match_tx_rx_1"};
const Json_Val_Def_t MATCH_TX_RX_2_MEMBER   = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_BOOL, "match_tx_rx_2"};
const Json_Val_Def_t MATCH_TX_RX_3_MEMBER   = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_BOOL, "match_tx_rx_3"};

const Json_Val_Def_t GT_TX_DIFFCTRL_MEMBER     = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_UINT, "gt_tx_diffctrl"};
const Json_Val_Def_t GT_TX_DIFFCTRL_0_MEMBER   = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_UINT, "gt_tx_diffctrl_0"};
const Json_Val_Def_t GT_TX_DIFFCTRL_1_MEMBER   = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_UINT, "gt_tx_diffctrl_1"};
const Json_Val_Def_t GT_TX_DIFFCTRL_2_MEMBER   = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_UINT, "gt_tx_diffctrl_2"};
const Json_Val_Def_t GT_TX_DIFFCTRL_3_MEMBER   = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_UINT, "gt_tx_diffctrl_3"};

const Json_Val_Def_t GT_TX_PRE_EMPH_MEMBER     = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_UINT, "gt_tx_pre_emph"};
const Json_Val_Def_t GT_TX_PRE_EMPH_0_MEMBER   = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_UINT, "gt_tx_pre_emph_0"};
const Json_Val_Def_t GT_TX_PRE_EMPH_1_MEMBER   = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_UINT, "gt_tx_pre_emph_1"};
const Json_Val_Def_t GT_TX_PRE_EMPH_2_MEMBER   = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_UINT, "gt_tx_pre_emph_2"};
const Json_Val_Def_t GT_TX_PRE_EMPH_3_MEMBER   = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_UINT, "gt_tx_pre_emph_3"};

const Json_Val_Def_t GT_TX_POST_EMPH_MEMBER    = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_UINT, "gt_tx_post_emph"};
const Json_Val_Def_t GT_TX_POST_EMPH_0_MEMBER  = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_UINT, "gt_tx_post_emph_0"};
const Json_Val_Def_t GT_TX_POST_EMPH_1_MEMBER  = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_UINT, "gt_tx_post_emph_1"};
const Json_Val_Def_t GT_TX_POST_EMPH_2_MEMBER  = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_UINT, "gt_tx_post_emph_2"};
const Json_Val_Def_t GT_TX_POST_EMPH_3_MEMBER  = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_UINT, "gt_tx_post_emph_3"};

const Json_Val_Def_t GT_RX_USE_LPM_MEMBER      = {REQUIRED_FALSE, HIDDEN_FALSE, JSON_NODE_VALUE, TYPE_ID_BOOL, "gt_rx_use_lpm"};
const Json_Val_Def_t GT_RX_USE_LPM_0_MEMBER    = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_BOOL, "gt_rx_use_lpm_0"};
const Json_Val_Def_t GT_RX_USE_LPM_1_MEMBER    = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_BOOL, "gt_rx_use_lpm_1"};
const Json_Val_Def_t GT_RX_USE_LPM_2_MEMBER    = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_BOOL, "gt_rx_use_lpm_2"};
const Json_Val_Def_t GT_RX_USE_LPM_3_MEMBER    = {REQUIRED_FALSE, HIDDEN_TRUE,  JSON_NODE_VALUE, TYPE_ID_BOOL, "gt_rx_use_lpm_3"};
const Json_Params_Def_t GT_MAC_PARAMETERS_DEFINITION = {
    VERBOSITY_MEMBER,
    TEST_SOURCE_MEMBER,
    TEST_SEQUENCE_MEMBER,
    OUTPUT_FILE_MEMBER,

    ACTIVE_MAC_MEMBER,
    ACTIVE_MAC_0_MEMBER,
    ACTIVE_MAC_1_MEMBER,
    ACTIVE_MAC_2_MEMBER,
    ACTIVE_MAC_3_MEMBER,

    LINE_RATE_MEMBER,
    LINE_RATE_0_MEMBER,
    LINE_RATE_1_MEMBER,
    LINE_RATE_2_MEMBER,
    LINE_RATE_3_MEMBER,

    UTILISATION_MEMBER,
    UTILISATION_0_MEMBER,
    UTILISATION_1_MEMBER,
    UTILISATION_2_MEMBER,
    UTILISATION_3_MEMBER,

    SET_TEST_PAT_MEMBER,
    SET_TEST_PAT_0_MEMBER,
    SET_TEST_PAT_1_MEMBER,
    SET_TEST_PAT_2_MEMBER,
    SET_TEST_PAT_3_MEMBER,

    FEC_MODE_MEMBER,
    FEC_MODE_0_MEMBER,
    FEC_MODE_1_MEMBER,
    FEC_MODE_2_MEMBER,
    FEC_MODE_3_MEMBER,

    TRAFFIC_TYPE_MEMBER,
    TRAFFIC_TYPE_0_MEMBER,
    TRAFFIC_TYPE_1_MEMBER,
    TRAFFIC_TYPE_2_MEMBER,
    TRAFFIC_TYPE_3_MEMBER,

    PACKET_CFG_MEMBER,
    PACKET_CFG_0_MEMBER,
    PACKET_CFG_1_MEMBER,
    PACKET_CFG_2_MEMBER,
    PACKET_CFG_3_MEMBER,

    TX_MAPPING_0_MEMBER,
    TX_MAPPING_1_MEMBER,
    TX_MAPPING_2_MEMBER,
    TX_MAPPING_3_MEMBER,

    MATCH_TX_RX_MEMBER,
    MATCH_TX_RX_0_MEMBER,
    MATCH_TX_RX_1_MEMBER,
    MATCH_TX_RX_2_MEMBER,
    MATCH_TX_RX_3_MEMBER,

    GT_TX_DIFFCTRL_MEMBER,
    GT_TX_DIFFCTRL_0_MEMBER,
    GT_TX_DIFFCTRL_1_MEMBER,
    GT_TX_DIFFCTRL_2_MEMBER,
    GT_TX_DIFFCTRL_3_MEMBER,

    GT_TX_PRE_EMPH_MEMBER,
    GT_TX_PRE_EMPH_0_MEMBER,
    GT_TX_PRE_EMPH_1_MEMBER,
    GT_TX_PRE_EMPH_2_MEMBER,
    GT_TX_PRE_EMPH_3_MEMBER,

    GT_TX_POST_EMPH_MEMBER,
    GT_TX_POST_EMPH_0_MEMBER,
    GT_TX_POST_EMPH_1_MEMBER,
    GT_TX_POST_EMPH_2_MEMBER,
    GT_TX_POST_EMPH_3_MEMBER,

    GT_RX_USE_LPM_MEMBER,
    GT_RX_USE_LPM_0_MEMBER,
    GT_RX_USE_LPM_1_MEMBER,
    GT_RX_USE_LPM_2_MEMBER,
    GT_RX_USE_LPM_3_MEMBER
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum KrnlType {
    KRNL_TYPE_PWR       = 0,
    KRNL_TYPE_MEM_DDR   = 1,
    KRNL_TYPE_MEM_HBM   = 2,
    KRNL_TYPE_GT        = 3,
    KRNL_TYPE_GT_MAC    = 4,
    KRNL_TYPE_UNKNOWN   = 5 // Need to be max value of enum
};

enum TestType {
    TEST_DEVICE     = 0,
    TEST_DEVICE_MGT = 1,
    TEST_DMA        = 2,
    TEST_VERIFY     = 3,
    TEST_POWER      = 4,
    TEST_MEMORY_DDR = 5,
    TEST_MEMORY_HBM = 6,
    TEST_GT         = 7,
    TEST_GT_MAC     = 8,
    TEST_MAX        = 9
};

enum TestCaseType {
    TESTCASE_TEST   = 0,
    TESTCASE_TASK   = 1
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline const bool StrMatchNoCase(std::string str1, std::string str2)
{
    bool ret = false;
    std::transform(str1.begin(), str1.end(), str1.begin(), tolower);
    std::transform(str2.begin(), str2.end(), str2.begin(), tolower);
    if (str1 == str2)
        ret = true;
    return ret;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline const bool StrMatchNoCase(std::string str1, int begin, int size, std::string str2)
{
    // Compares "size" characters from "begin" index of str1 with string str2
    bool ret = false;
    std::transform(str1.begin(), str1.end(), str1.begin(), tolower);
    std::transform(str2.begin(), str2.end(), str2.begin(), tolower);
    if (str1.compare(begin, size,  str2) == 0)
        ret = true;
    return ret;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline const Json_Parameters_t::iterator FindJsonParam(Json_Parameters_t *json_parameters, Json_Val_Def_t json_val_def)
{
    Json_Parameters_t::iterator it = json_parameters->end();
    // Check if str present in json_parameters using lower case and return iterator
    for (Json_Parameters_t::iterator param_it = json_parameters->begin(); param_it != json_parameters->end(); ++param_it)
    {
        if (StrMatchNoCase(param_it->first, json_val_def.name) == true)
        {
            it = param_it;
            break;
        }
    }
    return it;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline const void EraseJsonParam(Json_Parameters_t *json_parameters, Json_Val_Def_t json_val_def)
{
    // Check if str present in json_parameters using lower case and return iterator
    for (Json_Parameters_t::iterator param_it = json_parameters->begin(); param_it != json_parameters->end(); ++param_it)
    {
        if (StrMatchNoCase(param_it->first, json_val_def.name))
        {
            json_parameters->erase(param_it->first);
            break;
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T> inline const void InsertJsonParam(Json_Parameters_t *json_parameters, Json_Val_Def_t json_val_def, T value)
{
    json_parameters->insert( Json_Parameter_t(json_val_def.name, new TestcaseParam<T>(value)) );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline const std::string TestTypeToString(TestType Type)
{
    std::string strString;
    switch (Type)
    {
        default:                strString =  "unknown";                 break;
        case TEST_DEVICE:       strString =  TYPE_MEMBER_DEVICE;        break;
        case TEST_DEVICE_MGT:   strString =  TYPE_MEMBER_DEVICE_MGT;    break;
        case TEST_DMA:          strString =  TYPE_MEMBER_DMA;           break;
        case TEST_VERIFY:       strString =  TYPE_MEMBER_VERIFY;        break;
        case TEST_POWER:        strString =  TYPE_MEMBER_POWER;         break;
        case TEST_MEMORY_DDR:   strString =  TYPE_MEMBER_MEMORY_DDR;    break;
        case TEST_MEMORY_HBM:   strString =  TYPE_MEMBER_MEMORY_HBM;    break;
        case TEST_GT:           strString =  TYPE_MEMBER_GT;            break;
        case TEST_GT_MAC:       strString =  TYPE_MEMBER_GT_MAC;        break;
    }
    std::transform(strString.begin(), strString.end(), strString.begin(), toupper);
    return strString;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline const TestType TestTypeStringToEnum(std::string Type_Str)
{
    if      (StrMatchNoCase(Type_Str, TYPE_MEMBER_DEVICE)      == true) return TEST_DEVICE;
    else if (StrMatchNoCase(Type_Str, TYPE_MEMBER_DEVICE_MGT)  == true) return TEST_DEVICE_MGT;
    else if (StrMatchNoCase(Type_Str, TYPE_MEMBER_DMA)         == true) return TEST_DMA;
    else if (StrMatchNoCase(Type_Str, TYPE_MEMBER_VERIFY)      == true) return TEST_VERIFY;
    else if (StrMatchNoCase(Type_Str, TYPE_MEMBER_POWER)       == true) return TEST_POWER;
    else if (StrMatchNoCase(Type_Str, TYPE_MEMBER_MEMORY_DDR)  == true) return TEST_MEMORY_DDR;
    else if (StrMatchNoCase(Type_Str, TYPE_MEMBER_MEMORY_HBM)  == true) return TEST_MEMORY_HBM;
    else if (StrMatchNoCase(Type_Str, TYPE_MEMBER_GT)          == true) return TEST_GT;
    else if (StrMatchNoCase(Type_Str, TYPE_MEMBER_GT_MAC)      == true) return TEST_GT_MAC;
    return TEST_MAX;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline const KrnlType TestTypeToKernelType(TestType test_type)
{
    KrnlType krnl_type;
    switch (test_type)
    {
        default:                krnl_type =  KRNL_TYPE_UNKNOWN; break;
        case TEST_POWER:        krnl_type =  KRNL_TYPE_PWR;     break;
        case TEST_MEMORY_DDR:   krnl_type =  KRNL_TYPE_MEM_DDR; break;
        case TEST_MEMORY_HBM:   krnl_type =  KRNL_TYPE_MEM_HBM; break;
        case TEST_GT:           krnl_type =  KRNL_TYPE_GT;      break;
        case TEST_GT_MAC:       krnl_type =  KRNL_TYPE_GT_MAC;  break;
    }
    return krnl_type;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T> inline std::string NumToStrHex ( T value )
{
    std::stringstream hex_sstream;
    hex_sstream << std::hex << value;
    return hex_sstream.str();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T> inline bool ConvString2Num( std::string str_in, T *value )
{
    bool ret_failure = false;
    std::string str = str_in;

    std::size_t found_not_0 = str.find_first_not_of('0');
    if (found_not_0 == std::string::npos) // String is one or more zero
        str = "0";
    else // Remove zero padding if any
        str.erase(0, found_not_0);

    std::istringstream ss(str);
    ss >> *value;

    if (StrMatchNoCase(str, std::to_string(*value)) == false) // Check if conversion failed
        ret_failure = true;

    return ret_failure;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline bool GetVerbosity( Json_Parameters_t *params, int *verbosity )
{
    // extract the verbosity from the parameters
    Json_Parameters_t::iterator it = FindJsonParam(params, VERBOSITY_MEMBER);
    if (it != params->end())
    {
        *verbosity = TestcaseParamCast<int>(it->second);
        if ((*verbosity < -2) || (*verbosity > 6))
            return true;
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const std::string VERBOSITY_FAILURE = "Wrong value for JSON member \"" + VERBOSITY_MEMBER.name + "\"";

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline std::string StrVectToStr( std::vector<std::string> str_vect, std::string delimiter )
{
    std::string str = "";
    for (uint ii = 0; ii < str_vect.size(); ii++ )
    {
        str += str_vect[ii];
        if (ii < str_vect.size() - 1)
            str += delimiter;
    }
    return str;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T> inline std::string NumVectToStr( std::vector<T> str_vect, std::string delimiter )
{
    std::string str = "";
    for (uint ii = 0; ii < str_vect.size(); ii++ )
    {
        str += std::to_string(str_vect[ii]);
        if (ii < str_vect.size() - 1)
            str += delimiter;
    }
    return str;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const std::string BOOL_TRUE_STR     = "true";
const std::string BOOL_FALSE_STR    = "false";
const std::set<std::string> SUPPORTED_BOOL_STR = {
    BOOL_TRUE_STR,
    BOOL_FALSE_STR
};

inline std::string BoolToStr( bool bool_in )
{
    std::string bool_out;
    if      (bool_in == true)   bool_out = BOOL_TRUE_STR;
    else if (bool_in == false)  bool_out = BOOL_FALSE_STR;
    return bool_out;
}

inline bool StrToBool( std::string bool_in )
{
    bool bool_out = false;
    if      (StrMatchNoCase(bool_in, BOOL_TRUE_STR)  == true) bool_out = true;
    else if (StrMatchNoCase(bool_in, BOOL_FALSE_STR) == true) bool_out = false;
    return bool_out;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T> inline std::string Float_to_String(T num, int precision)
{
    int prec = precision;
    if (precision < 0) prec = 0;

    std::stringstream stream;
    stream << std::fixed << std::setprecision(prec) << num;
    std::string s = stream.str();
    return s;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Platform def structure
const std::string PLATDEF_JSON_NAME     = "xbtest_pfm_def.json";

const uint MAX_CLOCKS           = 10;
const uint MAX_TEMP_SOURCES     = 10;
const uint MAX_POWER_SOURCES    = 10;

typedef struct Xbtest_Pfm_Def_Clock_t
{
    std::vector<std::string>    name;
    uint                        frequency;
} Xbtest_Pfm_Def_Clock_t;

typedef struct Xbtest_Pfm_Def_Info_t
{
    std::string                             name;
    std::vector<Xbtest_Pfm_Def_Clock_t>    clocks;
    uint                                    num_clocks;
} Xbtest_Pfm_Def_Info_t;

typedef struct Xbtest_Pfm_Def_Runtime_t
{
    int download_time;
} Xbtest_Pfm_Def_Runtime_t;

typedef struct Xbtest_Pfm_Def_Pwr_Src_t
{
    std::vector<std::string>    name;
    std::vector<std::string>    name_current;
    std::vector<std::string>    name_voltage;
    std::string                 source_name;
    std::string                 source_name_current;
    std::string                 source_name_voltage;
    int                         limit;
    bool                        powertest;
    int                         calibration;
    bool                        def_by_curr_volt;
} Xbtest_Pfm_Def_Pwr_Src_t;

typedef struct Xbtest_Pfm_Def_Pwr_Target_t
{
    uint min;
    uint max;
} Xbtest_Pfm_Def_Pwr_Target_t;

typedef struct Xbtest_Pfm_Def_Power_t
{
    uint                                    max_calibration;
    Xbtest_Pfm_Def_Pwr_Target_t             power_target;
    std::vector<Xbtest_Pfm_Def_Pwr_Src_t>   power_sources;
    uint                                    num_power_sources;
} Xbtest_Pfm_Def_Power_t;


typedef struct Xbtest_Pfm_Def_Leak_Calib_t
{
    double    a;
    double    b;
    double    c;
} Xbtest_Pfm_Def_Leak_Calib_t;

typedef struct Xbtest_Pfm_Def_Temp_Src_t
{
    std::vector<std::string>    name;
    std::string                 source_name;
    int                         limit;
} Xbtest_Pfm_Def_Temp_Src_t;

typedef struct Xbtest_Pfm_Def_Thermal_t
{
    Xbtest_Pfm_Def_Leak_Calib_t             calibration;
    Xbtest_Pfm_Def_Leak_Calib_t             xpe_leakage;
    std::vector<Xbtest_Pfm_Def_Temp_Src_t>  temp_sources;
    uint                                    num_temp_sources;
} Xbtest_Pfm_Def_Thermal_t;

typedef struct Xbtest_Pfm_Def_Physical_t
{
    Xbtest_Pfm_Def_Thermal_t   thermal;
    Xbtest_Pfm_Def_Power_t     power;
} Xbtest_Pfm_Def_Physical_t;

typedef struct Xbtest_Pfm_Def_Mem_Thresh_HiLo_t
{
    uint high = 0;
    uint low = 0;
} Xbtest_Pfm_Def_Mem_Thresh_HiLo_t;

typedef struct Xbtest_Pfm_Def_Mem_Thresh_WrRd_t
{
    Xbtest_Pfm_Def_Mem_Thresh_HiLo_t write;
    Xbtest_Pfm_Def_Mem_Thresh_HiLo_t read;
} Xbtest_Pfm_Def_Mem_Thresh_WrRd_t;

typedef struct Xbtest_Pfm_Def_Mem_Thresh_Wr_t
{
    Xbtest_Pfm_Def_Mem_Thresh_HiLo_t write;
} Xbtest_Pfm_Def_Mem_Thresh_Wr_t;

typedef struct Xbtest_Pfm_Def_Mem_Thresh_Rd_t
{
    Xbtest_Pfm_Def_Mem_Thresh_HiLo_t read;
} Xbtest_Pfm_Def_Mem_Thresh_Rd_t;

typedef struct Xbtest_Pfm_Def_Mem_Thresh_CU_t
{
    Xbtest_Pfm_Def_Mem_Thresh_WrRd_t   alt_wr_rd;
    Xbtest_Pfm_Def_Mem_Thresh_Wr_t     only_wr;
    Xbtest_Pfm_Def_Mem_Thresh_Rd_t     only_rd;
} Xbtest_Pfm_Def_Mem_Thresh_CU_t;

typedef struct Xbtest_Pfm_Def_MemType_t
{
    uint                                size;
    uint                                quantity;
    Xbtest_Pfm_Def_Mem_Thresh_WrRd_t    dma_bw;
    Xbtest_Pfm_Def_Mem_Thresh_CU_t      cu_bw;
} Xbtest_Pfm_Def_MemType_t;

typedef struct Xbtest_Pfm_Def_Memory_t
{
    Xbtest_Pfm_Def_MemType_t    hbm;
    Xbtest_Pfm_Def_MemType_t    ddr;
    bool                        hbm_exists;
    bool                        ddr_exists;
} Xbtest_Pfm_Def_Memory_t;

typedef struct Xbtest_Pfm_Def_t
{
    Xbtest_Pfm_Def_Info_t      info;
    Xbtest_Pfm_Def_Runtime_t   runtime;
    Xbtest_Pfm_Def_Physical_t  physical;
    Xbtest_Pfm_Def_Memory_t    memory;
} Xbtest_Pfm_Def_t;

const uint      MAX_UINT_VAL        = 0xFFFFFFFF; // 32b
const uint64_t  MAX_UINT64_T_VAL    = 0xFFFFFFFFFFFFFFFF; // 64b


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Memory Topology structure
typedef struct
{
    std::string type;
    uint        temp;
    std::string tag;
    bool        enabled;
} MemData_t;

typedef struct
{
    std::vector<MemData_t> mem_data;
    uint                   mem_count;
} MemTopology_t;


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline void GetTimestamp(uint64_t* timestamp_us)
{
    struct timespec sample;
    clock_gettime(CLOCK_MONOTONIC, &sample);
    uint64_t secs   = (uint64_t)(sample.tv_sec*1000000); // conv sec to us
    uint64_t u_secs = (uint64_t)(sample.tv_nsec/1000);   // conv ns to us
    *timestamp_us = secs+u_secs;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum LogLevel {
    LOG_FAILURE     = 6,
    LOG_ERROR       = 5,
    LOG_PASS        = 4,
    LOG_CRIT_WARN	= 3,
    LOG_WARN	    = 2,
    LOG_INFO	    = 1,
    LOG_STATUS	    = 0,
    LOG_DEBUG	    = -1,
    LOG_DESIGNER    = -2
};

typedef struct
{
    LogLevel    verbosity;
    std::string logging;
    bool        use_logging;
} Global_Config_t;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct Connection_t
{
    uint arg_index;
    uint m_ip_layout_index;
    uint mem_data_index;
} Connection_t;

typedef struct Connectivity_t
{
    std::vector<Connection_t>  m_connection;
    uint                       m_count;
} Connectivity_t;

#endif /* _XBTESTCOMMON_H */
