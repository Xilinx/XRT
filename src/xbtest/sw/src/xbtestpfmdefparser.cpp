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

#include "xbtestpfmdefparser.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

XbtestPfmDefParser::XbtestPfmDefParser( Json_Parameters_t *device_params, Global_Config_t global_config, std::atomic<bool> *abort )
{
    m_log = Logging::getInstance(); // handle to the logging
    m_log_msg_test_type = "XBT_PFM_DEF: ";
    m_global_config = global_config;
    m_device_params = device_params;
    m_abort = abort;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

XbtestPfmDefParser::~XbtestPfmDefParser() {
    ClearParser();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool XbtestPfmDefParser::Parse()
{
    bool ret_failure = false;

    /////////////////////////////////////////////////////////////////////////////////
    // Get input parameters
    /////////////////////////////////////////////////////////////////////////////////
    Json_Parameters_t::iterator it;

    // Get device in test JSON
    it = FindJsonParam(m_device_params, DEVICE_MEMBER);
    if (it != m_device_params->end())
    {
        m_device = TestcaseParamCast<std::string>(it->second);
        LogMessage(LOG_DEBUG, "Using \"" + DEVICE_MEMBER.name + "\": " + m_device);
    }
    else
    {
        LogMessage(LOG_FAILURE, "\"" + DEVICE_MEMBER.name + "\" must be defined in test json file or in command line");
        return RET_FAILURE;
    }

    // Default xbtest_pfm_def file location
    m_filename  = "/opt/xilinx/dsa/" + m_device + "/test/" + PLATDEF_JSON_NAME;

    // xbtest_pfm_def file location overwrite in test JSON
    it = FindJsonParam(m_device_params, XBTEST_PFM_DEF_MEMBER);
    if (it != m_device_params->end())
    {
        m_filename = TestcaseParamCast<std::string>(it->second);
    }
    else
    {
        LogMessage(LOG_INFO, "Using default Platform definition: " + m_filename);
        InsertJsonParam(m_device_params, XBTEST_PFM_DEF_MEMBER, m_filename);
    }

    /////////////////////////////////////////////////////////////////////////////////
    // Parse JSON file
    /////////////////////////////////////////////////////////////////////////////////
    // check json configuration file exists
    std::ifstream infile(m_filename);
    if (infile.good() == false)
    {
        LogMessage(LOG_FAILURE, "Platform definition does not exist: " + m_filename);
        LogMessage(LOG_INFO,    "Check " + m_filename + " or overwrite this path in test JSON file using member: \"" + XBTEST_PFM_DEF_MEMBER.name + "\"");
        return RET_FAILURE;
    }

    // Initialize json parser and reader
    LogMessage(LOG_INFO, "Using Platform definition: " + m_filename);
    m_json_parser = json_parser_new();

    GError *error = NULL;
    json_parser_load_from_file (m_json_parser, m_filename.c_str(), &error);
    if (error)
    {
        LogMessage(LOG_FAILURE, "Unable to parse Platform definition: " + std::string(error->message));
        g_error_free (error);
        g_object_unref (m_json_parser);
        return RET_FAILURE;
    }

    m_json_root_node = json_parser_get_root(m_json_parser);
    m_json_reader = json_reader_new(NULL);
    json_reader_set_root (m_json_reader, m_json_root_node);

    /////////////////////////////////////////////////////////////////////////////////
    // Check the json file content against the defined members
    /////////////////////////////////////////////////////////////////////////////////
    LogMessage(LOG_DEBUG, "Check the JSON file content");
    m_json_definition = GetJsonDefinition();
    if (CheckMembers(m_json_definition) == RET_FAILURE)
        return RET_FAILURE;

    /////////////////////////////////////////////////////////////////////////////////
    // Get JSON parameters
    /////////////////////////////////////////////////////////////////////////////////
    LogMessage(LOG_DEBUG, "Get JSON parameters");
    std::vector<std::string> node_title;
    std::vector<std::string> node_title_2;
    std::vector<std::string> node_title_3;

    ///////////////////////////
    // Version
    ///////////////////////////
    node_title = {VERSION};
    ret_failure |= ExtractNodeValueStr(node_title, &(m_version));
    if (ret_failure == RET_FAILURE)
    {
        PrintRequiredNotFound(node_title);
        return RET_FAILURE;
    }
    if ((StrMatchNoCase(m_version, PLATDEF_JSON_VERSION) == false) && (ret_failure == false))
    {
        LogMessage(LOG_FAILURE, "Incorrect Platform definition version: " + m_version + ". Expected: " + PLATDEF_JSON_VERSION);
        return RET_FAILURE;
    }

    ///////////////////////////
    // Device name
    ///////////////////////////
    node_title = {DEVICE, INFO, NAME};
    ret_failure |= ExtractNodeValueStr(node_title, &(m_xbtest_pfm_def.info.name));
    if (ret_failure == RET_FAILURE)
    {
        PrintRequiredNotFound(node_title);
        return RET_FAILURE;
    }

    if ((StrMatchNoCase(m_xbtest_pfm_def.info.name, m_device) == false) && (ret_failure == false))
    {
        LogMessage(LOG_FAILURE, "Device name in Platform definition: " + m_xbtest_pfm_def.info.name + " does not match Test JSON: " + m_device);
        return RET_FAILURE;
    }

    ///////////////////////////
    // Clocks
    //////////////////////////
    m_xbtest_pfm_def.info.num_clocks = 0; // init
    node_title = {DEVICE, INFO, CLOCKS};
    if (NodeExists(node_title) == true)
    {
        // Check if first index exists
        node_title = {DEVICE, INFO, CLOCKS, "0"};
        if (NodeExists(node_title) == false)
        {
            PrintRequiredNotFound(node_title);
            return RET_FAILURE;
        }
        // Get clocks
        for (uint idx = 0; idx < MAX_CLOCKS; idx++)
        {
            Xbtest_Pfm_Def_Clock_t clock;

            node_title = {DEVICE, INFO, CLOCKS, std::to_string(idx)};
            if (NodeExists(node_title) == false)
            {
                break; // Finished get clocks
            }
            else
            {
                // Get name
                node_title = {DEVICE, INFO, CLOCKS, std::to_string(idx), NAME};
                ret_failure |= ExtractNodeArrayStr(node_title, &(clock.name));
                if (ret_failure == RET_FAILURE)
                {
                    PrintRequiredNotFound(node_title);
                    return RET_FAILURE;
                }

                // Get frequency
                node_title = {DEVICE, INFO, CLOCKS, std::to_string(idx), FREQUENCY};
                ret_failure |= ExtractNodeValueInt<uint>(node_title, &(clock.frequency));
                if (ret_failure == RET_FAILURE)
                {
                    PrintRequiredNotFound(node_title);
                    return RET_FAILURE;
                }
                m_xbtest_pfm_def.info.clocks.push_back(clock);
                m_xbtest_pfm_def.info.num_clocks++;
            }
        }
    }

    ///////////////////////////
    // Download time
    ///////////////////////////
    m_xbtest_pfm_def.runtime.download_time = DEFAULT_DOWNLOAD_TIME;
    node_title = {DEVICE, RUNTIME, DOWNLOAD_TIME};
    if (NodeExists(node_title) == true)
    {
        ret_failure |= ExtractNodeValueInt<int>(node_title, &(m_xbtest_pfm_def.runtime.download_time));
        if (ret_failure == RET_FAILURE)
            return RET_FAILURE;
    }
    ///////////////////////////
    // calibration
    //////////////////////////
    // Get a, b, c
    node_title = {DEVICE, PHYSICAL, THERMAL, CALIBRATION, A};
    ret_failure |= ExtractNodeValueDouble<double>(node_title, &(m_xbtest_pfm_def.physical.thermal.calibration.a));
    if (ret_failure == RET_FAILURE)
    {
        PrintRequiredNotFound(node_title);
        return RET_FAILURE;
    }

    node_title = {DEVICE, PHYSICAL, THERMAL, CALIBRATION, B};
    ret_failure |= ExtractNodeValueDouble<double>(node_title, &(m_xbtest_pfm_def.physical.thermal.calibration.b));
    if (ret_failure == RET_FAILURE)
    {
        PrintRequiredNotFound(node_title);
        return RET_FAILURE;
    }

    node_title = {DEVICE, PHYSICAL, THERMAL, CALIBRATION, C};
    ret_failure |= ExtractNodeValueDouble<double>(node_title, &(m_xbtest_pfm_def.physical.thermal.calibration.c));
    if (ret_failure == RET_FAILURE)
    {
        PrintRequiredNotFound(node_title);
        return RET_FAILURE;
    }


    ///////////////////////////
    // XPE_LEAKAGE
    //////////////////////////
    // default value
    m_xbtest_pfm_def.physical.thermal.xpe_leakage.a = 0.0;
    m_xbtest_pfm_def.physical.thermal.xpe_leakage.b = 0.0;
    m_xbtest_pfm_def.physical.thermal.xpe_leakage.c = 0.0;
    // Get a, b, c
    node_title   = {DEVICE, PHYSICAL, THERMAL, XPE_LEAKAGE, A};
    node_title_2 = {DEVICE, PHYSICAL, THERMAL, XPE_LEAKAGE, B};
    node_title_3 = {DEVICE, PHYSICAL, THERMAL, XPE_LEAKAGE, C};

    // if 1 of the node is defined, the 2 others must be there too
    if ( (NodeExists(node_title) == true) || (NodeExists(node_title_2) == true) || (NodeExists(node_title_3) == true) )
    {
        if (NodeExists(node_title) == false)
        {
            PrintRequiredNotFound(node_title);
            return RET_FAILURE;
        }
        if (NodeExists(node_title_2) == false)
        {
            PrintRequiredNotFound(node_title_2);
            return RET_FAILURE;
        }
        if (NodeExists(node_title_3) == false)
        {
            PrintRequiredNotFound(node_title_3);
            return RET_FAILURE;
        }

        ret_failure |= ExtractNodeValueDouble<double>(node_title  , &(m_xbtest_pfm_def.physical.thermal.xpe_leakage.a));
        ret_failure |= ExtractNodeValueDouble<double>(node_title_2, &(m_xbtest_pfm_def.physical.thermal.xpe_leakage.b));
        ret_failure |= ExtractNodeValueDouble<double>(node_title_3, &(m_xbtest_pfm_def.physical.thermal.xpe_leakage.c));
    }

    ///////////////////////////
    // Temperature sources
    //////////////////////////
    m_xbtest_pfm_def.physical.thermal.num_temp_sources = 0; // init
    node_title = {DEVICE, PHYSICAL, THERMAL, TEMP_SOURCES};
    if (NodeExists(node_title) == true)
    {
        // Check if first index exists
        node_title = {DEVICE, PHYSICAL, THERMAL, TEMP_SOURCES, "0"};
        if (NodeExists(node_title) == false)
        {
            PrintRequiredNotFound(node_title);
            return RET_FAILURE;
        }
        // Get temperature sources
        for (uint idx = 0; idx < MAX_TEMP_SOURCES; idx++)
        {
            Xbtest_Pfm_Def_Temp_Src_t temp_source;

            node_title = {DEVICE, PHYSICAL, THERMAL, TEMP_SOURCES, std::to_string(idx)};
            if (NodeExists(node_title) == false)
            {
                break; // Finished get temperature sources
            }
            else
            {
                // Get name
                node_title = {DEVICE, PHYSICAL, THERMAL, TEMP_SOURCES, std::to_string(idx), NAME};
                ret_failure |= ExtractNodeArrayStr(node_title, &(temp_source.name));
                if (ret_failure == RET_FAILURE)
                {
                    PrintRequiredNotFound(node_title);
                    return RET_FAILURE;
                }

                // Get limit
                temp_source.limit = DEFAULT_LIMIT;
                node_title = {DEVICE, PHYSICAL, THERMAL, TEMP_SOURCES, std::to_string(idx), LIMIT};
                if (NodeExists(node_title) == true)
                {
                    ret_failure |= ExtractNodeValueInt<int>(node_title, &(temp_source.limit));
                    if (ret_failure == RET_FAILURE)
                        return RET_FAILURE;
                }

                // Get log_name
                temp_source.source_name = "Temperature[" + std::to_string(m_xbtest_pfm_def.physical.thermal.num_temp_sources) + "]";
                node_title = {DEVICE, PHYSICAL, THERMAL, TEMP_SOURCES, std::to_string(idx), SOURCE_NAME};
                if (NodeExists(node_title) == true)
                {
                    ret_failure |= ExtractNodeValueStr(node_title, &(temp_source.source_name));
                    if (ret_failure == RET_FAILURE)
                        return RET_FAILURE;
                    temp_source.source_name += " Temperature";
                }
                m_xbtest_pfm_def.physical.thermal.temp_sources.push_back(temp_source);
                m_xbtest_pfm_def.physical.thermal.num_temp_sources++;
            }
        }
    }

    ///////////////////////////
    // Power target
    //////////////////////////
    node_title = {DEVICE, PHYSICAL, POWER, POWER_TARGET, MIN};
    ret_failure |= ExtractNodeValueInt<uint>(node_title, &(m_xbtest_pfm_def.physical.power.power_target.min));
    if (ret_failure == RET_FAILURE)
    {
        PrintRequiredNotFound(node_title);
        return RET_FAILURE;
    }
    node_title = {DEVICE, PHYSICAL, POWER, POWER_TARGET, MAX};
    ret_failure |= ExtractNodeValueInt<uint>(node_title, &(m_xbtest_pfm_def.physical.power.power_target.max));
    if (ret_failure == RET_FAILURE)
    {
        PrintRequiredNotFound(node_title);
        return RET_FAILURE;
    }

    ///////////////////////////
    // Power sources
    //////////////////////////
    m_xbtest_pfm_def.physical.power.num_power_sources = 0; // init
    node_title = {DEVICE, PHYSICAL, POWER, POWER_SOURCES};
    if (NodeExists(node_title) == false)
    {
        PrintRequiredNotFound(node_title);
        return RET_FAILURE;
    }

    // Check if first index exists
    node_title = {DEVICE, PHYSICAL, POWER, POWER_SOURCES, "0"};
    if (NodeExists(node_title) == false)
    {
        PrintRequiredNotFound(node_title);
        return RET_FAILURE;
    }

    m_xbtest_pfm_def.physical.power.max_calibration = 0;
    // Get power sources
    for (uint idx = 0; idx < MAX_POWER_SOURCES; idx++)
    {
        Xbtest_Pfm_Def_Pwr_Src_t power_source;

        node_title = {DEVICE, PHYSICAL, POWER, POWER_SOURCES, std::to_string(idx)};
        if (NodeExists(node_title) == false)
        {
            break; // Finished get power sources
        }
        else
        {
            bool name_valid = false;
            // Get name
            node_title = {DEVICE, PHYSICAL, POWER, POWER_SOURCES, std::to_string(idx), NAME};
            if (NodeExists(node_title) == true)
            {
                ret_failure |= ExtractNodeArrayStr(node_title, &(power_source.name));
                if (ret_failure == RET_FAILURE)
                    return RET_FAILURE;
                name_valid = true;
                power_source.def_by_curr_volt = false;
            }

            if (name_valid == false)
            {
                // Get name_current
                bool name_curr_valid = false;
                node_title = {DEVICE, PHYSICAL, POWER, POWER_SOURCES, std::to_string(idx), NAME_CURRENT};
                if (NodeExists(node_title) == true)
                {
                    ret_failure |= ExtractNodeArrayStr(node_title, &(power_source.name_current));
                    if (ret_failure == RET_FAILURE)
                        return RET_FAILURE;
                    name_curr_valid = true;
                }

                // Get name_voltage
                bool name_volt_valid = false;
                node_title = {DEVICE, PHYSICAL, POWER, POWER_SOURCES, std::to_string(idx), NAME_VOLTAGE};
                if (NodeExists(node_title) == true)
                {
                    ret_failure |= ExtractNodeArrayStr(node_title, &(power_source.name_voltage));
                    if (ret_failure == RET_FAILURE)
                        return RET_FAILURE;
                    name_volt_valid = true;
                    power_source.def_by_curr_volt = true;
                }
                if ((name_valid == true) && (name_volt_valid == true))
                    power_source.def_by_curr_volt = true;

                name_valid = name_curr_valid & name_volt_valid;
            }

            if (name_valid == false)
            {
                LogMessage(LOG_FAILURE, "Power source name not valid for source: " + std::to_string(idx) + ". Expected \"name\" or \"name_current\" + \"name_voltage\" defined");
                return RET_FAILURE;
            }

            // Get log_name
            power_source.source_name = "Power[" + std::to_string(m_xbtest_pfm_def.physical.power.num_power_sources) + "]";
            power_source.source_name_current = "Current[" + std::to_string(m_xbtest_pfm_def.physical.power.num_power_sources) + "]";
            power_source.source_name_voltage = "Voltage[" + std::to_string(m_xbtest_pfm_def.physical.power.num_power_sources) + "]";
            node_title = {DEVICE, PHYSICAL, POWER, POWER_SOURCES, std::to_string(idx), SOURCE_NAME};
            if (NodeExists(node_title) == true)
            {
                ret_failure |= ExtractNodeValueStr(node_title, &(power_source.source_name));
                if (ret_failure == RET_FAILURE)
                    return RET_FAILURE;
                power_source.source_name_current = power_source.source_name + " Current";
                power_source.source_name_voltage = power_source.source_name + " Voltage";
                power_source.source_name += " Power";
            }

            // Get powertest
            power_source.powertest = DEFAULT_POWERTEST;
            node_title = {DEVICE, PHYSICAL, POWER, POWER_SOURCES, std::to_string(idx), POWERTEST};
            if (NodeExists(node_title))
            {
                ret_failure |= ExtractNodeValueBool(node_title, &(power_source.powertest));
                if (ret_failure == RET_FAILURE)
                    return RET_FAILURE;
            }

            // Get limit
            power_source.limit = DEFAULT_LIMIT;
            node_title = {DEVICE, PHYSICAL, POWER, POWER_SOURCES, std::to_string(idx), LIMIT};
            if (NodeExists(node_title))
            {
                ret_failure |= ExtractNodeValueInt<int>(node_title, &(power_source.limit));
                if (ret_failure == RET_FAILURE)
                    return RET_FAILURE;
            }
            else
            {
                if (power_source.powertest == true)
                {
                    PrintRequiredNotFound(node_title);
                    return RET_FAILURE;
                }
            }

            // Get calibration max power, this node is not mandatory
            power_source.calibration = DEFAULT_CALIBRATION;
            node_title = {DEVICE, PHYSICAL, POWER, POWER_SOURCES, std::to_string(idx), CALIBRATION};
            if (NodeExists(node_title))
            {
                if (power_source.powertest == true)
                {
                    ret_failure |= ExtractNodeValueInt<int>(node_title, &(power_source.calibration));
                    if (ret_failure == RET_FAILURE)
                        return RET_FAILURE;

                    if (power_source.calibration > power_source.limit)
                    {
                        LogMessage(LOG_FAILURE, "Calibration power bigger than the power limit");
                        return RET_FAILURE;
                    }

                }
                else
                {
                    LogMessage(LOG_FAILURE, "Calibration defined but power source is not enabled for powertest");
                    return RET_FAILURE;
                }
                if (power_source.calibration >= 0)
                    m_xbtest_pfm_def.physical.power.max_calibration +=  power_source.calibration;

            }

            m_xbtest_pfm_def.physical.power.power_sources.push_back(power_source);
            m_xbtest_pfm_def.physical.power.num_power_sources++;
        }
    }

    if (m_xbtest_pfm_def.physical.power.max_calibration == 0)
    {
        LogMessage(LOG_FAILURE, "No Calibration power defined in any power sources");
        return RET_FAILURE;
    }

    // saturate the calibration power to power target max
    if  (m_xbtest_pfm_def.physical.power.max_calibration > m_xbtest_pfm_def.physical.power.power_target.max)
        m_xbtest_pfm_def.physical.power.max_calibration = m_xbtest_pfm_def.physical.power.power_target.max;

    ///////////////////////////
    // Memory
    //////////////////////////
    m_xbtest_pfm_def.memory.hbm_exists = false;
    m_xbtest_pfm_def.memory.ddr_exists = false;
    for (auto mem_type : MEM_TYPES)
    {
        node_title = {DEVICE, MEMORY, mem_type};
        if (NodeExists(node_title))
        {
            if (StrMatchNoCase(mem_type, "HBM") == true)
                m_xbtest_pfm_def.memory.hbm_exists = true;
            if (StrMatchNoCase(mem_type, "DDR") == true)
                m_xbtest_pfm_def.memory.ddr_exists = true;

            Xbtest_Pfm_Def_MemType_t mem;

            // Get size
            node_title = {DEVICE, MEMORY, mem_type, SIZE};
            ret_failure |= ExtractNodeValueInt<uint>(node_title, &(mem.size));
            if (ret_failure == RET_FAILURE)
            {
                PrintRequiredNotFound(node_title);
                return RET_FAILURE;
            }
            // Get quantity
            node_title = {DEVICE, MEMORY, mem_type, QUANTITY};
            ret_failure |= ExtractNodeValueInt<uint>(node_title, &(mem.quantity));
            if (ret_failure == RET_FAILURE)
            {
                PrintRequiredNotFound(node_title);
                return RET_FAILURE;
            }
            //////////////////////////
            // dma_bw
            //////////////////////////
            // Get BW threshold
            node_title = {DEVICE, MEMORY, mem_type, DMA_BW, WRITE, HIGH};
            ret_failure |= ExtractNodeValueInt<uint>(node_title, &(mem.dma_bw.write.high));
            if (ret_failure == RET_FAILURE)
            {
                PrintRequiredNotFound(node_title);
                return RET_FAILURE;
            }
            // Get BW threshold
            node_title = {DEVICE, MEMORY, mem_type, DMA_BW, WRITE, LOW};
            ret_failure |= ExtractNodeValueInt<uint>(node_title, &(mem.dma_bw.write.low));
            if (ret_failure == RET_FAILURE)
            {
                PrintRequiredNotFound(node_title);
                return RET_FAILURE;
            }
            // Get BW threshold
            node_title = {DEVICE, MEMORY, mem_type, DMA_BW, READ, HIGH};
            ret_failure |= ExtractNodeValueInt<uint>(node_title, &(mem.dma_bw.read.high));
            if (ret_failure == RET_FAILURE)
            {
                PrintRequiredNotFound(node_title);
                return RET_FAILURE;
            }
            // Get BW threshold
            node_title = {DEVICE, MEMORY, mem_type, DMA_BW, READ, LOW};
            ret_failure |= ExtractNodeValueInt<uint>(node_title, &(mem.dma_bw.read.low));
            if (ret_failure == RET_FAILURE)
            {
                PrintRequiredNotFound(node_title);
                return RET_FAILURE;
            }
            //////////////////////////
            // cu_bw
            //////////////////////////
            // Get BW threshold
            node_title = {DEVICE, MEMORY, mem_type, CU_BW, ALT_WR_RD, WRITE, HIGH};
            ret_failure |= ExtractNodeValueInt<uint>(node_title, &(mem.cu_bw.alt_wr_rd.write.high));
            if (ret_failure == RET_FAILURE)
            {
                PrintRequiredNotFound(node_title);
                return RET_FAILURE;
            }
            // Get BW threshold
            node_title = {DEVICE, MEMORY, mem_type, CU_BW, ALT_WR_RD, WRITE, LOW};
            ret_failure |= ExtractNodeValueInt<uint>(node_title, &(mem.cu_bw.alt_wr_rd.write.low));
            if (ret_failure == RET_FAILURE)
            {
                PrintRequiredNotFound(node_title);
                return RET_FAILURE;
            }
            // Get BW threshold
            node_title = {DEVICE, MEMORY, mem_type, CU_BW, ALT_WR_RD, READ, HIGH};
            ret_failure |= ExtractNodeValueInt<uint>(node_title, &(mem.cu_bw.alt_wr_rd.read.high));
            if (ret_failure == RET_FAILURE)
            {
                PrintRequiredNotFound(node_title);
                return RET_FAILURE;
            }
            // Get BW threshold
            node_title = {DEVICE, MEMORY, mem_type, CU_BW, ALT_WR_RD, READ, LOW};
            ret_failure |= ExtractNodeValueInt<uint>(node_title, &(mem.cu_bw.alt_wr_rd.read.low));
            if (ret_failure == RET_FAILURE)
            {
                PrintRequiredNotFound(node_title);
                return RET_FAILURE;
            }
            // Get BW threshold
            node_title = {DEVICE, MEMORY, mem_type, CU_BW, ONLY_WR, WRITE, HIGH};
            ret_failure |= ExtractNodeValueInt<uint>(node_title, &(mem.cu_bw.only_wr.write.high));
            if (ret_failure == RET_FAILURE)
            {
                PrintRequiredNotFound(node_title);
                return RET_FAILURE;
            }
            // Get BW threshold
            node_title = {DEVICE, MEMORY, mem_type, CU_BW, ONLY_WR, WRITE, LOW};
            ret_failure |= ExtractNodeValueInt<uint>(node_title, &(mem.cu_bw.only_wr.write.low));
            if (ret_failure == RET_FAILURE)
            {
                PrintRequiredNotFound(node_title);
                return RET_FAILURE;
            }
            // Get BW threshold
            node_title = {DEVICE, MEMORY, mem_type, CU_BW, ONLY_RD, READ, HIGH};
            ret_failure |= ExtractNodeValueInt<uint>(node_title, &(mem.cu_bw.only_rd.read.high));
            if (ret_failure == RET_FAILURE)
            {
                PrintRequiredNotFound(node_title);
                return RET_FAILURE;
            }
            // Get BW threshold
            node_title = {DEVICE, MEMORY, mem_type, CU_BW, ONLY_RD, READ, LOW};
            ret_failure |= ExtractNodeValueInt<uint>(node_title, &(mem.cu_bw.only_rd.read.low));
            if (ret_failure == RET_FAILURE)
            {
                PrintRequiredNotFound(node_title);
                return RET_FAILURE;
            }

            // Append parameters
            if (StrMatchNoCase(mem_type, "HBM") == true)
                m_xbtest_pfm_def.memory.hbm = mem;
            if (StrMatchNoCase(mem_type, "DDR") == true)
                m_xbtest_pfm_def.memory.ddr = mem;
        }
    }
    if ((m_xbtest_pfm_def.memory.hbm_exists == false) && (m_xbtest_pfm_def.memory.ddr_exists == false))
    {
        LogMessage(LOG_FAILURE, "At least one memory must be defined");
        return RET_FAILURE;
    }

    PrintPlatformDef();
    return RET_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void XbtestPfmDefParser::PrintPlatformDef()
{
    LogMessage(LOG_INFO, "Version: " + m_version);
    LogMessage(LOG_INFO, "Device: " + m_xbtest_pfm_def.info.name);

    for (uint i=0; i<m_xbtest_pfm_def.info.num_clocks; i++)
    {
        LogMessage(LOG_INFO, "Clock " + std::to_string(i) + ":");
        LogMessage(LOG_INFO, "\t - Name: \""      + StrVectToStr(m_xbtest_pfm_def.info.clocks[i].name, ".") + "\"");
        LogMessage(LOG_INFO, "\t - Frequency: " + std::to_string(m_xbtest_pfm_def.info.clocks[i].frequency) + " MHz");
    }

    if (m_xbtest_pfm_def.runtime.download_time > -1)
        LogMessage(LOG_INFO, "Download time: " + std::to_string(m_xbtest_pfm_def.runtime.download_time) + " us");
    else
        LogMessage(LOG_INFO, "Download time: not checked");

    LogMessage(LOG_INFO, "Thermal calibration: Power = a + b * e^(c*temperature)");
    LogMessage(LOG_INFO, "\t - a: " + Float_to_String(m_xbtest_pfm_def.physical.thermal.calibration.a,10));
    LogMessage(LOG_INFO, "\t - b: " + Float_to_String(m_xbtest_pfm_def.physical.thermal.calibration.b,10));
    LogMessage(LOG_INFO, "\t - c: " + Float_to_String(m_xbtest_pfm_def.physical.thermal.calibration.c,10));

    LogMessage(LOG_INFO, "XPE leakage: Power = a + b * e^(c*temperature)");
    LogMessage(LOG_INFO, "\t - a: " + Float_to_String(m_xbtest_pfm_def.physical.thermal.xpe_leakage.a,10));
    LogMessage(LOG_INFO, "\t - b: " + Float_to_String(m_xbtest_pfm_def.physical.thermal.xpe_leakage.b,10));
    LogMessage(LOG_INFO, "\t - c: " + Float_to_String(m_xbtest_pfm_def.physical.thermal.xpe_leakage.c,10));

    for (uint i=0; i<m_xbtest_pfm_def.physical.thermal.num_temp_sources; i++)
    {
        LogMessage(LOG_INFO, "Temperature source " + std::to_string(i) + " configuration:");
        LogMessage(LOG_INFO, "\t - Name: \"" + StrVectToStr(m_xbtest_pfm_def.physical.thermal.temp_sources[i].name, ".") + "\"");
        LogMessage(LOG_INFO, "\t - Source name: \"" + m_xbtest_pfm_def.physical.thermal.temp_sources[i].source_name);
        if (m_xbtest_pfm_def.physical.thermal.temp_sources[i].limit > -1)
            LogMessage(LOG_INFO, "\t - Limit: " + std::to_string(m_xbtest_pfm_def.physical.thermal.temp_sources[i].limit) + " deg C");
        else
            LogMessage(LOG_INFO, "\t - Limit: not checked");
    }

    LogMessage(LOG_INFO, "Power target limits: ");
    LogMessage(LOG_INFO, "\t - Minimum: " + std::to_string(m_xbtest_pfm_def.physical.power.power_target.min));
    LogMessage(LOG_INFO, "\t - Maximum: " + std::to_string(m_xbtest_pfm_def.physical.power.power_target.max));

    for (uint i=0; i<m_xbtest_pfm_def.physical.power.num_power_sources; i++)
    {
        LogMessage(LOG_INFO, "Power source " + std::to_string(i) + " configuration:");
        if (m_xbtest_pfm_def.physical.power.power_sources[i].def_by_curr_volt == false)
        {
            LogMessage(LOG_INFO, "\t - Name: \"" + StrVectToStr(m_xbtest_pfm_def.physical.power.power_sources[i].name, ".") + "\"");
        }
        else
        {
            LogMessage(LOG_INFO, "\t - Current: \"" + StrVectToStr(m_xbtest_pfm_def.physical.power.power_sources[i].name_current, ".") + "\"");
            LogMessage(LOG_INFO, "\t - Voltage: \"" + StrVectToStr(m_xbtest_pfm_def.physical.power.power_sources[i].name_voltage, ".") + "\"");
        }
        LogMessage(LOG_INFO, "\t - Source name power: " + m_xbtest_pfm_def.physical.power.power_sources[i].source_name);
        if (m_xbtest_pfm_def.physical.power.power_sources[i].def_by_curr_volt == true)
        {
            LogMessage(LOG_INFO, "\t - Source name current: " + m_xbtest_pfm_def.physical.power.power_sources[i].source_name_current);
            LogMessage(LOG_INFO, "\t - Source name voltage: " + m_xbtest_pfm_def.physical.power.power_sources[i].source_name_voltage);
        }
        if (m_xbtest_pfm_def.physical.power.power_sources[i].limit > -1)
            LogMessage(LOG_INFO, "\t - Limit: " + std::to_string(m_xbtest_pfm_def.physical.power.power_sources[i].limit) + " W");
        else
            LogMessage(LOG_INFO, "\t - Limit: not checked");
        LogMessage(LOG_INFO, "\t - Powertest: " + BoolToStr(m_xbtest_pfm_def.physical.power.power_sources[i].powertest));
        if (m_xbtest_pfm_def.physical.power.power_sources[i].powertest == true)
            LogMessage(LOG_INFO, "\t - Calibration: " + std::to_string(m_xbtest_pfm_def.physical.power.power_sources[i].calibration) + " W");
    }
    LogMessage(LOG_INFO, "Maximum power calibration: " + std::to_string(m_xbtest_pfm_def.physical.power.max_calibration) + " W");

    for (auto mem_type : MEM_TYPES)
    {
        bool mem_exists = false;
        Xbtest_Pfm_Def_MemType_t mem;
        if (StrMatchNoCase(mem_type, "HBM"))
        {
            mem_exists = m_xbtest_pfm_def.memory.hbm_exists;
            mem = m_xbtest_pfm_def.memory.hbm;
        }
        else if (StrMatchNoCase(mem_type, "DDR"))
        {
            mem_exists = m_xbtest_pfm_def.memory.ddr_exists;
            mem = m_xbtest_pfm_def.memory.ddr;
        }
        if (mem_exists == true)
        {
            LogMessage(LOG_INFO, mem_type + " configuration:");
            LogMessage(LOG_INFO, "\t - Size: " + std::to_string(mem.size) + " MB");
            LogMessage(LOG_INFO, "\t - Quantity: " + std::to_string(mem.quantity));
            LogMessage(LOG_INFO, "\t - DMA BW thresholds:");
            LogMessage(LOG_INFO, "\t\t - Write: ");
            LogMessage(LOG_INFO, "\t\t\t - High: " + std::to_string(mem.dma_bw.write.high)    + " MBps");
            LogMessage(LOG_INFO, "\t\t\t - Low:  " + std::to_string(mem.dma_bw.write.low)     + " MBps");
            LogMessage(LOG_INFO, "\t\t - Read: ");
            LogMessage(LOG_INFO, "\t\t\t - High: " + std::to_string(mem.dma_bw.read.high)     + " MBps");
            LogMessage(LOG_INFO, "\t\t\t - Low:  " + std::to_string(mem.dma_bw.read.low)      + " MBps");

            LogMessage(LOG_INFO, "\t - Compute unit BW thresholds:");

            LogMessage(LOG_INFO, "\t\t - Test mode \"alt_wr_rd\":");
            LogMessage(LOG_INFO, "\t\t\t - Write: ");
            LogMessage(LOG_INFO, "\t\t\t\t - High: " + std::to_string(mem.cu_bw.alt_wr_rd.write.high) + " MBps");
            LogMessage(LOG_INFO, "\t\t\t\t - low:  " + std::to_string(mem.cu_bw.alt_wr_rd.write.low)  + " MBps");
            LogMessage(LOG_INFO, "\t\t\t - Read: ");
            LogMessage(LOG_INFO, "\t\t\t\t - High: " + std::to_string(mem.cu_bw.alt_wr_rd.read.high)  + " MBps");
            LogMessage(LOG_INFO, "\t\t\t\t - low:  " + std::to_string(mem.cu_bw.alt_wr_rd.read.low)   + " MBps");

            LogMessage(LOG_INFO, "\t\t - Test mode \"only_wr\":");
            LogMessage(LOG_INFO, "\t\t\t - Write: ");
            LogMessage(LOG_INFO, "\t\t\t\t - High: " + std::to_string(mem.cu_bw.only_wr.write.high)   + " MBps");
            LogMessage(LOG_INFO, "\t\t\t\t - Low:  " + std::to_string(mem.cu_bw.only_wr.write.low)    + " MBps");

            LogMessage(LOG_INFO, "\t\t - Test mode \"only_rd\":");
            LogMessage(LOG_INFO, "\t\t\t - Read: ");
            LogMessage(LOG_INFO, "\t\t\t\t - High: " + std::to_string(mem.cu_bw.only_rd.read.high)    + " MBps");
            LogMessage(LOG_INFO, "\t\t\t\t - Low:  " + std::to_string(mem.cu_bw.only_rd.read.low)     + " MBps");
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void XbtestPfmDefParser::PrintRequiredNotFound( std::vector<std::string> node_title_in )
{
    LogMessage(LOG_FAILURE, "Required parameter not found in Platform definition: " + StrVectToStr(node_title_in, "."));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Json_Definition_t XbtestPfmDefParser::GetJsonDefinition()
{
    Json_Definition_t json_definition;
    json_definition.insert( Definition_t({VERSION}, JSON_NODE_VALUE));
    json_definition.insert( Definition_t({DEVICE},  JSON_NODE_OBJECT));

    json_definition.insert( Definition_t({DEVICE, INFO},        JSON_NODE_OBJECT));
    json_definition.insert( Definition_t({DEVICE, INFO, NAME},  JSON_NODE_VALUE));

    json_definition.insert( Definition_t({DEVICE, INFO, CLOCKS}, JSON_NODE_OBJECT));
    for (uint idx = 0; idx < MAX_CLOCKS; idx++)
    {
        json_definition.insert( Definition_t({DEVICE, INFO, CLOCKS, std::to_string(idx)},               JSON_NODE_OBJECT));
        json_definition.insert( Definition_t({DEVICE, INFO, CLOCKS, std::to_string(idx), NAME},         JSON_NODE_ARRAY));
        json_definition.insert( Definition_t({DEVICE, INFO, CLOCKS, std::to_string(idx), FREQUENCY},    JSON_NODE_VALUE));
    }

    json_definition.insert( Definition_t({DEVICE, RUNTIME},                 JSON_NODE_OBJECT));
    json_definition.insert( Definition_t({DEVICE, RUNTIME, DOWNLOAD_TIME},  JSON_NODE_VALUE));

    json_definition.insert( Definition_t({DEVICE, PHYSICAL}, JSON_NODE_OBJECT));

    json_definition.insert( Definition_t({DEVICE, PHYSICAL, THERMAL},                  JSON_NODE_OBJECT));
    json_definition.insert( Definition_t({DEVICE, PHYSICAL, THERMAL, CALIBRATION},     JSON_NODE_OBJECT));
    json_definition.insert( Definition_t({DEVICE, PHYSICAL, THERMAL, CALIBRATION, A},  JSON_NODE_VALUE));
    json_definition.insert( Definition_t({DEVICE, PHYSICAL, THERMAL, CALIBRATION, B},  JSON_NODE_VALUE));
    json_definition.insert( Definition_t({DEVICE, PHYSICAL, THERMAL, CALIBRATION, C},  JSON_NODE_VALUE));

    json_definition.insert( Definition_t({DEVICE, PHYSICAL, THERMAL, XPE_LEAKAGE},     JSON_NODE_OBJECT));
    json_definition.insert( Definition_t({DEVICE, PHYSICAL, THERMAL, XPE_LEAKAGE, A},  JSON_NODE_VALUE));
    json_definition.insert( Definition_t({DEVICE, PHYSICAL, THERMAL, XPE_LEAKAGE, B},  JSON_NODE_VALUE));
    json_definition.insert( Definition_t({DEVICE, PHYSICAL, THERMAL, XPE_LEAKAGE, C},  JSON_NODE_VALUE));

    json_definition.insert( Definition_t({DEVICE, PHYSICAL, THERMAL, TEMP_SOURCES}, JSON_NODE_OBJECT));
    for (uint idx = 0; idx < MAX_TEMP_SOURCES; idx++)
    {
        json_definition.insert( Definition_t({DEVICE, PHYSICAL, THERMAL, TEMP_SOURCES, std::to_string(idx)},            JSON_NODE_OBJECT));
        json_definition.insert( Definition_t({DEVICE, PHYSICAL, THERMAL, TEMP_SOURCES, std::to_string(idx), NAME},      JSON_NODE_ARRAY));
        json_definition.insert( Definition_t({DEVICE, PHYSICAL, THERMAL, TEMP_SOURCES, std::to_string(idx), SOURCE_NAME},  JSON_NODE_VALUE));
        json_definition.insert( Definition_t({DEVICE, PHYSICAL, THERMAL, TEMP_SOURCES, std::to_string(idx), LIMIT},     JSON_NODE_VALUE));
    }

    json_definition.insert( Definition_t({DEVICE, PHYSICAL, POWER},                     JSON_NODE_OBJECT));
    json_definition.insert( Definition_t({DEVICE, PHYSICAL, POWER, POWER_TARGET},       JSON_NODE_OBJECT));
    json_definition.insert( Definition_t({DEVICE, PHYSICAL, POWER, POWER_TARGET, MIN},  JSON_NODE_VALUE));
    json_definition.insert( Definition_t({DEVICE, PHYSICAL, POWER, POWER_TARGET, MAX},  JSON_NODE_VALUE));
    json_definition.insert( Definition_t({DEVICE, PHYSICAL, POWER, POWER_SOURCES},      JSON_NODE_OBJECT));
    for (uint idx = 0; idx < MAX_POWER_SOURCES; idx++)
    {
        json_definition.insert( Definition_t({DEVICE, PHYSICAL, POWER, POWER_SOURCES, std::to_string(idx)},                     JSON_NODE_OBJECT));
        json_definition.insert( Definition_t({DEVICE, PHYSICAL, POWER, POWER_SOURCES, std::to_string(idx), NAME},               JSON_NODE_ARRAY));
        json_definition.insert( Definition_t({DEVICE, PHYSICAL, POWER, POWER_SOURCES, std::to_string(idx), NAME_CURRENT},       JSON_NODE_ARRAY));
        json_definition.insert( Definition_t({DEVICE, PHYSICAL, POWER, POWER_SOURCES, std::to_string(idx), NAME_VOLTAGE},       JSON_NODE_ARRAY));
        json_definition.insert( Definition_t({DEVICE, PHYSICAL, POWER, POWER_SOURCES, std::to_string(idx), SOURCE_NAME},           JSON_NODE_VALUE));
        json_definition.insert( Definition_t({DEVICE, PHYSICAL, POWER, POWER_SOURCES, std::to_string(idx), LIMIT},              JSON_NODE_VALUE));
        json_definition.insert( Definition_t({DEVICE, PHYSICAL, POWER, POWER_SOURCES, std::to_string(idx), POWERTEST},          JSON_NODE_VALUE));
        json_definition.insert( Definition_t({DEVICE, PHYSICAL, POWER, POWER_SOURCES, std::to_string(idx), CALIBRATION},        JSON_NODE_VALUE));
    }

    json_definition.insert( Definition_t({DEVICE, MEMORY}, JSON_NODE_OBJECT));
    for (auto mem_type : MEM_TYPES)
    {
        json_definition.insert( Definition_t({DEVICE, MEMORY, mem_type},                JSON_NODE_OBJECT));
        json_definition.insert( Definition_t({DEVICE, MEMORY, mem_type, SIZE},          JSON_NODE_VALUE));
        json_definition.insert( Definition_t({DEVICE, MEMORY, mem_type, QUANTITY},      JSON_NODE_VALUE));

        json_definition.insert( Definition_t({DEVICE, MEMORY, mem_type, DMA_BW},                JSON_NODE_OBJECT));
        json_definition.insert( Definition_t({DEVICE, MEMORY, mem_type, DMA_BW, WRITE},         JSON_NODE_OBJECT));
        json_definition.insert( Definition_t({DEVICE, MEMORY, mem_type, DMA_BW, WRITE, HIGH},   JSON_NODE_VALUE));
        json_definition.insert( Definition_t({DEVICE, MEMORY, mem_type, DMA_BW, WRITE, LOW},    JSON_NODE_VALUE));
        json_definition.insert( Definition_t({DEVICE, MEMORY, mem_type, DMA_BW, READ},          JSON_NODE_OBJECT));
        json_definition.insert( Definition_t({DEVICE, MEMORY, mem_type, DMA_BW, READ, HIGH},    JSON_NODE_VALUE));
        json_definition.insert( Definition_t({DEVICE, MEMORY, mem_type, DMA_BW, READ, LOW},     JSON_NODE_VALUE));

        json_definition.insert( Definition_t({DEVICE, MEMORY, mem_type, CU_BW},                         JSON_NODE_OBJECT));
        json_definition.insert( Definition_t({DEVICE, MEMORY, mem_type, CU_BW, ALT_WR_RD},              JSON_NODE_OBJECT));
        json_definition.insert( Definition_t({DEVICE, MEMORY, mem_type, CU_BW, ALT_WR_RD, WRITE},       JSON_NODE_OBJECT));
        json_definition.insert( Definition_t({DEVICE, MEMORY, mem_type, CU_BW, ALT_WR_RD, WRITE, HIGH}, JSON_NODE_VALUE));
        json_definition.insert( Definition_t({DEVICE, MEMORY, mem_type, CU_BW, ALT_WR_RD, WRITE, LOW},  JSON_NODE_VALUE));
        json_definition.insert( Definition_t({DEVICE, MEMORY, mem_type, CU_BW, ALT_WR_RD, READ},        JSON_NODE_OBJECT));
        json_definition.insert( Definition_t({DEVICE, MEMORY, mem_type, CU_BW, ALT_WR_RD, READ, HIGH},  JSON_NODE_VALUE));
        json_definition.insert( Definition_t({DEVICE, MEMORY, mem_type, CU_BW, ALT_WR_RD, READ, LOW},   JSON_NODE_VALUE));

        json_definition.insert( Definition_t({DEVICE, MEMORY, mem_type, CU_BW, ONLY_WR},                JSON_NODE_OBJECT));
        json_definition.insert( Definition_t({DEVICE, MEMORY, mem_type, CU_BW, ONLY_WR, WRITE},         JSON_NODE_OBJECT));
        json_definition.insert( Definition_t({DEVICE, MEMORY, mem_type, CU_BW, ONLY_WR, WRITE, HIGH},   JSON_NODE_VALUE));
        json_definition.insert( Definition_t({DEVICE, MEMORY, mem_type, CU_BW, ONLY_WR, WRITE, LOW},    JSON_NODE_VALUE));

        json_definition.insert( Definition_t({DEVICE, MEMORY, mem_type, CU_BW, ONLY_RD},                JSON_NODE_OBJECT));
        json_definition.insert( Definition_t({DEVICE, MEMORY, mem_type, CU_BW, ONLY_RD, READ},          JSON_NODE_OBJECT));
        json_definition.insert( Definition_t({DEVICE, MEMORY, mem_type, CU_BW, ONLY_RD, READ, HIGH},    JSON_NODE_VALUE));
        json_definition.insert( Definition_t({DEVICE, MEMORY, mem_type, CU_BW, ONLY_RD, READ, LOW},     JSON_NODE_VALUE));
    }
    return json_definition;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Xbtest_Pfm_Def_t XbtestPfmDefParser::GetPlatformDef()
{
    return m_xbtest_pfm_def;
}
