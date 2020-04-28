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

#include "xbtestcommon.h"
#include "logging.h"
#include "inputparser.h"
#include "xjsonparser.h"
#include "xbtestpfmdefparser.h"
#include "xbutildumpparser.h"
#include "testcase.h"
#include "deviceinterface.h"
#include "verifytest.h"
#include "dmatest.h"
#include "powertest.h"
#include "memorytest.h"
#include "devicemgt.h"
#include "gttest.h"
#include "gtmactest.h"
#include <queue>

const int MAX_ABORT_WATCHDOG     = 10;
const LogLevel DEFAULT_LOG_LEVEL = LOG_STATUS;
const std::string GENERAL = "GENERAL    : ";

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// logging
Logging *gLog;
Global_Config_t gGlobal_config;
std::string gCmd_line;
pid_t gPid = getpid();

void LogMessage( LogLevel Level, std::string msg ) { gLog->LogMessage(Level, GENERAL + msg, gGlobal_config.verbosity); }
void LogMessageNoHeader( LogLevel Level, std::string msg ) { gLog->LogMessage(Level, msg, gGlobal_config.verbosity); }

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// signal handler

volatile std::sig_atomic_t gSignalStatus;
std::atomic<bool> gAbort;

void signal_handler(int signal)
{
    gSignalStatus = signal;

    if(gSignalStatus == SIGINT)
    {
        /* signal abort */
        gAbort = true;
        LogMessage(LOG_FAILURE, "User abort received");
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint GetNumTestCasesNotCompleted( std::vector<TestCase*> tc_list, TestCaseType test_case_type );
bool CheckTestCasesCompleted( std::vector<std::string> test_thread_name_list, std::vector<TestCase*> tc_list );
void SignalAbortTestCases( std::vector<TestCase*> tc_list, TestCaseType test_case_type );
void CheckTestCasesAborted( std::vector<std::string> test_thread_name_list, std::vector<TestCase*> tc_list, TestCaseType test_case_type );
bool WaitEndOfTestCases( std::vector<std::string> test_thread_name_list, std::vector<TestCase*> tc_list, TestCaseType test_case_type );
bool CheckStringInSet ( std::string value, std::set<std::string> test_set );
bool SetCommandLineParam(
    Json_Parameters_t *device_params,
    bool bflag, std::string verbosity_str,
    bool pflag, std::string platform,
    bool eflag, std::string xbtest_pfm_def,
    bool tflag, std::string timestamp_mode,
    bool dflag, std::string device_idx,
    bool xflag, std::string xclbin
);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool SystemConfig( std::string time, Json_Parameters_t *device_params )
{
    bool ret_failure = false;
    std::string sysname;
    std::string release;
    std::string version;
    std::string now;
    std::string xrt_version;
    std::string xrt_build_date;
    std::string xclbin_uuid;
    std::string firewall_status;

    // Note:
    // At "START" time, the device index is not known and not needed as we get only information of the system"
    // At "END" time, the device index is known and needed as some information depend on the board"
    std::string device_index = "NONE";
    if (time == "END")
    {
        // extract the device index from the parameters
        Json_Parameters_t::iterator it = FindJsonParam(device_params, DEVICE_IDX_MEMBER);
        if (it != device_params->end())
        {
            device_index = std::to_string(TestcaseParamCast<uint>(it->second));
        }
        else
        {
            return true;
        }
    }

    XbutilDumpParser *xbutil_dump_parser = new XbutilDumpParser(device_index, gGlobal_config, &gAbort);

    ret_failure |= xbutil_dump_parser->Parse();
    if (ret_failure == true) return true;

    ret_failure |= xbutil_dump_parser->ExtractNodeValueStr({"system", "now"}, &now);
    if (ret_failure == true) return true;

    if (time == "START")
    {
        ret_failure |= xbutil_dump_parser->ExtractNodeValueStr({"system", "sysname"}, &sysname);
        if (ret_failure == true) return true;
        ret_failure |= xbutil_dump_parser->ExtractNodeValueStr({"system", "release"}, &release);
        if (ret_failure == true) return true;
        ret_failure |= xbutil_dump_parser->ExtractNodeValueStr({"system", "version"}, &version);
        if (ret_failure == true) return true;

        ret_failure |= xbutil_dump_parser->ExtractNodeValueStr({"runtime", "build", "version"}, &xrt_version);
        if (ret_failure == true) return true;
        ret_failure |= xbutil_dump_parser->ExtractNodeValueStr({"runtime", "build", "date"}, &xrt_build_date);
        if (ret_failure == true) return true;
    }

    if (time == "END")
    {
        ret_failure |= xbutil_dump_parser->ExtractNodeValueStr({"board", "error", "firewall", "status"}, &firewall_status);
        if (ret_failure == true) return true;
        ret_failure |= xbutil_dump_parser->ExtractNodeValueStr({"board", "xclbin", "uuid"}, &xclbin_uuid);
        if (ret_failure == true) return true;
    }

    if (time == "START")
    {
        LogMessageNoHeader(LOG_INFO, "***************************************************");
        LogMessageNoHeader(LOG_INFO, "XBTEST: ");
        LogMessageNoHeader(LOG_INFO, "\t - Version: " + XBTEST_VERSION_STR);
        LogMessageNoHeader(LOG_INFO, "\t - SW Build: " + std::to_string(SW_PERFORCE_VERSION) + " on " + SW_BUILD_DATE);
        LogMessageNoHeader(LOG_INFO, "\t - Process ID: " + std::to_string(gPid));
        LogMessageNoHeader(LOG_INFO, "\t - Command line: " + gCmd_line);
        LogMessageNoHeader(LOG_INFO, "System: ");
        LogMessageNoHeader(LOG_INFO, "\t - Name:    " + sysname);
        LogMessageNoHeader(LOG_INFO, "\t - Release: " + release);
        LogMessageNoHeader(LOG_INFO, "\t - Version: " + version);
        LogMessageNoHeader(LOG_INFO, "XRT: ");
        LogMessageNoHeader(LOG_INFO, "\t - Version:    " + xrt_version);
        LogMessageNoHeader(LOG_INFO, "\t - Build date: " + xrt_build_date);
        LogMessageNoHeader(LOG_INFO, "Start of session at: " + now);
        LogMessageNoHeader(LOG_INFO, "***************************************************");
    }
    else if (time == "END")
    {
        LogMessageNoHeader(LOG_INFO, "XBTEST: ");
        LogMessageNoHeader(LOG_INFO, "\t - Version: " + XBTEST_VERSION_STR);
        LogMessageNoHeader(LOG_INFO, "\t - SW Build: " + std::to_string(SW_PERFORCE_VERSION) + " on " + SW_BUILD_DATE);
        LogMessageNoHeader(LOG_INFO, "Board: ");
        LogMessageNoHeader(LOG_INFO, "\t - XCLBIN UUID: " + xclbin_uuid);
        LogMessageNoHeader(LOG_INFO, "\t - Firewall status: " + firewall_status);
        LogMessageNoHeader(LOG_INFO, "End of session at: " + now);
        LogMessageNoHeader(LOG_INFO, "***************************************************");
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
std::queue<int> IPC_queue;

int main(int argc, char* argv[])
{
    gGlobal_config.verbosity = DEFAULT_LOG_LEVEL;
    gGlobal_config.use_logging = false;
    gGlobal_config.logging = "";

    // xbtest_failures levels:
    // - after each level propagate failure to next levels
    // - summary is printed depending on level
    enum FailureLevel {
        LEVEL_0             = 0,    // Failure before Test json parsing
        LEVEL_1             = 1,    // Failure after Test json parsing and before test run
        LEVEL_2             = 2,    // Failure after test run / Global failures
        MAX_FAILURE_LEVEL   = 3,    // To keep with max enum value
    };
    bool xbtest_failures[MAX_FAILURE_LEVEL] = {false, false, false};

    std::vector<TestCase*> tc_list;
    std::vector<TestCase*>::iterator tc_list_it;
    std::vector<std::string> test_thread_name_list;

    Json_Parameters_t     device_params;
    Testcase_Parameters_t dma_parameters;
    Testcase_Parameters_t memory_ddr_parameters;
    Testcase_Parameters_t memory_hbm_parameters;
    Testcase_Parameters_t power_parameters;
    Testcase_Parameters_t gt_parameters;
    Testcase_Parameters_t gtmac_parameters;
    Testcase_Parameters_t device_mgt_parameters;
    Json_Parameters_t::iterator it;

    bool hflag = false;
    bool vflag = false;
    bool pflag = false;
    bool jflag = false;
    bool eflag = false;
    bool lflag = false;
    bool dflag = false;
    bool xflag = false;
    bool tflag = false;
    bool bflag = false;

    std::string platform = "";
    std::string json_file_name = "";
    std::string xbtest_pfm_def = "";
    std::string device_idx = "";
    std::string xclbin = "";
    std::string timestamp_mode = "";
    std::string verbosity_str = "";

    // install signal handler
    gAbort = false;
    std::signal(SIGINT, signal_handler);

    // get instance of log
    gLog = Logging::getInstance();

    try
    {
        // xbtest_failures[LEVEL_0]
        gCmd_line = "";
        for(int i = 0; i < argc; ++i)
            gCmd_line += std::string(argv[i]) + " ";

        xbtest_failures[LEVEL_0] = SystemConfig("START", &device_params);

        int test_option;
        bool command_line_failure = false;

        if (xbtest_failures[LEVEL_0] == false)
        {
           // first character of optstring is :
           //   - so it returns ':' instead of '?' to indicate a missing option argument
           //   - getopt will not print error messages, error messages are handled by xbtests
            std::string opt_string = ":hvp:j:e:l:d:x:t:b:";
            while ((test_option = getopt(argc, argv, opt_string.c_str())) != -1)
            {
                char buff [256];
                switch (test_option)
                {
                    case 'p' : // platform
                        if (pflag == true)
                        {
                            LogMessage(LOG_FAILURE, "More than one platform provided");
                            command_line_failure = true;
                        }
                        else
                        {
                            platform = std::string(optarg);
                            pflag = true;
                        }
                        break;

                    case 'j' : // json file
                        if (jflag == true)
                        {
                            LogMessage(LOG_FAILURE, "More than one JSON file provided");
                            command_line_failure = true;
                        }
                        else
                        {
                            json_file_name = std::string(optarg);
                            jflag = true;
                        }
                        break;

                    case 'e' : // xbtest_pfm_def
                        if (eflag == true)
                        {
                            LogMessage(LOG_FAILURE, "More than one Platform definition provided");
                            command_line_failure = true;
                        }
                        else
                        {
                            xbtest_pfm_def = std::string(optarg);
                            eflag = true;
                        }
                        break;

                    case 'l' : // logging
                        if (lflag == true)
                        {
                            LogMessage(LOG_FAILURE, "More than one logging provided");
                            command_line_failure = true;
                        }
                        else
                        {
                            gGlobal_config.use_logging = true;
                            gGlobal_config.logging = std::string(optarg);
                            lflag = true;
                        }
                        break;

                    case 'd' : // device index
                        if (dflag == true)
                        {
                            LogMessage(LOG_FAILURE, "More than one device index provided");
                            command_line_failure = true;
                        }
                        else
                        {
                            device_idx = std::string(optarg);
                            dflag = true;
                        }
                        break;

                    case 'x' : // xclbin
                        if (xflag == true)
                        {
                            LogMessage(LOG_FAILURE, "More than one xclbin provided");
                            command_line_failure = true;
                        }
                        else
                        {
                            xclbin = std::string(optarg);
                            xflag = true;
                        }
                        break;

                    case 't' : // timestamp_mode
                        if (tflag == true)
                        {
                            LogMessage(LOG_FAILURE, "More than one timestamp provided");
                            command_line_failure = true;
                        }
                        else
                        {
                            timestamp_mode = std::string(optarg);
                            tflag = true;
                        }
                        break;

                    case 'b' : // verbosity
                        if (bflag == true)
                        {
                            LogMessage(LOG_FAILURE, "More than one verbosity provided");
                            command_line_failure = true;
                        }
                        else
                        {
                            verbosity_str = std::string(optarg);
                            bflag = true;
                        }
                        break;

                    case 'h' :
                        LogMessage(LOG_INFO, "Usage: " + std::string(argv[0]) + " [options]");
                        LogMessage(LOG_INFO, "Command and option summary:");
                        LogMessage(LOG_INFO, "    -h              : Print this message");
                        LogMessage(LOG_INFO, "    -v              : Print version");
                        LogMessage(LOG_INFO, "    -j json_file    : Specify the file containing test sequences and test environment (card, xclbin ...)");
                        // LogMessage(LOG_INFO, "    -e explication  : Specify the Platform definition (overwrites \"xbtest_pfm_def\" specified in json_file)");
                        // LogMessage(LOG_INFO, "    -b verbosity    : Specify the verbosity level (overwrites \"verbosity\" specified in json_file)");
                        LogMessage(LOG_INFO, "    -l logging      : Specify the directory name to store all log files (overwrites \"logging\" specified in json_file)");
                        // LogMessage(LOG_INFO, "    -p platform     : Specify the name of the shell (overwrites \"device\" specified in json_file)");
                        LogMessage(LOG_INFO, "    -d device_idx   : Specify the index of the card (overwrites \"device_idx\" specified in json_file)");
                        LogMessage(LOG_INFO, "    -x xclbin       : Specify the xclbin (overwrites \"xclbin\" specified in json_file)");
                        LogMessage(LOG_INFO, "For complete documentation, refer to UG1361");
                        hflag = true;
                        break;

                    case 'v' :
                        LogMessage(LOG_INFO, std::string(argv[0]) + " hostcode version " + XBTEST_VERSION_STR);
                        LogMessage(LOG_INFO, "\t Expected HW Compute Unit version:");
                        LogMessage(LOG_INFO, "\t \t DMA   : " + std::to_string(DMA_SW_VERSION_MAJOR)       + "." + std::to_string(DMA_SW_VERSION_MINOR));
                        LogMessage(LOG_INFO, "\t \t Power : " + std::to_string(BI_PWR_HW_VERSION_MAJOR)    + "." + std::to_string(BI_PWR_HW_VERSION_MINOR));
                        LogMessage(LOG_INFO, "\t \t Memory: " + std::to_string(BI_MEM_HW_VERSION_MAJOR)    + "." + std::to_string(BI_MEM_HW_VERSION_MINOR));
                        // LogMessage(LOG_INFO, "\t \t GT    : " + std::to_string(BI_GT_HW_VERSION_MAJOR)     + "." + std::to_string(BI_GT_HW_VERSION_MINOR));
                        LogMessage(LOG_INFO, "\t \t GT MAC: " + std::to_string(BI_GT_MAC_HW_VERSION_MAJOR) + "." + std::to_string(BI_GT_MAC_HW_VERSION_MINOR));
                        vflag = true;
                        break;

                    case ':' :
                        if (isprint (optopt))
                            sprintf(buff, "Option -%c requires an argument", optopt);
                        LogMessage(LOG_FAILURE, std::string(buff));
                        command_line_failure = true;
                        break;

                    default: /* '?' */
                        if (isprint (optopt))
                            sprintf(buff, "Unknown option -%c", optopt);
                        LogMessage(LOG_FAILURE, std::string(buff));
                        command_line_failure = true;
                        break;
                }
            }

           if ((optind < argc) && (command_line_failure == false))
           {
               std::string non_argv = "";
               uint num = 0;
               while (optind < argc)
               {
                   if (num > 0) non_argv += ", ";
                   non_argv += std::string(argv[optind++]);
                   num++;
               }
               LogMessage(LOG_FAILURE, "Found " + std::to_string(num) + " invalid command line option(s): " + non_argv);
               command_line_failure = true;
           }

            if (((hflag == true) || (vflag == true)) && (command_line_failure == false))
            {
                LogMessage(LOG_INFO, "No test performed");
                return EXIT_SUCCESS;
            }

            if ((jflag == false) && (command_line_failure == false))
            {
               LogMessage(LOG_FAILURE, "Required option not found: -j");
               command_line_failure = true;
            }

            if (command_line_failure == true)
            {
                LogMessage(LOG_INFO, "For help, try command line option: -h");
                xbtest_failures[LEVEL_0] = true;
            }
        }


        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Test JSON parser
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        InputParser *input_parser = nullptr;

        if (xbtest_failures[LEVEL_0] == false)
        {
            input_parser = new InputParser(json_file_name, gGlobal_config, &gAbort);
            xbtest_failures[LEVEL_0] |= input_parser->Parse();
        }
        if (xbtest_failures[LEVEL_0] == false) input_parser->ClearParser();

        // Parameters get reset value at the start of input_parser->Parse()
        if (xbtest_failures[LEVEL_0] == false)
        {
            device_params           = input_parser->GetDeviceParameters();
            dma_parameters          = input_parser->GetTestcaseParameters(TEST_DMA);
            memory_ddr_parameters   = input_parser->GetTestcaseParameters(TEST_MEMORY_DDR);
            memory_hbm_parameters   = input_parser->GetTestcaseParameters(TEST_MEMORY_HBM);
            power_parameters        = input_parser->GetTestcaseParameters(TEST_POWER);
            gt_parameters           = input_parser->GetTestcaseParameters(TEST_GT);
            gtmac_parameters        = input_parser->GetTestcaseParameters(TEST_GT_MAC);
            device_mgt_parameters   = input_parser->GetTestcaseParameters(TEST_DEVICE_MGT);
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Update json parameters with command line parameters
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        if (xbtest_failures[LEVEL_0] == false)
        {
            xbtest_failures[LEVEL_0] |= SetCommandLineParam (
                &device_params,
                bflag, verbosity_str,
                pflag, platform,
                eflag, xbtest_pfm_def,
                tflag, timestamp_mode,
                dflag, device_idx,
                xflag, xclbin
            );
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Propagate failure to next level
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        for (uint i = LEVEL_1; i < MAX_FAILURE_LEVEL; i++)
            xbtest_failures[i] |= xbtest_failures[LEVEL_0];
        // xbtest_failures[LEVEL_1]

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Platform def parser
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        XbtestPfmDefParser *xbtest_pfm_def_parser;
        Xbtest_Pfm_Def_t xbtest_pfm_def;

        if (xbtest_failures[LEVEL_1] == false)
        {
            xbtest_pfm_def_parser = new XbtestPfmDefParser(&device_params, gGlobal_config, &gAbort);
            xbtest_failures[LEVEL_1] |= xbtest_pfm_def_parser->Parse();
        }
        if (xbtest_failures[LEVEL_1] == false) xbtest_pfm_def = xbtest_pfm_def_parser->GetPlatformDef();

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Device
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        DeviceInterface *device = nullptr;

        if (xbtest_failures[LEVEL_1] == false) device = new DeviceInterface(xbtest_pfm_def, gGlobal_config, &gAbort);
        if (xbtest_failures[LEVEL_1] == false) xbtest_failures[LEVEL_1] = device->SetupDevice(&device_params);
        if (xbtest_failures[LEVEL_1] == false) xbtest_failures[LEVEL_1] = device->CheckXCLBINDownloadTime();

        uint clock_failure = 0;
        if (xbtest_failures[LEVEL_1] == false)
        {
            clock_failure = device->CheckClocks();
            if (clock_failure == 1) xbtest_failures[LEVEL_1] = true;
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // DMA Test
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        bool dma_internal_abort = false;

        if ((dma_parameters.test_exists == true) && (gAbort == false) && (xbtest_failures[LEVEL_1] == false))
        {
            DMATest *dma_test = new DMATest(xbtest_pfm_def, device, dma_parameters, gGlobal_config);
            TestCase *dma_tc = new TestCase(TESTCASE_TEST, TEST_DMA, static_cast<TestInterface *>(dma_test), gGlobal_config);
            dma_tc->future_result = std::async(std::launch::async, std::bind(&TestCase::SpawnTest, dma_tc));
            tc_list.push_back(dma_tc);
            test_thread_name_list.push_back(TestTypeToString(TEST_DMA));

            // Wait for DMA test to finish at this point
            dma_internal_abort = WaitEndOfTestCases(test_thread_name_list, tc_list, TESTCASE_TEST);
        }

        if (dma_internal_abort == true) gAbort = false; // Overwrite abort generated by DMAtest for verify task

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Device mgt test, don't check if present in json, run anyway
        // Note: Device mgt not design as singleton but other test might call this test directly, so this object must be created before the other
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        DeviceMgt *device_mgt = nullptr;
        if ((gAbort == false) && (xbtest_failures[LEVEL_1] == false))
        {
            device_mgt = new DeviceMgt(xbtest_pfm_def, device, device_mgt_parameters, gGlobal_config);
            TestCase *device_mgt_tc = new TestCase(TESTCASE_TASK, TEST_DEVICE_MGT, static_cast<TestInterface *>(device_mgt), gGlobal_config);
            device_mgt_tc->future_result = std::async(std::launch::async, std::bind(&TestCase::SpawnTest, device_mgt_tc));
            tc_list.push_back(device_mgt_tc);
            test_thread_name_list.push_back(TestTypeToString(TEST_DEVICE_MGT));
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Create kernel, create buffer, set kernel args
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        if ((gAbort == false) && (xbtest_failures[LEVEL_1] == false))
        {
            xbtest_failures[LEVEL_1] = device->SetupKernels();
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Check if test specified in JSON can run
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        if ((gAbort == false) && (xbtest_failures[LEVEL_1] == false))
        {
            for(uint i=0; i < TEST_MAX; i++)
            {
                TestType test_type = (TestType)i;

                Testcase_Parameters_t testcase_parameters;
                switch(test_type)
                {
                    case TEST_DMA:          testcase_parameters = dma_parameters;           break;
                    case TEST_MEMORY_DDR:   testcase_parameters = memory_ddr_parameters;    break;
                    case TEST_MEMORY_HBM:   testcase_parameters = memory_hbm_parameters;    break;
                    case TEST_POWER:        testcase_parameters = power_parameters;         break;
                    case TEST_GT:           testcase_parameters = gt_parameters;            break;
                    case TEST_GT_MAC:       testcase_parameters = gtmac_parameters;         break;
                    default: break;
                }

                if (TestTypeToKernelType(test_type) != KRNL_TYPE_UNKNOWN) // Check only when a kernel has been associated, e.g. not for TEST_DEVICE, TEST_DEVICE_MGT
                {
                    if (testcase_parameters.test_exists == true)
                    {
                        if (device->GetNumKernels(TestTypeToKernelType(test_type)) == 0)
                        {
                            LogMessage(LOG_FAILURE, "Test \"type\": \"" + TestTypeToString(test_type) + "\" is present in Test json file but no Compute Unit detected in the xclbin");
                            xbtest_failures[LEVEL_1] = true;
                            break;
                        }
                        bool mem_exists = true; // Check only for memory
                        if      (test_type == TEST_MEMORY_DDR) mem_exists = xbtest_pfm_def.memory.ddr_exists;
                        else if (test_type == TEST_MEMORY_HBM) mem_exists = xbtest_pfm_def.memory.hbm_exists;
                        if (mem_exists == false)
                        {
                            LogMessage(LOG_FAILURE, "Test \"type\": \"" + TestTypeToString(test_type) + "\" is present in test json file but memory not defined in Platform definition");
                            xbtest_failures[LEVEL_1] = true;
                        }
                    }
                }
            }
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Verify test
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        if ((gAbort == false) && (xbtest_failures[LEVEL_1] == false))
        {
            std::vector<TestCase*> verify_tc_list;
            std::vector<std::string> verify_test_thread_name_list;

            VerifyTest *verify_test = new VerifyTest(device, gGlobal_config);
            TestCase *verify_tc = new TestCase(TESTCASE_TEST, TEST_VERIFY, static_cast<TestInterface *>(verify_test), gGlobal_config);
            verify_tc->future_result = std::async(std::launch::async, std::bind(&TestCase::SpawnTest, verify_tc));
            verify_tc_list.push_back(verify_tc);
            verify_test_thread_name_list.push_back(TestTypeToString(TEST_VERIFY));

            WaitEndOfTestCases(verify_test_thread_name_list, verify_tc_list, TESTCASE_TEST); // Wait for verify test to finish at this point
            if (verify_tc->future_result.get() != TestCase::TestCaseThreadResult::TC_PASS)
                xbtest_failures[LEVEL_1] = true;
        }

        if (dma_internal_abort == true) gAbort = true; // Revert overwrite abort generated by DMAtest for verify

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Power Test
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        if ((power_parameters.test_exists == true) && (gAbort == false) && (xbtest_failures[LEVEL_1] == false))
        {
            PowerTest *power_test = new PowerTest(xbtest_pfm_def, device, device_mgt, power_parameters, gGlobal_config);
            TestCase *power_tc = new TestCase(TESTCASE_TEST, TEST_POWER, static_cast<TestInterface *>(power_test), gGlobal_config);
            power_tc->future_result = std::async(std::launch::async, std::bind(&TestCase::SpawnTest, power_tc));
            tc_list.push_back(power_tc);
            test_thread_name_list.push_back(TestTypeToString(TEST_POWER));
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // GT Test
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        if ((gt_parameters.test_exists == true) && (gAbort == false) && (xbtest_failures[LEVEL_1] == false))
        {
            for (int kernel_idx=0; kernel_idx<device->GetNumGTKernels(); kernel_idx++)
            {
                GTTest *gt_test = new GTTest(device, gt_parameters, kernel_idx, gGlobal_config);
                TestCase *gt_tc = new TestCase(TESTCASE_TEST, TEST_GT, static_cast<TestInterface *>(gt_test), gGlobal_config);
                gt_tc->future_result = std::async(std::launch::async, std::bind(&TestCase::SpawnTest, gt_tc));
                tc_list.push_back(gt_tc);
                test_thread_name_list.push_back(TestTypeToString(TEST_GT) + std::to_string(kernel_idx));
            }
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // GT MAC Test
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        if ((gtmac_parameters.test_exists == true) && (gAbort == false) && (xbtest_failures[LEVEL_1] == false))
        {
            for (int kernel_idx=0; kernel_idx<device->GetNumGTMACKernels(); kernel_idx++)
            {
                GTMACTest *gt_mac_test = new GTMACTest(xbtest_pfm_def, device, device_mgt, gtmac_parameters, kernel_idx, gGlobal_config);
                TestCase *gt_mac_tc = new TestCase(TESTCASE_TEST, TEST_GT_MAC, static_cast<TestInterface *>(gt_mac_test), gGlobal_config);
                gt_mac_tc->future_result = std::async(std::launch::async, std::bind(&TestCase::SpawnTest, gt_mac_tc));
                tc_list.push_back(gt_mac_tc);
                test_thread_name_list.push_back(TestTypeToString(TEST_GT_MAC) + std::to_string(kernel_idx));
            }
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Memory Test (DDR + HBM)
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        for (auto const& test_type : {TEST_MEMORY_DDR, TEST_MEMORY_HBM})
        {
            int num_krnls;
            std::string type_member_str;
            Testcase_Parameters_t testcase_parameters;
            if (test_type == TEST_MEMORY_DDR)
            {
                num_krnls = device->GetNumMemDDRKernels();
                testcase_parameters = memory_ddr_parameters;
            }
            else if (test_type == TEST_MEMORY_HBM)
            {
                num_krnls = device->GetNumMemHBMKernels();
                testcase_parameters = memory_hbm_parameters;
            }
            if ((testcase_parameters.test_exists == true) && (gAbort == false) && (xbtest_failures[LEVEL_1] == false))
            {
                for (int kernel_idx=0; kernel_idx<num_krnls; kernel_idx++)
                {
                    MemoryTest *memory_test = new MemoryTest(xbtest_pfm_def, device, device_mgt, testcase_parameters, test_type, kernel_idx, gGlobal_config);
                    TestCase *memory_tc = new TestCase(TESTCASE_TEST, test_type, static_cast<TestInterface *>(memory_test), gGlobal_config);
                    memory_tc->future_result = std::async(std::launch::async, std::bind(&TestCase::SpawnTest, memory_tc));
                    tc_list.push_back(memory_tc);

                    std::string test_thread_name;
                    if (test_type == TEST_MEMORY_DDR)
                        test_thread_name = "MEMORY_TEST " + device->GetMemKernelDDRTag(kernel_idx, 0);  // Only 1 port for DDR memory test kernel
                    else if (test_type == TEST_MEMORY_HBM)
                        test_thread_name = "MEMORY_TEST HBM";

                    test_thread_name_list.push_back(test_thread_name);
                }
            }
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Propagate failure to next level
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        for (uint i = LEVEL_2; i < MAX_FAILURE_LEVEL; i++)
            xbtest_failures[i] |= xbtest_failures[LEVEL_1];
        // xbtest_failures[LEVEL_2]

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Wait all test completion and manage interrupt
        // then stop and wait for the end of all tasks
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Wait end of test ONLY
        WaitEndOfTestCases(test_thread_name_list, tc_list, TESTCASE_TEST);

        // stop tasks as task doesn't use on testcase complete to stop
        SignalAbortTestCases(tc_list, TESTCASE_TASK);

        // Wait end of tasks
        WaitEndOfTestCases(test_thread_name_list, tc_list, TESTCASE_TASK);

        // Check if a task has failed
        for (tc_list_it = tc_list.begin(); tc_list_it != tc_list.end(); tc_list_it++)
        {
            if ((*tc_list_it)->GetTestCaseType() == TESTCASE_TASK)
            {
                TestCase::TestCaseThreadResult test_result = (*tc_list_it)->future_result.get();
                if (test_result != TestCase::TestCaseThreadResult::TC_PASS)
                    xbtest_failures[LEVEL_2] = true;
            }
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Output test results
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        LogMessageNoHeader(LOG_INFO, "********************* SUMMARY *********************");
        xbtest_failures[LEVEL_2] |= SystemConfig("END", &device_params);

        if (xbtest_failures[LEVEL_0] == false) // Don't print Device and Test params at level 0 failures because it is before Test JSON parsing
        {
            input_parser->SetLogMsgTestType(""); // Remove line header to print parameters
            input_parser->PrintJsonParameters (LOG_INFO, TEST_DEVICE, device_params);
            LogMessageNoHeader(LOG_INFO, "***************************************************");
            if (xbtest_failures[LEVEL_1] == false) // Don't print Device and Test params at level 0 failures because it is before Test are run
            {
                for (tc_list_it = tc_list.begin(); tc_list_it != tc_list.end(); tc_list_it++)
                {
                    if ((*tc_list_it)->GetTestCaseType() == TESTCASE_TEST)
                    {
                        int tc_idx = std::distance(tc_list.begin(), tc_list_it);
                        TestType test_type = (*tc_list_it)->GetTestType();
                        TestCase::TestCaseThreadResult test_result = (*tc_list_it)->future_result.get();
                        Testcase_Parameters_t testcase_parameters;
                        switch (test_type)
                        {
                            case TEST_DMA:         testcase_parameters = dma_parameters;         break;
                            case TEST_MEMORY_DDR:  testcase_parameters = memory_ddr_parameters;  break;
                            case TEST_MEMORY_HBM:  testcase_parameters = memory_hbm_parameters;  break;
                            case TEST_POWER:       testcase_parameters = power_parameters;       break;
                            case TEST_GT:          testcase_parameters = gt_parameters;          break;
                            case TEST_GT_MAC:      testcase_parameters = gtmac_parameters;       break;
                            case TEST_DEVICE_MGT:  testcase_parameters = device_mgt_parameters;  break;
                            default: break;
                        }
                        // if test is found then display what is found, except for verify and device_mgt
                        if (testcase_parameters.test_exists == true)
                        {
                            input_parser->PrintJsonParameters (LOG_INFO, test_type, testcase_parameters.param);
                            switch(test_result)
                            {
                                default:                                            LogMessageNoHeader(LOG_FAILURE,   test_thread_name_list[tc_idx] + " TEST UNKNOWN STATE");   xbtest_failures[LEVEL_2] |= true;   break;
                                case TestCase::TestCaseThreadResult::TC_ABORTED:    LogMessageNoHeader(LOG_FAILURE,   test_thread_name_list[tc_idx] + " TEST ABORTED");         xbtest_failures[LEVEL_2] |= true;   break;
                                case TestCase::TestCaseThreadResult::TC_FAIL:       LogMessageNoHeader(LOG_ERROR,     test_thread_name_list[tc_idx] + " TEST FAILED");          xbtest_failures[LEVEL_2] |= true;   break;
                                case TestCase::TestCaseThreadResult::TC_PASS:       LogMessageNoHeader(LOG_PASS,      test_thread_name_list[tc_idx] + " TEST PASSED");                                              break;
                            }
                            LogMessageNoHeader(LOG_INFO, "***************************************************");
                        }
                    }
                }
                xbtest_failures[LEVEL_2] |= gAbort; // Make sure abort is reported even if already detected when TC state is ABORT
            }
        }
        // Propagate failure to next level
        for (uint i = LEVEL_2; i < MAX_FAILURE_LEVEL; i++)
            xbtest_failures[i] |= xbtest_failures[LEVEL_2];
        // Propagate clock failure
        if (clock_failure == 2) xbtest_failures[MAX_FAILURE_LEVEL-1] = true;

        std::string msg_count_str;
        xbtest_failures[LEVEL_2] |= gLog->GetReportMsgCount(&msg_count_str);
        LogMessageNoHeader(LOG_INFO,  msg_count_str);

        std::string first_error = gLog->GetFirstError();
        if (first_error != "")
            LogMessageNoHeader(LOG_INFO, "FIRST_ERROR: " + first_error);

        LogMessageNoHeader(LOG_INFO, "***************************************************");
        if (xbtest_failures[MAX_FAILURE_LEVEL-1] == false)
            LogMessageNoHeader(LOG_PASS,  "RESULT: ALL TESTS PASSED");
        else
            LogMessageNoHeader(LOG_ERROR, "RESULT: SOME TESTS FAILED");
        LogMessageNoHeader(LOG_INFO, "***************************************************");

    }
    catch (const std::exception& ex) // catch all thrown exceptions, should not occur
    {
        LogMessageNoHeader(LOG_FAILURE, "Exception Caught: " + std::string(ex.what()));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint GetNumTestCasesNotCompleted( std::vector<TestCase*> tc_list, TestCaseType test_case_type )
{
    uint tc_count = 0;
    std::vector<TestCase*>::iterator tc_list_it;
    for (tc_list_it = tc_list.begin(); tc_list_it != tc_list.end(); tc_list_it++)
    {
        if((*tc_list_it)->GetTestComplete() == false)
        {
            if ((*tc_list_it)->GetTestCaseType() == test_case_type)
                tc_count++;
        }
    }
    return tc_count;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CheckTestCasesCompleted( std::vector<std::string> test_thread_name_list, std::vector<TestCase*> tc_list )
{
    bool internal_abort = false;
    std::vector<TestCase*>::iterator tc_list_it;

    for (tc_list_it = tc_list.begin(); tc_list_it != tc_list.end(); tc_list_it++)
    {
        int tc_idx = std::distance(tc_list.begin(), tc_list_it);

        // no need to check test if already completed or if abort has been detected
        if ( ((*tc_list_it)->GetTestComplete() == false) && (internal_abort == false) )
        {
            std::future_status status = (*tc_list_it)->future_result.wait_for(std::chrono::seconds(1)); // block for a second and check if thread has complete
            if(status == std::future_status::ready) // thread has completed
            {
                if((*tc_list_it)->CheckTestAborted() == true) // thread has completed, check if it has aborted
                {
                    LogMessage(LOG_DEBUG, test_thread_name_list[tc_idx] + " internally aborted");
                    gAbort = true; // set flag to abort all other thread
                    internal_abort = true;
                }
                (*tc_list_it)->SetTestComplete(); // complete test
            }
        }
    }
    return internal_abort;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SignalAbortTestCases( std::vector<TestCase*> tc_list, TestCaseType test_case_type )
{
    if (test_case_type == TESTCASE_TEST)
    {
        LogMessage(LOG_DEBUG, "Propagate abort to all testcases");
    }
    else
    {
        // task doesn't abort but they are stopped
        LogMessage(LOG_DEBUG, "Propagate stop to all tasks");
    }

    std::vector<TestCase*>::iterator tc_list_it;
    for (tc_list_it = tc_list.begin(); tc_list_it != tc_list.end(); tc_list_it++)
    {
        if ((*tc_list_it)->GetTestComplete() == false)
        {
            if ((*tc_list_it)->GetTestCaseType() == test_case_type)
                (*tc_list_it)->SignalAbortTest();
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CheckTestCasesAborted( std::vector<std::string> test_thread_name_list, std::vector<TestCase*> tc_list, TestCaseType test_case_type )
{
    std::vector<TestCase*>::iterator tc_list_it;
    for (tc_list_it = tc_list.begin(); tc_list_it != tc_list.end(); tc_list_it++)
    {
        int tc_idx = std::distance(tc_list.begin(), tc_list_it);
        if ((*tc_list_it)->GetTestComplete() == false) // no need to check test if already completed, and wait for abort to be detected
        {
            if ((*tc_list_it)->GetTestCaseType() == test_case_type)
            {
                std::future_status status = (*tc_list_it)->future_result.wait_for(std::chrono::milliseconds(100)); // block for a second and check if thread has complete
                if (status == std::future_status::ready) // thread has completed
                {
                    (*tc_list_it)->SetTestComplete(); // complete test
                    if ((*tc_list_it)->CheckTestAborted() == false) // thread has completed, check if it has aborted
                        LogMessage(LOG_DEBUG, test_thread_name_list[tc_idx] + " already ended before being aborted");
                    if (GetNumTestCasesNotCompleted(tc_list, test_case_type) == 0)
                        LogMessage(LOG_DEBUG, "All threads correctly aborted");
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool WaitEndOfTestCases( std::vector<std::string> test_thread_name_list, std::vector<TestCase*> tc_list, TestCaseType test_case_type )
{
    bool internal_abort = false;
    bool abort_propagated = false;
    while (GetNumTestCasesNotCompleted(tc_list, test_case_type) > 0)
    {
        if (gAbort == false) // Manage end of test without abort and thread with internal abort
        {
            internal_abort = CheckTestCasesCompleted(test_thread_name_list, tc_list); // Check for all testcase type and if abort
        }
        else
        {
            if (abort_propagated == false)
            {
                // if abort detected, signal abort to all thread not completed
                SignalAbortTestCases(tc_list, test_case_type);
                abort_propagated = true;
            }
            CheckTestCasesAborted(test_thread_name_list, tc_list, test_case_type); // Check thread aborted
        }
    }
    return internal_abort;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CheckStringInSet ( std::string value, std::set<std::string> test_set )
{
    bool ret_failure = false;

    std::set<std::string> test_set_lowercase;

    for (auto test : test_set)
    {
        std::transform(test.begin(), test.end(), test.begin(), tolower);
        test_set_lowercase.insert(test);
    }
    std::string value_lower = value;
    std::transform(value_lower.begin(), value_lower.end(), value_lower.begin(), tolower);

    if (test_set_lowercase.count(value_lower)==0)
    {
        std::string set_str = "";
        for (auto f : test_set) {
            set_str += "\"" + f + "\", ";
        }
        LogMessage(LOG_FAILURE, "Invalid value : \"" + value + "\"");
        LogMessage(LOG_DESIGNER, "Supported values : " + set_str);
        ret_failure = true;
    }

    return ret_failure;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool SetCommandLineParam(
    Json_Parameters_t *device_params,
    bool bflag, std::string verbosity_str,
    bool pflag, std::string platform,
    bool eflag, std::string xbtest_pfm_def,
    bool tflag, std::string timestamp_mode,
    bool dflag, std::string device_idx,
    bool xflag, std::string xclbin
)
{
    Json_Parameters_t::iterator it;
    bool verbosity_failure = false;
    if (bflag == true)
    {
        LogMessage(LOG_INFO, "Using verbosity provided in command line: " + verbosity_str);
        int verbosity;
        if (ConvString2Num<int>(verbosity_str, &verbosity))
            verbosity_failure = true;
        if (verbosity_failure == false)
        {
            if ((verbosity < -2) || (verbosity > 6))
                verbosity_failure = true;
        }

        if (verbosity_failure == true)
        {
            LogMessage(LOG_FAILURE, "Wrong value for verbosity: " + std::to_string(verbosity) + ". Expected [0;6]");
            return true;
        }
        else
        {
            gGlobal_config.verbosity = static_cast<LogLevel>(verbosity);
        }
    }
    else
    {
        int verbosity = (int)gGlobal_config.verbosity;
        verbosity_failure = GetVerbosity(device_params, &verbosity);
        if (verbosity_failure == true)
        {
            LogMessage(LOG_FAILURE, VERBOSITY_FAILURE);
            return true;
        }
        else
        {
            gGlobal_config.verbosity = static_cast<LogLevel>(verbosity);
        }
    }

    it = FindJsonParam(device_params, DEVICE_MEMBER);
    if (pflag == true)
    {
        LogMessage(LOG_INFO, "Using platform provided in command line: " + platform);
        if (it != device_params->end())
            EraseJsonParam(device_params, DEVICE_MEMBER);
        InsertJsonParam<std::string>(device_params, DEVICE_MEMBER, platform);
    }

    it = FindJsonParam(device_params, XBTEST_PFM_DEF_MEMBER);
    if (eflag == true)
    {
        LogMessage(LOG_INFO, "Using Platform definition provided in command line: " + xbtest_pfm_def);
        if (it != device_params->end())
            EraseJsonParam(device_params, XBTEST_PFM_DEF_MEMBER);
        InsertJsonParam<std::string>(device_params, XBTEST_PFM_DEF_MEMBER, xbtest_pfm_def);
    }

    it = FindJsonParam(device_params, TIMESTAMP_MEMBER);
    if (tflag == true)
    {
        LogMessage(LOG_INFO, "Using timestamp provided in command line: " + timestamp_mode);
        if (it != device_params->end())
            EraseJsonParam(device_params, TIMESTAMP_MEMBER);
        InsertJsonParam<std::string>(device_params, TIMESTAMP_MEMBER, timestamp_mode);
    }

    it = FindJsonParam(device_params, TIMESTAMP_MEMBER);
    if (it != device_params->end())
    {
        std::string timestamp_mode = TestcaseParamCast<std::string>(it->second);
        if (CheckStringInSet(timestamp_mode, SUPPORTED_TIMESTAMP_MODE) == true) return true;
        gLog->SetTimestampMode(timestamp_mode);
    }

    it = FindJsonParam(device_params, LOGGING_MEMBER);
    if (gGlobal_config.use_logging == true)
    {
        LogMessage(LOG_INFO, "Using logging provided in command line: " + gGlobal_config.logging);
        if (it != device_params->end())
            EraseJsonParam(device_params, LOGGING_MEMBER);
        InsertJsonParam<std::string>(device_params, LOGGING_MEMBER, gGlobal_config.logging);
    }
    else
    {
        if (it != device_params->end())
        {
            // Using logging provided in JSON
            gGlobal_config.use_logging = true;
            gGlobal_config.logging = TestcaseParamCast<std::string>(it->second);
        }
    }
    if (gLog->SetLoggingMode(GENERAL, gGlobal_config) == true) return true;

    it = FindJsonParam(device_params, DEVICE_IDX_MEMBER);
    if (dflag == true)
    {
        uint device_idx_int;
        if (ConvString2Num<uint>(device_idx, &device_idx_int) == true)
        {
            LogMessage(LOG_FAILURE, "Failed to convert device_idx provided in command line: " + device_idx);
            return true;
        }
        LogMessage(LOG_INFO, "Using device_idx provided in command line: " + std::to_string(device_idx_int));
        if (it != device_params->end())
            EraseJsonParam(device_params, DEVICE_IDX_MEMBER);
        InsertJsonParam<uint>(device_params, DEVICE_IDX_MEMBER, device_idx_int);
    }

    it = FindJsonParam(device_params, XCLBIN_MEMBER);
    if (xflag == true)
    {
        LogMessage(LOG_INFO, "Using xclbin provided in command line: " + xclbin);
        if (it != device_params->end())
            EraseJsonParam(device_params, XCLBIN_MEMBER);
        InsertJsonParam<std::string>(device_params, XCLBIN_MEMBER, xclbin);
    }

    return false;
}
