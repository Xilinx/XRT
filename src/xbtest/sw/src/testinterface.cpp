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

#include "testinterface.h"

std::string TestInterface::ResultToString( TestResult Result )
{
    switch (Result)
    {
        case TR_PASSED:     return "PASSED";    break;
        case TR_FAILED:     return "FAILED";    break;
        case TR_ABORTED:    return "ABORTED";   break;
        default:            return "NOTSET";    break;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::string TestInterface::StateToString( TestState State )
{
    switch (State)
    {
        case TS_PRE_SETUP:      return "PRESETUP";      break;
        case TS_RUNNING:        return "RUNNING";       break;
        case TS_POST_TEARDOWN:  return "POSTTEARDOWN";  break;
        case TS_COMPLETE:       return "COMPLETE";      break;
        default:                return "NOTSET";        break;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TestInterface::TestState TestInterface::GetState()      { return m_state; }
TestInterface::TestResult TestInterface::GetResult()    { return m_result; }

void TestInterface::PrintState()   { LogMessage(LOG_INFO, "State: " + StateToString(m_state)); }
void TestInterface::PrintResult()  { LogMessage(LOG_INFO, "Result: " + ResultToString(m_result)); }

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool TestInterface::CheckStringInSet ( std::string value, std::set<std::string> test_set )
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

bool TestInterface::OpenOutputFile( std::string test_outputfile_name_in, std::ofstream *outputfile )
{
    bool test_failure = false;
    std::string test_outputfile_name = test_outputfile_name_in;
    if (m_global_config.use_logging == true)
        test_outputfile_name = m_global_config.logging + "/" + test_outputfile_name;

    LogMessage(LOG_INFO, "Using output file: " + test_outputfile_name);
    std::ifstream iftest(test_outputfile_name);
    if (iftest.good())
    {
        LogMessage(LOG_WARN, "Output file exist, overwriting: " + test_outputfile_name);
    }
    iftest.close();
    outputfile->open(test_outputfile_name, std::ofstream::out);
    if (outputfile->fail())
    {
        LogMessage(LOG_FAILURE, "Failed to create file to store measurements, check permissions. File: " + test_outputfile_name);
        test_failure = true;
    }
    return test_failure;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TestInterface::LogMessage ( LogLevel Level, std::string msg ) { m_log->LogMessage(Level, m_log_msg_test_type + msg, m_global_config.verbosity); }

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool TestInterface::GetJsonParamStr( Json_Val_Def_t json_val_def, std::set<std::string> supported_set, std::string *param, std::string param_default )
{
    Json_Parameters_t::iterator it = FindJsonParam(&(m_test_parameters.param), json_val_def);
    if (it != m_test_parameters.param.end())
    {
        *param = TestcaseParamCast<std::string>(it->second);
        if (CheckStringInSet(*param, supported_set) == true) return true;
        if (json_val_def.hidden == HIDDEN_FALSE)
            LogMessage(LOG_INFO, "Overwriting " + std::string(json_val_def.name) + ": " + *param);
    }
    else
    {
        *param = param_default;
        if (json_val_def.hidden == HIDDEN_FALSE)
            LogMessage(LOG_INFO, "Setting to default " + std::string(json_val_def.name) + ": " + *param);
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool TestInterface::GetJsonParamBool( Json_Val_Def_t json_val_def, bool *param, bool param_default )
{
    Json_Parameters_t::iterator it = FindJsonParam(&(m_test_parameters.param), json_val_def);
    if (it != m_test_parameters.param.end())
    {
        *param = TestcaseParamCast<bool>(it->second);
        if (json_val_def.hidden == HIDDEN_FALSE)
            LogMessage(LOG_INFO, "Overwriting " + std::string(json_val_def.name) + ": " + BoolToStr(*param));
    }
    else
    {
        *param = param_default;
        if (json_val_def.hidden == HIDDEN_FALSE)
            LogMessage(LOG_INFO, "Setting to default " + std::string(json_val_def.name) + ": " + BoolToStr(*param));
    }
    return false;
}
