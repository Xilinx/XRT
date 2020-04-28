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

#include "xclbinutilparser.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

XclbinUtilParser::XclbinUtilParser( std::string device_name, uint device_idx, std::string xclbin, std::string xclbin_uuid, Global_Config_t global_config, std::atomic<bool> *abort )
{
    m_log = Logging::getInstance(); // handle to the logging
    m_log_msg_test_type = "XCLBIN UTIL: ";
    m_global_config = global_config;
    m_device_name = device_name;
    m_device_idx = device_idx;
    m_xclbin = xclbin;
    m_xclbin_uuid = xclbin_uuid;
    m_abort = abort;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

XclbinUtilParser::~XclbinUtilParser()
{
    ClearParser();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool XclbinUtilParser::Parse() {return RET_SUCCESS;}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool XclbinUtilParser::ParseConnectivity()
{
    bool ret_failure = RET_SUCCESS;
    LogMessage(LOG_INFO, "Get xclbin connectivity");
    // Execute xclbinutil
    std::string connectivity_file_name = "connectivity_" + m_device_name + "_" + std::to_string(m_device_idx) + "_" + m_xclbin_uuid + ".json";
    if (m_global_config.use_logging == true)
        connectivity_file_name = m_global_config.logging + "/" + connectivity_file_name;

    std::string sys_cmd = "xclbinutil -i " + m_xclbin + " --force --dump-section CONNECTIVITY:JSON:\"" + connectivity_file_name + "\"";

    FILE* m_pipe = popen(sys_cmd.c_str(), "r");
    if (m_pipe == 0)
    {
        LogMessage(LOG_FAILURE, "Failed to execute command: " + sys_cmd);
        return RET_FAILURE;
    }

    // Get xbutil dump output
    char buffer[256];
    std::string dump_output = "";
    try {
        while (!feof(m_pipe))
        {
            if (fgets(buffer, sizeof(buffer), m_pipe) != NULL)
                dump_output.append(buffer);
        }
    } catch (...) {
        pclose(m_pipe);
        LogMessage(LOG_FAILURE, "Failed to execute command: " + sys_cmd);
        return RET_FAILURE;
    }

    if (dump_output.size() == 0)
    {
        LogMessage(LOG_FAILURE, "Empty output for command: " + sys_cmd);
        return RET_FAILURE;
    }
    pclose(m_pipe);

    /////////////////////////////////////////////////////////////////////////////////
    // Parse JSON file
    /////////////////////////////////////////////////////////////////////////////////
    // check json configuration file exists
    std::ifstream infile(connectivity_file_name);
    if (infile.good() == false)
    {
        LogMessage(LOG_FAILURE, "Connectivity JSON does not exist: " + connectivity_file_name);
        return RET_FAILURE;
    }

    // Initialize json parser and reader
    LogMessage(LOG_INFO, "Using Connectivity JSON: " + connectivity_file_name);
    m_json_parser = json_parser_new();

    GError *error = NULL;
    json_parser_load_from_file (m_json_parser, connectivity_file_name.c_str(), &error);
    if (error)
    {
        LogMessage(LOG_FAILURE, "Unable to parse Connectivity JSON: " + std::string(error->message));
        g_error_free(error);
        g_object_unref(m_json_parser);
        return RET_FAILURE;
    }

    m_json_root_node = json_parser_get_root(m_json_parser);
    m_json_reader = json_reader_new(NULL);
    json_reader_set_root(m_json_reader, m_json_root_node);

    /////////////////////////////////////////////////////////////////////////////////
    // Get connectivity parameters
    /////////////////////////////////////////////////////////////////////////////////
    std::vector<std::string> node_title = {CONNECTIVITY};
    if (NodeExists(node_title) == false)
    {
        PrintRequiredNotFound(node_title);
        return RET_FAILURE;
    }
    node_title = {CONNECTIVITY, M_COUNT};
    std::string full_node_title_count = StrVectToStr(node_title, ".");
    std::string tmp_str;
    ret_failure |= ExtractNodeValueStr(node_title, &tmp_str);
    if (ret_failure == RET_FAILURE)
    {
        PrintRequiredNotFound(node_title);
        return RET_FAILURE;
    }
    ret_failure |= ConvString2Num<uint>(tmp_str, &(m_connectivity.m_count));
    if (ret_failure == RET_FAILURE)
    {
        LogMessage(LOG_FAILURE, "Failed to convert value: " + StrVectToStr(node_title, "."));
        return RET_FAILURE;
    }

    std::vector<std::string> connection_title = {CONNECTIVITY, M_CONNECTION};
    std::string full_node_title_connection = StrVectToStr(connection_title, ".");
    if (NodeExists(connection_title) == false)
    {
        PrintRequiredNotFound(connection_title);
        return RET_FAILURE;
    }

    ExtractNode(connection_title); // Move cursor to testcases array

    uint elements_count = (uint)json_reader_count_elements(m_json_reader);
    if (m_connectivity.m_count != elements_count)
    {
        LogMessage(LOG_FAILURE, full_node_title_count + ": " + std::to_string(m_connectivity.m_count) + " does not match size of array " + full_node_title_connection + ": " + std::to_string(elements_count));
        return RET_FAILURE;
    }

    m_connectivity.m_connection.clear();

    for (uint j = 0; j < m_connectivity.m_count; j++) // For each element in connection array
    {
        json_reader_read_element(m_json_reader, j); // Move cursor to connection element

        Connection_t connection;
        // arg_index
        node_title = {ARG_INDEX};
        std::string full_node_count = StrVectToStr(node_title, ".");
        ret_failure |= ExtractNodeValueStr(node_title, &tmp_str);
        if (ret_failure == RET_FAILURE)
        {
            PrintRequiredNotFound(node_title);
            return RET_FAILURE;
        }
        ret_failure |= ConvString2Num<uint>(tmp_str, &(connection.arg_index));
        if (ret_failure == RET_FAILURE)
        {
            LogMessage(LOG_FAILURE, "Failed to convert value: " + StrVectToStr(node_title, "."));
            return RET_FAILURE;
        }
        // m_ip_layout_index
        node_title = {M_IP_LAYOUT_INDEX};
        full_node_title_count = StrVectToStr(node_title, ".");
        ret_failure |= ExtractNodeValueStr(node_title, &tmp_str);
        if (ret_failure == RET_FAILURE)
        {
            PrintRequiredNotFound(node_title);
            return RET_FAILURE;
        }
        ret_failure |= ConvString2Num<uint>(tmp_str, &(connection.m_ip_layout_index));
        if (ret_failure == RET_FAILURE)
        {
            LogMessage(LOG_FAILURE, "Failed to convert value: " + StrVectToStr(node_title, "."));
            return RET_FAILURE;
        }
        // mem_data_index
        node_title = {MEM_DATA_INDEX};
        full_node_title_count = StrVectToStr(node_title, ".");
        ret_failure |= ExtractNodeValueStr(node_title, &tmp_str);
        if (ret_failure == RET_FAILURE)
        {
            PrintRequiredNotFound(node_title);
            return RET_FAILURE;
        }
        ret_failure |= ConvString2Num<uint>(tmp_str, &(connection.mem_data_index));
        if (ret_failure == RET_FAILURE)
        {
            LogMessage(LOG_FAILURE, "Failed to convert value: " + StrVectToStr(node_title, "."));
            return RET_FAILURE;
        }

        // Add parsed connection to connectivity vector
        m_connectivity.m_connection.push_back(connection);

        json_reader_end_element(m_json_reader); // Move back cursor to connection array
    }
    for (uint ii = 0; ii < connection_title.size(); ii++ ) // Move cursor back from connection array
        json_reader_end_element(m_json_reader);

    PrintConnectivity();
    return RET_SUCCESS;
}

/////////////////////////////////////////////////////////////////////////////////

void XclbinUtilParser::PrintConnectivity()
{
    LogMessage(LOG_DEBUG, "Xclbin connectivity:");
    LogMessage(LOG_DEBUG, "\t - m_count: " + std::to_string(m_connectivity.m_count));
    for (uint j = 0; j < m_connectivity.m_count; j++) // For each element in connection array
    {
        LogMessage(LOG_DEBUG, "\t - m_connection[" + std::to_string(j) + "]:");
        LogMessage(LOG_DEBUG, "\t\t - arg_index: "          + std::to_string(m_connectivity.m_connection[j].arg_index));
        LogMessage(LOG_DEBUG, "\t\t - m_ip_layout_index: "  + std::to_string(m_connectivity.m_connection[j].m_ip_layout_index));
        LogMessage(LOG_DEBUG, "\t\t - mem_data_index: "     + std::to_string(m_connectivity.m_connection[j].mem_data_index));
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void XclbinUtilParser::PrintRequiredNotFound( std::vector<std::string> node_title_in )
{
    LogMessage(LOG_FAILURE, "Required parameter not found in Connectivity JSON: " + StrVectToStr(node_title_in, "."));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Connectivity_t XclbinUtilParser::GetConnectivity() { return m_connectivity; }

