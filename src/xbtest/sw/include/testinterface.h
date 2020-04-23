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

#ifndef _TESTINTERFACE_H
#define _TESTINTERFACE_H

#include "xbtestcommon.h"
#include "logging.h"
#include "deviceinterface.h"

class TestInterface
{

public:

    typedef enum {
        TR_NOT_SET,
        TR_PASSED,
        TR_FAILED,
        TR_ABORTED
    } TestResult;
    typedef enum {
        TS_NOT_SET,
        TS_PRE_SETUP,
        TS_RUNNING,
        TS_POST_TEARDOWN,
        TS_COMPLETE
    } TestState;

    TestResult m_result = TR_NOT_SET;
    TestState m_state   = TS_NOT_SET;

    Logging *m_log = nullptr;
    std::string m_log_msg_test_type;
    Global_Config_t m_global_config;

    Testcase_Parameters_t m_test_parameters;

    std::string m_test_source;
    std::string m_test_source_filename;
    std::ifstream m_test_source_ifs;

    std::string ResultToString( TestResult Result );
    std::string StateToString( TestState State );

    // pure virtual methods
    virtual bool PreSetup() = 0;
	virtual void Run() = 0;
    virtual void PostTeardown() = 0;
    virtual void Abort() = 0;

    TestState GetState();
    TestResult GetResult();
    void PrintState();
    void PrintResult();

    bool CheckStringInSet ( std::string value, std::set<std::string> test_set );

    bool OpenOutputFile( std::string test_outputfile_name_in, std::ofstream *outputfile );
    void LogMessage ( LogLevel Level, std::string msg );
    bool GetJsonParamStr ( Json_Val_Def_t json_val_def, std::set<std::string> supported_set, std::string *param, std::string param_default );
    bool GetJsonParamBool( Json_Val_Def_t json_val_def, bool *param, bool param_default );

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename T> bool ConvertStringToNum( std::string param_name, std::string str_in, T *value )
    {
        bool ret_failure = ConvString2Num<T>(str_in, value);
        if (ret_failure == true)
            LogMessage(LOG_FAILURE, "Failed to convert parameter \"" + param_name + "\" = \"" + str_in + "\". Check parameter type");
        return ret_failure;
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename T> bool CheckParam ( std::string name, T request, T min, T max )
    {
        bool test_failure = false;
        if (request < min)
        {
            LogMessage(LOG_FAILURE, name + " below the minimum of " + std::to_string(min) + ": " + std::to_string(request));
            test_failure = true;
        }
        else if (request > max)
        {
            LogMessage(LOG_FAILURE, name + " above the maximum of " + std::to_string(max) + ": " + std::to_string(request));
            test_failure = true;
        }
        return test_failure;
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename T> bool CheckThresholdLoVsHi ( Json_Val_Def_t json_val_def_min, T param_min, Json_Val_Def_t json_val_def_max, T param_max )
    {
        bool test_failure = false;
        if (param_min > param_max)
        {
            LogMessage(LOG_FAILURE, "\"" + json_val_def_min.name + "\": " + std::to_string(param_min) + " is greater than \"" + json_val_def_max.name + "\": " + std::to_string(param_max));
            test_failure = true;
        }
        return test_failure;
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename T> bool GetJsonParamNum ( Json_Val_Def_t json_val_def, T param_min, T param_nom, T param_max, T *param  )
    {
        Json_Parameters_t::iterator it = FindJsonParam(&(m_test_parameters.param), json_val_def);
        if (it != m_test_parameters.param.end())
        {
            *param = TestcaseParamCast<T>(it->second);
            if (CheckParam<T> (json_val_def.name, *param, param_min, param_max) == true) return true;
            if (json_val_def.hidden == HIDDEN_FALSE)
                LogMessage(LOG_INFO, "Overwriting " + std::string(json_val_def.name) + ": " + std::to_string(*param));
        }
        else
        {
            *param = param_nom; // Default value
            if (json_val_def.hidden == HIDDEN_FALSE)
                LogMessage(LOG_INFO, "Setting to default " + std::string(json_val_def.name) + ": " + std::to_string(*param));
        }
        return false;
    }

};

#endif /* _TESTINTERFACE_H */
