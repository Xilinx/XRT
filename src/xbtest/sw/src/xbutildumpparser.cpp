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

#include "xbutildumpparser.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

XbutilDumpParser::XbutilDumpParser( std::string device_index, Global_Config_t global_config, std::atomic<bool> *abort )
{
    m_log = Logging::getInstance(); // handle to the logging
    m_log_msg_test_type = "XBUTIL DUMP: ";
    m_global_config = global_config;
    m_device_index = device_index;
    m_abort = abort;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

XbutilDumpParser::~XbutilDumpParser()
{
    ClearParser();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool XbutilDumpParser::Parse()
{
    // Execute xbutil dump
    std::string sys_cmd = "xbutil dump";
    if (m_device_index != "NONE") sys_cmd += " -d " + m_device_index;

    FILE* m_pipe = popen(sys_cmd.c_str(), "r");

    if (m_pipe == 0)
    {
        LogMessage(LOG_FAILURE, "Failed to execute command: " + sys_cmd);
        return true;
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
        return true;
    }

    if (dump_output.size() == 0)
    {
        LogMessage(LOG_FAILURE, "Empty output for command: " + sys_cmd);
        return true;
    }
    pclose(m_pipe);

    // Initialize xbutil dump output parser and reader
    m_json_parser = json_parser_new(); // parse the json

    GError *error = NULL;
    json_parser_load_from_data(m_json_parser, dump_output.c_str(), dump_output.size(), &error);
    if (error)
    {
        LogMessage(LOG_FAILURE, "Unable to parse xbutil dump output: " + std::string(error->message));
        LogMessage(LOG_INFO, "Reporting xbutil dump output below:\n" + dump_output);
        g_error_free (error);
        g_object_unref (m_json_parser);
        return true;
    }

    m_json_root_node = json_parser_get_root(m_json_parser);
    m_json_reader = json_reader_new(NULL);
    json_reader_set_root (m_json_reader, m_json_root_node);

    return false;
}




