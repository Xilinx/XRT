
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

#include "logging.h"

static std::mutex m_logging_mtx;
static bool s_instance_flag = false;
static Logging* s_log = nullptr;
static uint64_t m_msg_count[7];
static std::string m_first_error;

static uint64_t m_timestamp_curr = 0;
static uint64_t m_timestamp_last = 0;
static std::string m_timestamp_mode = TIMESTAMP_MODE_NONE;
static bool m_timestamp_mode_en = false;
static bool m_timestamp_mode_abs_n_diff = false;
static bool m_timestamp_first = false;


static Global_Config_t m_global_config;
static std::vector<std::string> m_log_rec;
static bool m_log_rec_en;
static std::ofstream m_output_log;

Logging::Logging()
{
    for (uint i=0;i<7;i++) m_msg_count[i] = 0;
    m_first_error = "";
    m_log_rec_en = true;
}

Logging::~Logging() { s_instance_flag = false; }

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Logging* Logging::getInstance()
{
    if (!s_instance_flag)
    {
        s_log = new Logging();
        s_instance_flag = true;
    }
    return s_log;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Logging::LogMessage(LogLevel Level, std::string Message, LogLevel Verbosity)
{
    if (Level >= Verbosity)
    {
        m_logging_mtx.lock(); //protect access
        std::string log_msg = "";

        if (m_timestamp_mode_en == true)
        {
            GetTimestamp(&m_timestamp_curr);
            if (m_timestamp_mode_abs_n_diff == true)
            {
                log_msg += "[" + Float_to_String<double>(((double)(m_timestamp_curr))/1000000.0,6) + "] ";
            }
            else
            {
                if (m_timestamp_first == false)
                    log_msg += "[ " + Float_to_String<double>(((double)(0))/1000000.0,6) + "] ";
                else
                    log_msg += "[+" + Float_to_String<double>(((double)(m_timestamp_curr - m_timestamp_last))/1000000.0,6) + "] ";
                m_timestamp_last = m_timestamp_curr;
                m_timestamp_first = true;
            }
        }
        log_msg += LogLevelToString(Level);
        log_msg += Message;

        std::cout << log_msg << std::endl;

        if (Level >= LOG_WARN) m_msg_count[Level]++;

        if (m_first_error == "")
        {
            if ((Level == LOG_FAILURE) || (Level == LOG_ERROR))
                m_first_error = log_msg;
        }

        if (m_log_rec_en == true)
        {
            m_log_rec.push_back(log_msg);
        }
        else if (m_global_config.use_logging == true)
        {
            m_output_log << log_msg << "\n";
            m_output_log.flush();
        }

        m_logging_mtx.unlock();
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::string Logging::GetFirstError() { return m_first_error; }

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Logging::SetTimestampMode( std::string timestamp_mode )
{
    m_timestamp_mode = timestamp_mode;
    if (StrMatchNoCase(timestamp_mode, TIMESTAMP_MODE_NONE) == false)
    {
        m_timestamp_mode_en = true;
        if (StrMatchNoCase(timestamp_mode, TIMESTAMP_MODE_ABSOLUTE) == true)
        {
            m_timestamp_mode_abs_n_diff = true;
        }
    }
}

bool Logging::SetLoggingMode( std::string head_log, Global_Config_t global_config )
{
    m_global_config = global_config;
    if (m_global_config.use_logging == true)
    {
        LogMessage(LOG_INFO, head_log + "Creating xbtest log directory: " + m_global_config.logging, m_global_config.verbosity);

        // Execute mkdir and get return code using echo
        std::string mkdir_cmd = "mkdir -p \"" + m_global_config.logging + "\"; echo $?";
        FILE* m_pipe = popen(mkdir_cmd.c_str(), "r");
        if (m_pipe == 0)
        {
            LogMessage(LOG_FAILURE, head_log + "Failed to execute command: " + mkdir_cmd, m_global_config.verbosity);
            return true;
        }

        // Get mkdir_cmd output
        char buffer[256];
        std::string mkdir_cmd_output = "";
        try {
            while (!feof(m_pipe))
            {
                if (fgets(buffer, sizeof(buffer), m_pipe) != NULL)
                    mkdir_cmd_output.append(buffer);
            }
        } catch (...) {
            pclose(m_pipe);
            LogMessage(LOG_FAILURE, head_log + "Failed to execute command: " + mkdir_cmd, m_global_config.verbosity);
            return true;
        }

        if (mkdir_cmd_output.size() == 0)
        {
            LogMessage(LOG_FAILURE, head_log + "Empty output for command: " + mkdir_cmd, m_global_config.verbosity);
            return true;
        }
        pclose(m_pipe);

        // Check return code
        if (StrMatchNoCase(mkdir_cmd_output, 0, 1, "0") == false)
        {
            LogMessage(LOG_FAILURE, head_log + "Failed to create xbtest log directory, check permissions. Directory: " + m_global_config.logging, m_global_config.verbosity);
            LogMessage(LOG_INFO, head_log + "Command output: " + mkdir_cmd_output, m_global_config.verbosity);
            return true;
        }

        // create the log file
        std::string output_log_name = m_global_config.logging + "/xbtest.log";
        LogMessage(LOG_INFO, head_log + "Use xbtest log file: " + output_log_name, m_global_config.verbosity);
        std::ifstream iftest(output_log_name);
        if (iftest.good())
            LogMessage(LOG_WARN, head_log + "xbtest log file exist, overwriting: " + output_log_name, m_global_config.verbosity);
        iftest.close();

        // Write previously saved log messages
        m_output_log.open(output_log_name, std::ofstream::out);
        if (m_output_log.fail())
        {
            LogMessage(LOG_FAILURE, head_log + "Failed to create xbtest log file, check permissions. File: " + output_log_name, m_global_config.verbosity);
            return true;
        }
        m_logging_mtx.lock(); // Stop logging while previous message are saved
        for (auto log_msg: m_log_rec)
            m_output_log << log_msg << "\n";
        m_output_log.flush();
    }
    else
    {
        m_logging_mtx.lock();
    }
    m_log_rec_en = false; // This authorize writing to log file if use_logging
    m_logging_mtx.unlock(); // Restart logging
    m_log_rec.clear();
    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Logging::GetReportMsgCount(std::string *msg_cnt_msg)
{
    std::string msg_count_str = "";
    bool ret = false;

    if (m_msg_count[LOG_PASS] == 0)
    {
        LogMessage(LOG_ERROR, "No test passes, check that test actually run", LOG_ERROR);
        ret = true;
    }

    msg_count_str += std::to_string(m_msg_count[LOG_WARN])      + " Warnings, ";
    msg_count_str += std::to_string(m_msg_count[LOG_CRIT_WARN]) + " Critical Warnings, ";
    msg_count_str += std::to_string(m_msg_count[LOG_PASS])      + " Passes, ";
    msg_count_str += std::to_string(m_msg_count[LOG_ERROR])     + " Errors, ";
    msg_count_str += std::to_string(m_msg_count[LOG_FAILURE])   + " Failures encountered";
    *msg_cnt_msg = msg_count_str;

    return ret;
}
