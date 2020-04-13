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

#ifndef _INPUTPARSER_H
#define _INPUTPARSER_H

#include "xjsonparser.h"

class InputParser : public XJsonParser
{

private:
    Testcase_Parameters_t m_testcase_parameters[TEST_MAX];

    Json_Definition_t m_json_definition;

    std::string m_filename;

    std::ifstream m_test_source_ifs;
    std::string m_test_source_filename;

    const std::set<std::string> EMPTY_HIDDEN_PARAMETERS = {};

    // test_source JSON values
    const std::string TEST_SOURCE_JSON  = "json";
    const std::string TEST_SOURCE_FILE  = "file";
    const std::set<std::string> SUPPORTED_TEST_SOURCE = {
        TEST_SOURCE_JSON,
        TEST_SOURCE_FILE
    };

public:

    InputParser( std::string filename,  Global_Config_t global_config, std::atomic<bool> *gAbort );
    ~InputParser();

    bool Parse();

    void PrintRequiredNotFound( std::vector<std::string> node_title_in );

    Json_Definition_t GetJsonDefinition( TestType test_type, bool visible_only );

    bool ParseJsonParameters( TestType test_type );

    bool ParseDMATestSequenceParametersFromString( std::string test_sequence_file_name, std::vector<std::string> line_params, DMA_Test_Sequence_Parameters_t *test_seq_param );
    bool ParseDMATestSequenceParametersFromJson  ( std::string test_sequence_name,      uint param_index,                     DMA_Test_Sequence_Parameters_t *test_seq_param );

    bool ParseMemoryTestSequenceParametersFromString( std::string test_sequence_file_name,  std::vector<std::string> line_params,  Memory_Test_Sequence_Parameters_t *test_seq_param );
    bool ParseMemoryTestSequenceParametersFromJson  ( std::string test_sequence_name,       uint param_index,                      Memory_Test_Sequence_Parameters_t *test_seq_param );

    bool ParsePowerTestSequenceParametersFromString( std::string test_sequence_file_name,   std::vector<std::string> line_params, Power_Test_Sequence_Parameters_t *test_seq_param );
    bool ParsePowerTestSequenceParametersFromJson  ( std::string test_sequence_name,        uint param_index,                     Power_Test_Sequence_Parameters_t *test_seq_param );

    bool ParseGTMACTestSequenceParametersFromString( std::string test_sequence_file_name, std::vector<std::string> line_params, GTMAC_Test_Sequence_Parameters_t *test_seq_param );
    bool ParseGTMACTestSequenceParametersFromJson  ( std::string test_sequence_name,      uint param_index,                     GTMAC_Test_Sequence_Parameters_t *test_seq_param );

    Json_Parameters_t       GetDeviceParameters();
    Testcase_Parameters_t   GetTestcaseParameters( TestType test_type );

    bool CheckStringInSet( std::string value, std::set<std::string> test_set, std::set<std::string> hidden_test_set  );

    bool ParseJsonParamStr ( std::vector<std::string> node_title, Json_Val_Def_t json_val_def, Json_Parameters_t *json_parameters );
    bool ParseJsonParamBool( std::vector<std::string> node_title, Json_Val_Def_t json_val_def, Json_Parameters_t *json_parameters );

    template<typename T> bool ParseJsonParamInt( std::vector<std::string> node_title, Json_Val_Def_t json_val_def, Json_Parameters_t *json_parameters )
    {
        T param;
        if (NodeExists(node_title) == true)
        {
            if (ExtractNodeValueInt<T>(node_title, &param) == RET_FAILURE) return RET_FAILURE;
            InsertJsonParam<T>(json_parameters, json_val_def, param);
        }
        return RET_SUCCESS;
    }
    template<typename T> bool ParseJsonParamDouble( std::vector<std::string> node_title, Json_Val_Def_t json_val_def, Json_Parameters_t *json_parameters )
    {
        T param;
        if (NodeExists(node_title) == true)
        {
            if (ExtractNodeValueDouble<T>(node_title, &param) == RET_FAILURE) return RET_FAILURE;
            InsertJsonParam<T>(json_parameters, json_val_def, param);
        }
        return RET_SUCCESS;
    }

    uint ReadCsvLine( uint num_param_max, std::vector<std::string> *line_params );
    bool CheckForQuote( std::string name, std::string *msg );
    bool ParseTestSequence( TestType test_type, Json_Parameters_t *json_parameters );

    template<typename T> void AppendTestSequenceParameters( Json_Parameters_t *json_parameters, T test_seq_param )
    {
        std::vector<T> tmp_val;
        Json_Parameters_t::iterator it = FindJsonParam(json_parameters, TEST_SEQUENCE_MEMBER);
        if (it != json_parameters->end())
            tmp_val = TestcaseParamCast<std::vector<T>>(it->second);
        tmp_val.push_back(test_seq_param);
        EraseJsonParam(json_parameters, TEST_SEQUENCE_MEMBER);
        InsertJsonParam<std::vector<T>>(json_parameters, TEST_SEQUENCE_MEMBER, tmp_val);
    }

    void PrintJsonDefintion( TestType test_type, Json_Definition_t json_definition );
    void PrintJsonParameters( LogLevel Level, TestType test_type, Json_Parameters_t json_parameters );
    void PrintDMATestSequence   ( LogLevel Level, std::vector<DMA_Test_Sequence_Parameters_t>    dma_test_sequence_parameters );
    void PrintMemoryTestSequence( LogLevel Level, std::vector<Memory_Test_Sequence_Parameters_t> memory_test_sequence_parameters );
    void PrintPowerTestSequence ( LogLevel Level, std::vector<Power_Test_Sequence_Parameters_t>  power_test_sequence_parameters );
    void PrintGTMACTestSequence ( LogLevel Level, std::vector<GTMAC_Test_Sequence_Parameters_t>  gtmac_test_sequence_parameters );

    void SetLogMsgTestType( std::string log_msg_test_type );

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename T> bool ConvertStringToNum( std::string name, std::string str_in, T *value )
    {
        if(ConvString2Num<T>(str_in, value) == RET_FAILURE)
        {
            LogMessage(LOG_FAILURE, "ConvertStringToNum: Failed to convert " + name + ": \"" + str_in + "\". Check parameter type");
            return RET_FAILURE;
        }
        return RET_SUCCESS;
    }
};

#endif /* _INPUTPARSER_H */
