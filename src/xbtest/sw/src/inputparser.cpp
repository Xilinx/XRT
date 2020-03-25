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

#include "inputparser.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

InputParser::InputParser( std::string filename, Global_Config_t global_config, std::atomic<bool> *gAbort )
{
    m_log = Logging::getInstance(); // handle to the logging
    m_log_msg_test_type = "INPUTPARSER: ";
    m_global_config = global_config;
    m_filename = filename; // Test JSON file location
    m_abort = gAbort;
}
InputParser::~InputParser() { ClearParser(); }

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool InputParser::Parse()
{
    std::vector<std::string> node_title;

    for (uint i=0; i < TEST_MAX; i++)
        m_testcase_parameters[i].test_exists = false;

    /////////////////////////////////////////////////////////////////////////////////
    // Parse JSON file
    /////////////////////////////////////////////////////////////////////////////////
    // check json configuration file exists
    std::ifstream infile(m_filename);
    if (infile.good() == false)
    {
        LogMessage(LOG_FAILURE, "Test JSON does not exist: " + m_filename);
        return RET_FAILURE;
    }
    // Initialize json parser and reader
    LogMessage(LOG_INFO, "Using Test JSON: " + m_filename);
    m_json_parser = json_parser_new();

    GError *error = NULL;
    json_parser_load_from_file(m_json_parser, m_filename.c_str(), &error);
    if (error)
    {
        LogMessage(LOG_FAILURE, "Unable to parse Test JSON: " + std::string(error->message));
        g_error_free(error);
        g_object_unref(m_json_parser);
        return RET_FAILURE;
    }
    m_json_root_node    = json_parser_get_root(m_json_parser);
    m_json_reader       = json_reader_new(NULL);
    json_reader_set_root(m_json_reader, m_json_root_node);

    /////////////////////////////////////////////////////////////////////////////////
    // Check the json file content against the defined members
    /////////////////////////////////////////////////////////////////////////////////
    LogMessage(LOG_DEBUG, "Check the JSON file content (top)");
    // First check content at top level
    m_json_definition = GetJsonDefinition(TEST_DEVICE, false);
    if (CheckMembers(m_json_definition) == RET_FAILURE)
    {
        Json_Definition_t visible_json_definition = GetJsonDefinition(TEST_DEVICE, true);
        PrintJsonDefintion(TEST_DEVICE, visible_json_definition); // Use test max for print
        return RET_FAILURE;
    }
    // Then check each element of testcase array
    std::vector<std::string> testcases_title = {TESTCASES_MEMBER};
    if (NodeExists(testcases_title) == true)
    {
        ExtractNode(testcases_title); // Move cursor to testcases array
        for (int j = 0; j < json_reader_count_elements(m_json_reader); j++) // For each element in testcases array
        {
            json_reader_read_element(m_json_reader, j); // Move cursor to testcases element
            // Type
            std::string test_type_str;
            node_title = {TYPE_MEMBER};
            if (NodeExists(node_title) == false)
            {
                PrintRequiredNotFound(node_title);
                return RET_FAILURE;
            }

            if (ExtractNodeValueStr(node_title, &(test_type_str)) == RET_FAILURE) return RET_FAILURE;
            if (CheckStringInSet(test_type_str, TEST_SUPPORTED_JSON_TYPE_VALUES, EMPTY_HIDDEN_PARAMETERS) == RET_FAILURE) return RET_FAILURE;

            TestType test_type = TestTypeStringToEnum(test_type_str);
            LogMessage(LOG_DEBUG, "Check the JSON file content (" + test_type_str + ")");

            // Get JSON definition depending on testcase type and check test json against the definition
            m_json_definition = GetJsonDefinition(test_type, false);

            if (CheckMembers(m_json_definition) == RET_FAILURE)
            {
                Json_Definition_t visible_json_definition = GetJsonDefinition(test_type, true);
                PrintJsonDefintion(test_type, visible_json_definition);
                return RET_FAILURE;
            }
            json_reader_end_element(m_json_reader); // Move back cursor to testcases array
        }
        for (uint ii = 0; ii < testcases_title.size(); ii++) // Move cursor back from testcases array
            json_reader_end_element(m_json_reader);
    }
    /////////////////////////////////////////////////////////////////////////////////
    // Get device parameters
    /////////////////////////////////////////////////////////////////////////////////
    ParseJsonParameters(TEST_DEVICE);
    // Set verbosity
    int verbosity = (int)m_global_config.verbosity;
    bool verbos_ret = GetVerbosity(&(m_testcase_parameters[TEST_DEVICE].param), &verbosity);
    if (verbos_ret == true)
    {
        LogMessage(LOG_FAILURE, VERBOSITY_FAILURE);
        return RET_FAILURE;
    }
    m_global_config.verbosity = static_cast<LogLevel>(verbosity);

    /////////////////////////////////////////////////////////////////////////////////
    // Get testcases parameters
    /////////////////////////////////////////////////////////////////////////////////
    if (NodeExists(testcases_title) == true)
    {
        ExtractNode(testcases_title); // Move cursor to testcases array
        for (int j = 0; j < json_reader_count_elements(m_json_reader); j++) // For each element in testcases array
        {
            json_reader_read_element(m_json_reader, j); // Move cursor to testcases element
            // Type
            std::string test_type_str;
            node_title = {TYPE_MEMBER};
            if (NodeExists(node_title) == false)
            {
                PrintRequiredNotFound(node_title);
                return RET_FAILURE;
            }
            if (ExtractNodeValueStr(node_title, &(test_type_str)) == RET_FAILURE) return RET_FAILURE;
            if (CheckStringInSet (test_type_str, TEST_SUPPORTED_JSON_TYPE_VALUES, EMPTY_HIDDEN_PARAMETERS) == RET_FAILURE) return RET_FAILURE;

            TestType test_type = TestTypeStringToEnum(test_type_str);
            // Parameters
            node_title = {PARAMETERS_MEMBER};
            if (NodeExists(node_title) == false)
            {
                PrintRequiredNotFound(node_title);
                return RET_FAILURE;
            }
            if (ParseJsonParameters(test_type) == RET_FAILURE) return RET_FAILURE;
            json_reader_end_element(m_json_reader); // Move back cursor to testcases array
        }
        for (uint ii = 0; ii < testcases_title.size(); ii++ ) // Move cursor back from testcases array
            json_reader_end_element(m_json_reader);
    }
    return RET_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void InputParser::PrintRequiredNotFound( std::vector<std::string> node_title_in )
{
    LogMessage(LOG_FAILURE, "Required parameter not found in Test JSON: " + StrVectToStr(node_title_in, "."));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Json_Definition_t InputParser::GetJsonDefinition( TestType test_type, bool visible_only )
{
    Json_Params_Def_t parameters_definition;
    switch(test_type)
    {
        case TEST_DMA:          parameters_definition = DMA_PARAMETERS_DEFINITION;          break;
        case TEST_MEMORY_DDR:   parameters_definition = MEMORY_PARAMETERS_DEFINITION;       break;
        case TEST_MEMORY_HBM:   parameters_definition = MEMORY_PARAMETERS_DEFINITION;       break;
        case TEST_POWER:        parameters_definition = POWER_PARAMETERS_DEFINITION;        break;
        case TEST_GT:           parameters_definition = GT_PARAMETERS_DEFINITION;           break;
        case TEST_GT_MAC:       parameters_definition = GT_MAC_PARAMETERS_DEFINITION;       break;
        case TEST_DEVICE_MGT:   parameters_definition = DEVICE_MGT_PARAMETERS_DEFINITION;   break;
        case TEST_DEVICE:       parameters_definition = DEVICE_PARAMETERS_DEFINITION;       break;
        default: break;
    }

    if (test_type == TEST_DEVICE)   LogMessage(LOG_DEBUG, "Get " + TestTypeToString(test_type) + " parameters definition");
    else                            LogMessage(LOG_DEBUG, "Get " + TestTypeToString(test_type) + " testcase parameters definition");

    Json_Definition_t json_definition;
    if (test_type == TEST_DEVICE)
    {
        json_definition.insert( Definition_t({TESTCASES_MEMBER}, JSON_NODE_ARRAY));
    }
    else
    {
        json_definition.insert( Definition_t({TYPE_MEMBER},        JSON_NODE_VALUE));
        json_definition.insert( Definition_t({PARAMETERS_MEMBER},  JSON_NODE_OBJECT));
    }
    for (auto json_val_def: parameters_definition)
    {
        std::vector<std::string> node_title;
        if (test_type == TEST_DEVICE) node_title = {json_val_def.name};
        else                          node_title = {PARAMETERS_MEMBER, json_val_def.name};

        if ((json_val_def.hidden == HIDDEN_FALSE) || ((visible_only == false) && (json_val_def.hidden == HIDDEN_TRUE)))
            json_definition.insert( Definition_t(node_title, json_val_def.node_type));
    }
    return json_definition;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool InputParser::ParseJsonParameters( TestType test_type )
{
    Json_Params_Def_t parameters_definition;
    switch(test_type)
    {
        case TEST_DMA:          parameters_definition = DMA_PARAMETERS_DEFINITION;         break;
        case TEST_MEMORY_DDR:   parameters_definition = MEMORY_PARAMETERS_DEFINITION;      break;
        case TEST_MEMORY_HBM:   parameters_definition = MEMORY_PARAMETERS_DEFINITION;      break;
        case TEST_POWER:        parameters_definition = POWER_PARAMETERS_DEFINITION;       break;
        case TEST_GT:           parameters_definition = GT_PARAMETERS_DEFINITION;          break;
        case TEST_GT_MAC:       parameters_definition = GT_MAC_PARAMETERS_DEFINITION;      break;
        case TEST_DEVICE_MGT:   parameters_definition = DEVICE_MGT_PARAMETERS_DEFINITION;  break;
        case TEST_DEVICE:       parameters_definition = DEVICE_PARAMETERS_DEFINITION;      break;
        default: break;
    }
    if (test_type == TEST_DEVICE)   LogMessage(LOG_DEBUG, "Parsing " + TestTypeToString(test_type) + " parameters");
    else                            LogMessage(LOG_DEBUG, "Parsing " + TestTypeToString(test_type) + " testcase parameters");

    Testcase_Parameters_t testcase_parameters;
    testcase_parameters.test_exists = true; // Set Test exists

    // Parse all test parameter except test_sequence as test_source need to be parsed first
    for (auto json_val_def: parameters_definition)
    {
        std::vector<std::string> node_title;
        if (test_type == TEST_DEVICE)   node_title = {json_val_def.name};
        else                            node_title = {PARAMETERS_MEMBER, json_val_def.name};

        if ((NodeExists(node_title) == false) && (json_val_def.required == REQUIRED_TRUE))
        {
            PrintRequiredNotFound(node_title);
            return RET_FAILURE;
        }
        if (json_val_def.node_type == JSON_NODE_VALUE)
        {
            switch(json_val_def.typeId)
            {
                case TYPE_ID_INT:
                    if (ParseJsonParamInt<int>(node_title, json_val_def, &(testcase_parameters.param)) == RET_FAILURE) return RET_FAILURE;
                    break;
                case TYPE_ID_UINT:
                    if (ParseJsonParamInt<uint>(node_title, json_val_def, &(testcase_parameters.param)) == RET_FAILURE) return RET_FAILURE;
                    break;
                case TYPE_ID_UINT64_T:
                    if (ParseJsonParamInt<uint64_t>(node_title, json_val_def, &(testcase_parameters.param)) == RET_FAILURE) return RET_FAILURE;
                    break;
                case TYPE_ID_FLOAT:
                    if (ParseJsonParamDouble<float>(node_title, json_val_def, &(testcase_parameters.param)) == RET_FAILURE) return RET_FAILURE;
                    break;
                case TYPE_ID_DOUBLE:
                    if (ParseJsonParamDouble<double>(node_title, json_val_def, &(testcase_parameters.param)) == RET_FAILURE) return RET_FAILURE;
                    break;
                case TYPE_ID_BOOL:
                    if (ParseJsonParamBool(node_title, json_val_def, &(testcase_parameters.param)) == RET_FAILURE) return RET_FAILURE;
                    break;
                case TYPE_ID_STRING:
                    if (ParseJsonParamStr(node_title, json_val_def, &(testcase_parameters.param)) == RET_FAILURE) return RET_FAILURE;
                    break;
                default: break;
            }
        }
    }
    // Parse test_sequence after other parameters as it requires test_source
    for (auto json_val_def: parameters_definition)
    {
        if (json_val_def.typeId == TYPE_ID_TEST_SEQUENCE)
        {
            std::vector<std::string> node_title = {PARAMETERS_MEMBER, TEST_SEQUENCE_MEMBER.name};
            if (NodeExists(node_title) == false)
            {
                PrintRequiredNotFound(node_title);
                return RET_FAILURE;
            }
            if (ParseTestSequence(test_type, &(testcase_parameters.param)) == RET_FAILURE) return RET_FAILURE;
        }
    }
    m_testcase_parameters[test_type] = testcase_parameters;

    PrintJsonParameters(LOG_DEBUG, test_type, testcase_parameters.param);
    return RET_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool InputParser::ParseDMATestSequenceParametersFromString( std::string test_sequence_file_name, std::vector<std::string> line_params, DMA_Test_Sequence_Parameters_t *test_seq_param )
{
    std::string name;
    if (line_params.size() != NUM_TEST_SEQ_PARAM_DMA)
    {
        LogMessage(LOG_FAILURE, m_test_source_filename + ": Wrong number of parameters in " + test_sequence_file_name + ": " + std::to_string(line_params.size()) + ". Expected " + std::to_string(NUM_TEST_SEQ_PARAM_DMA) + " parameters");
        return RET_FAILURE;
    }

    // Get param 0
    name = test_sequence_file_name + "." + DURATION;
    if (ConvertStringToNum<uint>(name, line_params[0], &(test_seq_param->duration)) == RET_FAILURE) return RET_FAILURE;
    // Get param 1
    test_seq_param->mem_type = line_params[1];
    name = test_sequence_file_name + "." + MEM_TYPE;
    if (CheckForQuote(name, &(test_seq_param->mem_type)) == RET_FAILURE) return RET_FAILURE;
    // Get param 2
    if (StrMatchNoCase(line_params[2], std::string("\"" + TEST_SEQUENCE_MODE_ALL + "\"")) == true)
    {
        test_seq_param->test_sequence_mode = TEST_SEQUENCE_MODE_ALL;
    }
    else
    {
        name = test_sequence_file_name + "." + MEM_INDEX;
        if (ConvertStringToNum<uint> (name, line_params[2], &(test_seq_param->mem_index)) == RET_FAILURE) return RET_FAILURE;
        test_seq_param->test_sequence_mode = TEST_SEQUENCE_MODE_SINGLE;
    }
    // Get param 3
    name = test_sequence_file_name + "." + BUFFER_SIZE;
    if (ConvertStringToNum<uint64_t>(name, line_params[3], &(test_seq_param->buffer_size)) == RET_FAILURE) return RET_FAILURE;
    return RET_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool InputParser::ParseDMATestSequenceParametersFromJson( std::string test_sequence_name, uint param_index, DMA_Test_Sequence_Parameters_t *test_seq_param )
{
    std::string name = test_sequence_name + ".";
    switch(param_index)
    {
        case 0: // duration
            name += DURATION;
            if (GetNodeValueInt<uint>(name, &(test_seq_param->duration)) == RET_FAILURE) return RET_FAILURE;
            break;
        case 1: // mem_type
            name += MEM_TYPE;
            if (GetNodeValueStr(name, &(test_seq_param->mem_type)) == RET_FAILURE) return RET_FAILURE;
            break;
        case 2: // mem_index
            name += MEM_INDEX;
            GType node_value_type;
            if (GetJsonNodeValueType(name, &node_value_type) == RET_FAILURE) return RET_FAILURE;
            if (node_value_type == G_TYPE_STRING)
            {
                if (GetNodeValueStr(name, &(test_seq_param->test_sequence_mode)) == RET_FAILURE) return RET_FAILURE;
                if (StrMatchNoCase(test_seq_param->test_sequence_mode, TEST_SEQUENCE_MODE_ALL) == false)
                {
                    LogMessage(LOG_FAILURE, "ParseDMATestSequenceParametersFromJson: Wrong value in " + name + ": " + test_seq_param->test_sequence_mode + ". Expected memory index or \"" + TEST_SEQUENCE_MODE_ALL + "\"");
                    return RET_FAILURE;
                }
            }
            else if (node_value_type == G_TYPE_INT64)
            {
                if (GetNodeValueInt<uint>(name, &(test_seq_param->mem_index)) == RET_FAILURE) return RET_FAILURE;
                test_seq_param->test_sequence_mode = TEST_SEQUENCE_MODE_SINGLE;
            }

            break;
        case 3: // buffer_size
            name += BUFFER_SIZE;
            if (GetNodeValueInt<uint64_t>(name, &(test_seq_param->buffer_size)) == RET_FAILURE) return RET_FAILURE;
            break;
    }
    return RET_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool InputParser::ParseMemoryTestSequenceParametersFromString( std::string test_sequence_file_name, std::vector<std::string> line_params, Memory_Test_Sequence_Parameters_t *test_seq_param )
{
    std::string name;
    // Check number of parameters
    test_seq_param->num_param = line_params.size();
    if ((test_seq_param->num_param != NUM_TEST_SEQ_PARAM_MEMORY_ALT) && (test_seq_param->num_param != NUM_TEST_SEQ_PARAM_MEMORY_ONLY) && (test_seq_param->num_param != NUM_TEST_SEQ_PARAM_MEMORY_DEF))
    {
        LogMessage(LOG_FAILURE, "ParseMemoryTestSequenceParametersFromString: Wrong number of parameters in " + test_sequence_file_name + ": " + std::to_string(test_seq_param->num_param)
        + ". Expected " + std::to_string(NUM_TEST_SEQ_PARAM_MEMORY_ALT)
        + ", or "       + std::to_string(NUM_TEST_SEQ_PARAM_MEMORY_ONLY)
        + ", or "       + std::to_string(NUM_TEST_SEQ_PARAM_MEMORY_DEF) + " parameters");
        return true;
    }
    // Get param 0
    name = test_sequence_file_name + "." + TEST_MODE;
    test_seq_param->test_mode = line_params[0];
    if (CheckForQuote(name, &(test_seq_param->test_mode)) == RET_FAILURE) return RET_FAILURE;
    if (CheckStringInSet(test_seq_param->test_mode, SUPPORTED_MEM_TEST_MODE, EMPTY_HIDDEN_PARAMETERS) == RET_FAILURE) return RET_FAILURE;

    // Check number of parameters more finely
    if (StrMatchNoCase(test_seq_param->test_mode, MEM_CTRL_TEST_MODE_STOP_TEST) == true)
    {
        if (test_seq_param->num_param != NUM_TEST_SEQ_PARAM_MEMORY_DEF)
        {
            LogMessage(LOG_FAILURE, "ParseMemoryTestSequenceParametersFromString: Wrong number of parameters in " + test_sequence_file_name + ": " + std::to_string(test_seq_param->num_param)
            + ". Expected " + std::to_string(NUM_TEST_SEQ_PARAM_MEMORY_DEF) + " parameters for test_mode: " + test_seq_param->test_mode);
            return RET_FAILURE;
        }
    }
    else
    {
        uint max_num_param;
        if      (StrMatchNoCase(test_seq_param->test_mode, MEM_CTRL_TEST_MODE_ALTERNATE_WR_RD_TEST)   == true) max_num_param = NUM_TEST_SEQ_PARAM_MEMORY_ALT;
        else if (StrMatchNoCase(test_seq_param->test_mode, MEM_CTRL_TEST_MODE_ONLY_WR_TEST)           == true) max_num_param = NUM_TEST_SEQ_PARAM_MEMORY_ONLY;
        else if (StrMatchNoCase(test_seq_param->test_mode, MEM_CTRL_TEST_MODE_ONLY_RD_TEST)           == true) max_num_param = NUM_TEST_SEQ_PARAM_MEMORY_ONLY;
        if ((test_seq_param->num_param != max_num_param) && (test_seq_param->num_param != NUM_TEST_SEQ_PARAM_MEMORY_DEF))
        {
            LogMessage(LOG_FAILURE, "ParseMemoryTestSequenceParametersFromString: Wrong number of parameters in " + test_sequence_file_name + ": " + std::to_string(test_seq_param->num_param)
            + ". Expected " + std::to_string(NUM_TEST_SEQ_PARAM_MEMORY_DEF)
            + " or " + std::to_string(max_num_param) + " parameters for test_mode: " + test_seq_param->test_mode);
            return RET_FAILURE;
        }
    }
    // Get param 1
    if (test_seq_param->num_param > 1)
    {
        name = test_sequence_file_name + "." + DURATION;
        if (ConvertStringToNum<uint>(name, line_params[1], &(test_seq_param->duration)) == RET_FAILURE) return RET_FAILURE;
    }
    // Get param 2
    if (test_seq_param->num_param > 2)
    {
        if (test_seq_param->test_mode == MEM_CTRL_TEST_MODE_ONLY_RD_TEST)
        {
            name = test_sequence_file_name + "." + RD_START_ADDR;
            if (ConvertStringToNum<uint64_t>(name, line_params[2], &(test_seq_param->rd_start_addr)) == RET_FAILURE) return RET_FAILURE;
        }
        else
        {
            name = test_sequence_file_name + "." + WR_START_ADDR;
            if (ConvertStringToNum<uint64_t>(name, line_params[2], &(test_seq_param->wr_start_addr)) == RET_FAILURE) return RET_FAILURE;
        }
    }
    // Get param 3
    if (test_seq_param->num_param > 3)
    {
        if (test_seq_param->test_mode == MEM_CTRL_TEST_MODE_ONLY_RD_TEST)
        {
            name = test_sequence_file_name + "." + RD_BURST_SIZE;
            if (ConvertStringToNum<uint>(name, line_params[3], &(test_seq_param->rd_burst_size)) == RET_FAILURE) return RET_FAILURE;
        }
        else
        {
            name = test_sequence_file_name + "." + WR_BURST_SIZE;
            if (ConvertStringToNum<uint>(name, line_params[3], &(test_seq_param->wr_burst_size)) == RET_FAILURE) return RET_FAILURE;
        }
    }
    // Get param 4
    if (test_seq_param->num_param > 4)
    {
        if (test_seq_param->test_mode == MEM_CTRL_TEST_MODE_ONLY_RD_TEST)
        {
            name = test_sequence_file_name + "." + RD_NUM_XFER;
            if (ConvertStringToNum<uint>(name, line_params[4], &(test_seq_param->rd_num_xfer)) == RET_FAILURE) return RET_FAILURE;
        }
        else
        {
            name = test_sequence_file_name + "." + WR_NUM_XFER;
            if (ConvertStringToNum<uint>(name, line_params[4], &(test_seq_param->wr_num_xfer)) == RET_FAILURE) return RET_FAILURE;
        }
    }
    // Get param 5
    if (test_seq_param->num_param > 5)
    {
        name = test_sequence_file_name + "." + RD_START_ADDR;
        if (ConvertStringToNum<uint64_t>(name, line_params[5], &(test_seq_param->rd_start_addr)) == RET_FAILURE) return RET_FAILURE;
    }
    // Get param 6
    if (test_seq_param->num_param > 6)
    {
        name = test_sequence_file_name + "." + RD_BURST_SIZE;
        if (ConvertStringToNum<uint>(name, line_params[6], &(test_seq_param->rd_burst_size)) == RET_FAILURE) return RET_FAILURE;
    }
    // Get param 7
    if (test_seq_param->num_param > 7)
    {
        name = test_sequence_file_name + "." + RD_NUM_XFER;
        if (ConvertStringToNum<uint>(name, line_params[7], &(test_seq_param->rd_num_xfer)) == RET_FAILURE) return RET_FAILURE;
    }
    return RET_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool InputParser::ParseMemoryTestSequenceParametersFromJson( std::string test_sequence_name, uint param_index, Memory_Test_Sequence_Parameters_t *test_seq_param )
{
    std::string name = test_sequence_name + ".";
    switch(param_index)
    {
        case 0: // type
            name += TYPE_MEMBER;
            if (GetNodeValueStr(name, &(test_seq_param->test_mode)) == RET_FAILURE) return RET_FAILURE;
            if (CheckStringInSet(test_seq_param->test_mode, SUPPORTED_MEM_TEST_MODE, EMPTY_HIDDEN_PARAMETERS) == RET_FAILURE) return RET_FAILURE;

            // Check number of parameters more finely
            if (StrMatchNoCase(test_seq_param->test_mode, MEM_CTRL_TEST_MODE_STOP_TEST) == true)
            {
                if (test_seq_param->num_param != NUM_TEST_SEQ_PARAM_MEMORY_DEF)
                {
                    LogMessage(LOG_FAILURE, "Wrong number of parameters in " + test_sequence_name + ": " + std::to_string(test_seq_param->num_param)
                    + ". Expected " + std::to_string(NUM_TEST_SEQ_PARAM_MEMORY_DEF) + " parameters for test_mode: " + test_seq_param->test_mode);
                    return RET_FAILURE;
                }
            }
            else
            {
                uint max_num_param;
                if      (StrMatchNoCase(test_seq_param->test_mode, MEM_CTRL_TEST_MODE_ALTERNATE_WR_RD_TEST)   == true) max_num_param = NUM_TEST_SEQ_PARAM_MEMORY_ALT;
                else if (StrMatchNoCase(test_seq_param->test_mode, MEM_CTRL_TEST_MODE_ONLY_WR_TEST)           == true) max_num_param = NUM_TEST_SEQ_PARAM_MEMORY_ONLY;
                else if (StrMatchNoCase(test_seq_param->test_mode, MEM_CTRL_TEST_MODE_ONLY_RD_TEST)           == true) max_num_param = NUM_TEST_SEQ_PARAM_MEMORY_ONLY;
                if ((test_seq_param->num_param != max_num_param) && (test_seq_param->num_param != NUM_TEST_SEQ_PARAM_MEMORY_DEF))
                {
                    LogMessage(LOG_FAILURE, "Wrong number of parameters in " + test_sequence_name + ": " + std::to_string(test_seq_param->num_param)
                    + ". Expected " + std::to_string(NUM_TEST_SEQ_PARAM_MEMORY_DEF)
                    + " or "        + std::to_string(max_num_param) + " parameters for test_mode: " + test_seq_param->test_mode);
                    return RET_FAILURE;
                }
            }
            break;
        case 1: // duration
            name += DURATION;
            if (GetNodeValueInt<uint>(name, &(test_seq_param->duration)) == RET_FAILURE) return RET_FAILURE;
            break;
        case 2: // wr_start_addr/rd_start_addr
            if (test_seq_param->test_mode == MEM_CTRL_TEST_MODE_ONLY_RD_TEST)
            {
                name += RD_START_ADDR;
                if (GetNodeValueInt<uint64_t>(name, &(test_seq_param->rd_start_addr)) == RET_FAILURE) return RET_FAILURE;
            }
            else
            {
                name += WR_START_ADDR;
                if (GetNodeValueInt<uint64_t>(name, &(test_seq_param->wr_start_addr)) == RET_FAILURE) return RET_FAILURE;
            }
            break;
        case 3: // wr_burst_size/rd_burst_size
            if (test_seq_param->test_mode == MEM_CTRL_TEST_MODE_ONLY_RD_TEST)
            {
                name += RD_BURST_SIZE;
                if (GetNodeValueInt<uint>(name, &(test_seq_param->rd_burst_size)) == RET_FAILURE) return RET_FAILURE;
            }
            else
            {
                name += WR_BURST_SIZE;
                if (GetNodeValueInt<uint>(name, &(test_seq_param->wr_burst_size)) == RET_FAILURE) return RET_FAILURE;
            }
            break;
        case 4: // wr_num_xfer/rd_num_xfer
            if (test_seq_param->test_mode == MEM_CTRL_TEST_MODE_ONLY_RD_TEST)
            {
                name += RD_NUM_XFER;
                if (GetNodeValueInt<uint>(name, &(test_seq_param->rd_num_xfer)) == RET_FAILURE) return RET_FAILURE;
            }
            else
            {
                name += WR_NUM_XFER;
                if (GetNodeValueInt<uint>(name, &(test_seq_param->wr_num_xfer)) == RET_FAILURE) return RET_FAILURE;
            }
            break;
        case 5: // rd_start_addr
            name += RD_START_ADDR;
            if (GetNodeValueInt<uint64_t>(name, &(test_seq_param->rd_start_addr)) == RET_FAILURE) return RET_FAILURE;
            break;
        case 6: // rd_burst_size
            name += RD_BURST_SIZE;
            if (GetNodeValueInt<uint>(name, &(test_seq_param->rd_burst_size)) == RET_FAILURE) return RET_FAILURE;
            break;
        case 7: // rd_num_xfer
            name += RD_NUM_XFER;
            if (GetNodeValueInt<uint>(name, &(test_seq_param->rd_num_xfer)) == RET_FAILURE) return RET_FAILURE;
            break;
    }
    return RET_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool InputParser::ParsePowerTestSequenceParametersFromString( std::string test_sequence_file_name, std::vector<std::string> line_params, Power_Test_Sequence_Parameters_t *test_seq_param )
{
    std::string name;
    if (line_params.size() != NUM_TEST_SEQ_PARAM_POWER)
    {
        LogMessage(LOG_FAILURE, m_test_source_filename + ": Wrong number of parameters in " + test_sequence_file_name + ": " + std::to_string(line_params.size()) + ". Expected " + std::to_string(NUM_TEST_SEQ_PARAM_POWER) + " parameters");
        return RET_FAILURE;
    }
    // Get param 0
    name = test_sequence_file_name + "." + DURATION;
    if (ConvertStringToNum<uint>(name, line_params[0], &(test_seq_param->duration)) == RET_FAILURE) return RET_FAILURE;
    // Get param 1
    name = test_sequence_file_name + "." + POWER_TOGGLE;
    if (ConvertStringToNum<uint>(name, line_params[1], &(test_seq_param->power_toggle)) == RET_FAILURE) return RET_FAILURE;
    return RET_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool InputParser::ParsePowerTestSequenceParametersFromJson( std::string test_sequence_name, uint param_index, Power_Test_Sequence_Parameters_t *test_seq_param )
{
    std::string name = test_sequence_name + ".";
    switch(param_index)
    {
        case 0: // duration
            name += DURATION;
            if (GetNodeValueInt<uint>(name, &(test_seq_param->duration)) == RET_FAILURE) return RET_FAILURE;
            break;
        case 1: // power_toggle
            name += POWER_TOGGLE;
            if (GetNodeValueInt<uint>(name, &(test_seq_param->power_toggle)) == RET_FAILURE) return RET_FAILURE;
            break;
    }
    return RET_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool InputParser::ParseGTMACTestSequenceParametersFromString( std::string test_sequence_file_name, std::vector<std::string> line_params, GTMAC_Test_Sequence_Parameters_t *test_seq_param )
{
    std::string name;
    if (line_params.size() != NUM_TEST_SEQ_PARAM_POWER)
    {
        LogMessage(LOG_FAILURE, m_test_source_filename + ": Wrong number of parameters in " + test_sequence_file_name + ": " + std::to_string(line_params.size()) + ". Expected " + std::to_string(NUM_TEST_SEQ_PARAM_POWER) + " parameters");
        return RET_FAILURE;
    }
    // Get param 0
    name = test_sequence_file_name + "." + DURATION;
    if (ConvertStringToNum<uint>(name, line_params[0], &(test_seq_param->duration)) == RET_FAILURE) return RET_FAILURE;
    // Get param 1
    name = test_sequence_file_name + "." + MODE;
    test_seq_param->mode = line_params[1];
    if (CheckForQuote(name, &(test_seq_param->mode)) == RET_FAILURE) return RET_FAILURE;
    return RET_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool InputParser::ParseGTMACTestSequenceParametersFromJson( std::string test_sequence_name, uint param_index, GTMAC_Test_Sequence_Parameters_t *test_seq_param )
{
    std::string name = test_sequence_name + ".";
    switch(param_index)
    {
        case 0: // duration
            name += DURATION;
            if (GetNodeValueInt<uint>(name, &(test_seq_param->duration)) == RET_FAILURE) return RET_FAILURE;
            break;
        case 1: // mode
            name += MODE;
            if (GetNodeValueStr(name, &(test_seq_param->mode)) == RET_FAILURE) return RET_FAILURE;
            break;
    }
    return RET_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Json_Parameters_t       InputParser::GetDeviceParameters()                          { return m_testcase_parameters[TEST_DEVICE].param; }
Testcase_Parameters_t   InputParser::GetTestcaseParameters( TestType test_type )    { return m_testcase_parameters[test_type]; }

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool InputParser::CheckStringInSet( std::string value, std::set<std::string> test_set, std::set<std::string> hidden_test_set )
{
    bool ret_pass = RET_SUCCESS;

    std::set<std::string> test_set_lowercase;
    for (auto test : test_set)
    {
        std::transform(test.begin(), test.end(), test.begin(), tolower);
        test_set_lowercase.insert(test);
    }
    std::set<std::string> hidden_test_set_lowercase;
    for (auto test : hidden_test_set)
    {
        std::transform(test.begin(), test.end(), test.begin(), tolower);
        hidden_test_set_lowercase.insert(test);
    }
    std::string value_lower = value;
    std::transform(value_lower.begin(), value_lower.end(), value_lower.begin(), tolower);
    if (test_set_lowercase.count(value_lower) == 0) // Not in test set
    {
        ret_pass = RET_FAILURE;

        if (hidden_test_set_lowercase.empty() == false) // Don't check when empty
        {
            if (hidden_test_set_lowercase.count(value_lower) != 0) // In hidden test set
            {
                ret_pass = RET_SUCCESS;
            }
        }
    }
    if (ret_pass == RET_FAILURE)
    {
        std::string set_str = "";
        for (auto f : test_set) {
            set_str += "\"" + f + "\", ";
        }
        LogMessage(LOG_FAILURE, "CheckStringInSet: Invalid json member/value - \"" + value + "\"");
        LogMessage(LOG_INFO,    "CheckStringInSet: Supported json member/value: " + set_str);
    }
    return ret_pass;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool InputParser::ParseJsonParamStr( std::vector<std::string> node_title, Json_Val_Def_t json_val_def, Json_Parameters_t *json_parameters )
{
    std::string param;
    if (NodeExists(node_title) == true) {
        if (ExtractNodeValueStr(node_title, &param) == RET_FAILURE) return RET_FAILURE;
        InsertJsonParam<std::string>(json_parameters, json_val_def, param);
    }
    return RET_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool InputParser::ParseJsonParamBool( std::vector<std::string> node_title, Json_Val_Def_t json_val_def, Json_Parameters_t *json_parameters )
{
    bool param;
    if (NodeExists(node_title) == true) {
        if (ExtractNodeValueBool(node_title, &param) == RET_FAILURE) return RET_FAILURE;
        InsertJsonParam<bool>(json_parameters, json_val_def, param);
    }
    return RET_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint InputParser::ReadCsvLine( uint num_param_max, std::vector<std::string> *line_params )
{
    int ret = 0;
    line_params->clear();
    if (!m_test_source_ifs.eof())
    {
        uint param_cnt = 0;
        std::string line;
        getline(m_test_source_ifs, line);
        if (line.size() != 0)
        {
            if (*line.rbegin() == '\r')
            {
                line.erase(line.length()-1, 1);
            }
            LogMessage(LOG_DESIGNER, m_test_source_filename + ": Get line: " + line);
            size_t delim_pos;
            do
            {
                delim_pos = line.find(',');
                std::string param;
                std::string param_nopadd;
                if (delim_pos == std::string::npos) // no matches were found, last parameter
                    param = line;
                else
                    param = line.substr(0, delim_pos);

                // Remove blank spaces before parameter
                param_nopadd = "";
                bool begin_param = false;
                for ( std::string::iterator it=param.begin(); it!=param.end(); ++it)
                {
                    if (*it  != ' ')
                        begin_param = true;
                    if (begin_param == true)
                        param_nopadd = param_nopadd + *it ;
                }
                param = param_nopadd;

                // Remove blank spaces after parameter
                param_nopadd = "";
                bool end_param = false;
                for ( std::string::reverse_iterator rit=param.rbegin(); rit!=param.rend(); ++rit)
                {
                    if (*rit != ' ')
                        end_param = true;
                    if (end_param == true)
                        param_nopadd = *rit + param_nopadd;
                }
                param = param_nopadd;

                // FInish parsing parameter
                line_params->push_back(param);

                LogMessage(LOG_DESIGNER, m_test_source_filename + ": Parameter parsed in line: " + param);
                LogMessage(LOG_DESIGNER, m_test_source_filename + ": Number of parameters parsed: " + std::to_string(line_params->size()));

                line.erase(0, delim_pos + 1);
                param_cnt ++;
            } while (delim_pos != std::string::npos);

            if (param_cnt > num_param_max)
            {
                ret = 1;
                LogMessage(LOG_FAILURE, m_test_source_filename + ": Too many parameters: " + std::to_string(param_cnt));
            }
        }
        else
        {
            ret = 2; // Empty line
        }
    }
    else
    {
        ret = 2; // End of file
    }

    if (ret == 2)
        LogMessage(LOG_DESIGNER, "End of input configuration file: " + m_test_source_filename);

    if (ret != 0)
        m_test_source_ifs.close();

    return ret;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool InputParser::CheckForQuote( std::string name, std::string *msg )
{
    std::string msg_last  = (*msg).substr((*msg).size()-1,(*msg).size()); // last character
    std::string msg_first = (*msg).substr(0,1); // first character
    if ((StrMatchNoCase(msg_last, "\"")) && (StrMatchNoCase(msg_first, "\"")))
    {
        (*msg) = (*msg).substr(1,(*msg).size()-2); // Remove quotes
    }
    else
    {
        LogMessage(LOG_FAILURE, "CheckForQuote: Syntax error in " + name + ": " + (*msg) + " expecting double quote: e.g. \"msg\"");
        return RET_FAILURE;
    }
    return RET_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool InputParser::ParseTestSequence( TestType test_type, Json_Parameters_t *json_parameters )
{
    std::vector<std::string> testsequence_title = {PARAMETERS_MEMBER, TEST_SEQUENCE_MEMBER.name};
    if (NodeExists(testsequence_title) == true)
    {
        LogMessage(LOG_DEBUG, "Parsing " + TestTypeToString(test_type) + " " + TEST_SEQUENCE_MEMBER.name);
        ExtractNode(testsequence_title); // Move cursor to test sequence array
        bool parse_failure = RET_SUCCESS;
        bool stop_parsing = false;
        uint parse_error_cnt = 0;
        for (int j = 0; j < json_reader_count_elements(m_json_reader) && (stop_parsing == false); j++) // For each element in test sequence array
        {
            bool parse_it_failure = RET_SUCCESS;
            DMA_Test_Sequence_Parameters_t      test_seq_param_dma;
            Memory_Test_Sequence_Parameters_t   test_seq_param_memory;
            Power_Test_Sequence_Parameters_t    test_seq_param_power;
            GTMAC_Test_Sequence_Parameters_t    test_seq_param_gt_mac;

            bool test_source_exists = false;
            std::string test_source_val;
            Json_Parameters_t::iterator it = FindJsonParam(json_parameters, TEST_SOURCE_MEMBER);
            if (it != json_parameters->end())
            {
                test_source_exists = true;
                test_source_val = TestcaseParamCast<std::string>(it->second);
            }

            json_reader_read_element(m_json_reader, j); // Move cursor to test sequence element
            std::string test_sequence_name = TestTypeToString(test_type) + " " + TEST_SEQUENCE_MEMBER.name + "[" + std::to_string(j) + "]";

            if ((test_source_exists == true) && (StrMatchNoCase(test_source_val, TEST_SOURCE_FILE) == true))
            {
                // Mode test sequence in input CSV file
                uint count_elements = (uint)(json_reader_count_elements(m_json_reader));
                if (count_elements != 1)
                {
                    LogMessage(LOG_FAILURE, "ParseTestSequence: Only 1 parameter (input_file) expected, but found " + std::to_string(count_elements) + " parameters in " + test_sequence_name);
                    parse_it_failure = RET_FAILURE;
                }
                json_reader_read_element(m_json_reader, 0); // Move cursor to test parameter element
                std::string name = test_sequence_name + ".input_file";
                if (parse_it_failure == RET_SUCCESS) parse_it_failure = GetNodeValueStr(name, &(m_test_source_filename));
                if (parse_it_failure == RET_SUCCESS)
                {
                    LogMessage(LOG_DEBUG, "Read " + test_sequence_name + " file: " + m_test_source_filename);
                    m_test_source_ifs.open(m_test_source_filename, std::ifstream::in);
                }
                if ((parse_it_failure == RET_SUCCESS) && (m_test_source_ifs.fail() == true))
                {
                    LogMessage(LOG_FAILURE, "ParseTestSequence: Couldn't open " + test_sequence_name + " file: " + m_test_source_filename);
                    parse_it_failure = RET_FAILURE;
                }
                if ((parse_it_failure == RET_SUCCESS) && (m_test_source_ifs.good() == false))
                {
                    LogMessage(LOG_FAILURE, "ParseTestSequence: Empty " + test_sequence_name + " file: " + m_test_source_filename);
                    m_test_source_ifs.close();
                    parse_it_failure = RET_FAILURE;
                }
                if (parse_it_failure == RET_SUCCESS)
                {
                    uint test_cnt = 0; // Use a test count when test_source is 'file', increment for each element (file line)
                    while ((*m_abort == false) && (stop_parsing == false))
                    {
                        std::string test_sequence_file_name = test_sequence_name + ".input_file[" + std::to_string(test_cnt) + "]";
                        parse_it_failure = RET_SUCCESS; // Reset Status for each line
                        uint num_param_max = 0;
                        switch(test_type)
                        {
                            case TEST_DMA:          num_param_max = NUM_TEST_SEQ_PARAM_DMA;         break;
                            case TEST_MEMORY_DDR:   num_param_max = NUM_TEST_SEQ_PARAM_MEMORY_ALT;  break;
                            case TEST_MEMORY_HBM:   num_param_max = NUM_TEST_SEQ_PARAM_MEMORY_ALT;  break;
                            case TEST_POWER:        num_param_max = NUM_TEST_SEQ_PARAM_POWER;       break;
                            case TEST_GT_MAC:       num_param_max = NUM_TEST_SEQ_PARAM_GTMAC;       break;
                            default: break;
                        }
                        std::vector<std::string> line_params;
                        uint rd_csv_ret = ReadCsvLine(num_param_max, &line_params);
                        if (rd_csv_ret == 2)
                        {
                            LogMessage(LOG_DESIGNER, "ParseTestSequence: Finished reading " + test_sequence_name + " file: " + m_test_source_filename);
                            break; // End of file
                        }
                        else if (rd_csv_ret == 1)
                        {
                            parse_it_failure = RET_FAILURE;
                        }
                        else
                        {
                            switch(test_type)
                            {
                                case TEST_DMA:          parse_it_failure = ParseDMATestSequenceParametersFromString   (test_sequence_file_name, line_params, &test_seq_param_dma);     break;
                                case TEST_MEMORY_DDR:   parse_it_failure = ParseMemoryTestSequenceParametersFromString(test_sequence_file_name, line_params, &test_seq_param_memory);  break;
                                case TEST_MEMORY_HBM:   parse_it_failure = ParseMemoryTestSequenceParametersFromString(test_sequence_file_name, line_params, &test_seq_param_memory);  break;
                                case TEST_POWER:        parse_it_failure = ParsePowerTestSequenceParametersFromString (test_sequence_file_name, line_params, &test_seq_param_power);   break;
                                case TEST_GT_MAC:       parse_it_failure = ParseGTMACTestSequenceParametersFromString (test_sequence_file_name, line_params, &test_seq_param_gt_mac);  break;
                                default: break;
                            }
                        }
                        if (parse_it_failure == RET_SUCCESS)
                        {
                            switch(test_type)
                            {
                                case TEST_DMA:          AppendTestSequenceParameters<DMA_Test_Sequence_Parameters_t>   (json_parameters, test_seq_param_dma);    break;
                                case TEST_MEMORY_DDR:   AppendTestSequenceParameters<Memory_Test_Sequence_Parameters_t>(json_parameters, test_seq_param_memory); break;
                                case TEST_MEMORY_HBM:   AppendTestSequenceParameters<Memory_Test_Sequence_Parameters_t>(json_parameters, test_seq_param_memory); break;
                                case TEST_POWER:        AppendTestSequenceParameters<Power_Test_Sequence_Parameters_t> (json_parameters, test_seq_param_power);  break;
                                case TEST_GT_MAC:       AppendTestSequenceParameters<GTMAC_Test_Sequence_Parameters_t> (json_parameters, test_seq_param_gt_mac); break;
                                default: break;
                            }
                        }
                        else
                        {
                            LogMessage(LOG_FAILURE, "ParseTestSequence: " + test_sequence_name + ": invalid parameters" );
                            parse_error_cnt++;
                            if (parse_error_cnt >= MAX_NUM_PARSER_ERROR) stop_parsing = true;
                        }
                        parse_failure |= parse_it_failure;
                        test_cnt ++;
                    }
                }
                else
                {
                    LogMessage(LOG_FAILURE, "ParseTestSequence: " + test_sequence_name + ": invalid parameters" );
                    parse_error_cnt++;
                    if (parse_error_cnt >= MAX_NUM_PARSER_ERROR) stop_parsing = true;
                }
                parse_failure |= parse_it_failure;
                json_reader_end_element(m_json_reader); // Move back cursor to test parameter array
            }
            else
            {
                // Mode test sequence in input JSON file
                // Check number of parameters
                uint count_elements = (uint)(json_reader_count_elements(m_json_reader));
                switch(test_type)
                {
                    case TEST_DMA:
                        if (count_elements != NUM_TEST_SEQ_PARAM_DMA)
                        {
                            LogMessage(LOG_FAILURE, "ParseTestSequence: Wrong number of parameters in " + test_sequence_name + ": " + std::to_string(count_elements) + ". Expected " + std::to_string(NUM_TEST_SEQ_PARAM_DMA) + " parameters");
                            parse_it_failure = RET_FAILURE;
                        }
                        break;
                    // Grouping cases TEST_MEMORY_DDR TEST_MEMORY_HBM
                    case TEST_MEMORY_DDR: case TEST_MEMORY_HBM:
                        if ((count_elements != NUM_TEST_SEQ_PARAM_MEMORY_ALT) && (count_elements != NUM_TEST_SEQ_PARAM_MEMORY_ONLY) && (count_elements != NUM_TEST_SEQ_PARAM_MEMORY_DEF))
                        {
                            LogMessage(LOG_FAILURE, "ParseTestSequence: Wrong number of parameters in " + test_sequence_name + ": " + std::to_string(count_elements)
                            + ". Expected " + std::to_string(NUM_TEST_SEQ_PARAM_MEMORY_ALT)
                            + ", or "       + std::to_string(NUM_TEST_SEQ_PARAM_MEMORY_ONLY)
                            + ", or "       + std::to_string(NUM_TEST_SEQ_PARAM_MEMORY_DEF) + " parameters");
                            parse_it_failure = RET_FAILURE;
                        }
                        test_seq_param_memory.num_param = count_elements;
                        break;
                    case TEST_POWER:
                        if (count_elements != NUM_TEST_SEQ_PARAM_POWER)
                        {
                            LogMessage(LOG_FAILURE, "ParseTestSequence: Wrong number of parameters in " + test_sequence_name + ": " + std::to_string(count_elements) + ". Expected " + std::to_string(NUM_TEST_SEQ_PARAM_POWER) + " parameters");
                            parse_it_failure = RET_FAILURE;
                        }
                        break;
                    case TEST_GT_MAC:
                        if (count_elements != NUM_TEST_SEQ_PARAM_GTMAC)
                        {
                            LogMessage(LOG_FAILURE, "ParseTestSequence: Wrong number of parameters in " + test_sequence_name + ": " + std::to_string(count_elements) + ". Expected " + std::to_string(NUM_TEST_SEQ_PARAM_GTMAC) + " parameters");
                            parse_it_failure = RET_FAILURE;
                        }
                        break;
                    default: break;
                }
                for (uint i = 0; i < count_elements && (parse_it_failure == RET_SUCCESS); i++) // For each element in test sequence array
                {
                    json_reader_read_element(m_json_reader, i); // Move cursor to test parameter element
                    switch(test_type)
                    {
                        case TEST_DMA:         parse_it_failure = ParseDMATestSequenceParametersFromJson   (test_sequence_name, i, &test_seq_param_dma);     break;
                        case TEST_MEMORY_DDR:  parse_it_failure = ParseMemoryTestSequenceParametersFromJson(test_sequence_name, i, &test_seq_param_memory);  break;
                        case TEST_MEMORY_HBM:  parse_it_failure = ParseMemoryTestSequenceParametersFromJson(test_sequence_name, i, &test_seq_param_memory);  break;
                        case TEST_POWER:       parse_it_failure = ParsePowerTestSequenceParametersFromJson (test_sequence_name, i, &test_seq_param_power);   break;
                        case TEST_GT_MAC:      parse_it_failure = ParseGTMACTestSequenceParametersFromJson (test_sequence_name, i, &test_seq_param_gt_mac);  break;
                        default: break;
                    }
                    json_reader_end_element(m_json_reader); // Move back cursor to test parameter array
                }

                if (parse_it_failure == RET_SUCCESS)
                {
                    switch(test_type)
                    {
                        case TEST_DMA:          AppendTestSequenceParameters<DMA_Test_Sequence_Parameters_t>   (json_parameters, test_seq_param_dma);    break;
                        case TEST_MEMORY_DDR:   AppendTestSequenceParameters<Memory_Test_Sequence_Parameters_t>(json_parameters, test_seq_param_memory); break;
                        case TEST_MEMORY_HBM:   AppendTestSequenceParameters<Memory_Test_Sequence_Parameters_t>(json_parameters, test_seq_param_memory); break;
                        case TEST_POWER:        AppendTestSequenceParameters<Power_Test_Sequence_Parameters_t> (json_parameters, test_seq_param_power);  break;
                        case TEST_GT_MAC:       AppendTestSequenceParameters<GTMAC_Test_Sequence_Parameters_t> (json_parameters, test_seq_param_gt_mac); break;
                        default: break;
                    }
                }
                else
                {
                    LogMessage(LOG_FAILURE, "ParseTestSequence: " + test_sequence_name + ": invalid parameters" );
                    parse_error_cnt++;
                    if (parse_error_cnt >= MAX_NUM_PARSER_ERROR) stop_parsing = true;
                }
                parse_failure |= parse_it_failure;
            }
            json_reader_end_element(m_json_reader); // Move back cursor to test sequence array
        }
        for (uint ii = 0; ii < testsequence_title.size(); ii++ ) // Move cursor back from test sequence array
            json_reader_end_element(m_json_reader);
        if (parse_failure == RET_FAILURE)
        {
            LogMessage(LOG_FAILURE, "ParseTestSequence: Some parameters in " + TestTypeToString(test_type) + " " + TEST_SEQUENCE_MEMBER.name + " are not valid, check error messages above");
            return RET_FAILURE;
        }
    }
    else
    {
        LogMessage(LOG_FAILURE, "ParseTestSequence: Required parameter not found for " + TestTypeToString(test_type) + " testcase: " + StrVectToStr(testsequence_title, "."));
        return RET_FAILURE;
    }
    return RET_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void InputParser::PrintJsonDefintion( TestType test_type, Json_Definition_t json_definition )
{
    if (test_type == TEST_DEVICE)   LogMessage(LOG_INFO, "Supported JSON " + TestTypeToString(test_type) + " parameters:");
    else                            LogMessage(LOG_INFO, "Supported JSON " + TestTypeToString(test_type) + " testcase parameters:");
    Json_Definition_t::iterator it = json_definition.begin();
    while(it != json_definition.end())
    {
        std::vector<std::string>    node_title  = it->first;
        JsonNodeType                node_type   = it->second;
        LogMessage(LOG_INFO, "\t - " + StrVectToStr(node_title, ".") + " (" + JsonNodeTypeToString(node_type) + ")");
        it++;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void InputParser::PrintJsonParameters( LogLevel Level, TestType test_type, Json_Parameters_t json_parameters )
{
    Json_Params_Def_t parameters_definition;
    switch(test_type)
    {
        case TEST_DMA:          parameters_definition = DMA_PARAMETERS_DEFINITION;         break;
        case TEST_MEMORY_DDR:   parameters_definition = MEMORY_PARAMETERS_DEFINITION;      break;
        case TEST_MEMORY_HBM:   parameters_definition = MEMORY_PARAMETERS_DEFINITION;      break;
        case TEST_POWER:        parameters_definition = POWER_PARAMETERS_DEFINITION;       break;
        case TEST_GT:           parameters_definition = GT_PARAMETERS_DEFINITION;          break;
        case TEST_GT_MAC:       parameters_definition = GT_MAC_PARAMETERS_DEFINITION;      break;
        case TEST_DEVICE_MGT:   parameters_definition = DEVICE_MGT_PARAMETERS_DEFINITION;  break;
        case TEST_DEVICE:       parameters_definition = DEVICE_PARAMETERS_DEFINITION;      break;
        default: break;
    }
    if (test_type == TEST_DEVICE)   LogMessage(Level, TestTypeToString(test_type) + " parameters:");
    else                            LogMessage(Level, TestTypeToString(test_type) + " testcase parameters:");

    for (auto json_val_def: parameters_definition)
    {
        Json_Parameters_t::iterator it = json_parameters.begin();
        while(it != json_parameters.end())
        {
            if (json_val_def.name == it->first)
            {
                switch(json_val_def.typeId)
                {
                    case TYPE_ID_INT:       LogMessage(Level, "\t - " + json_val_def.name + ": " + std::to_string(TestcaseParamCast<int>(it->second)));      break;
                    case TYPE_ID_UINT:      LogMessage(Level, "\t - " + json_val_def.name + ": " + std::to_string(TestcaseParamCast<uint>(it->second)));     break;
                    case TYPE_ID_UINT64_T:  LogMessage(Level, "\t - " + json_val_def.name + ": " + std::to_string(TestcaseParamCast<uint64_t>(it->second))); break;
                    case TYPE_ID_FLOAT:     LogMessage(Level, "\t - " + json_val_def.name + ": " + std::to_string(TestcaseParamCast<float>(it->second)));    break;
                    case TYPE_ID_DOUBLE:    LogMessage(Level, "\t - " + json_val_def.name + ": " + std::to_string(TestcaseParamCast<double>(it->second)));   break;
                    case TYPE_ID_BOOL:      LogMessage(Level, "\t - " + json_val_def.name + ": " + BoolToStr(TestcaseParamCast<bool>(it->second)));          break;
                    case TYPE_ID_STRING:    LogMessage(Level, "\t - " + json_val_def.name + ": " + TestcaseParamCast<std::string>(it->second));              break;
                    case TYPE_ID_TEST_SEQUENCE:
                        switch(test_type)
                        {
                            case TEST_DMA:          PrintDMATestSequence   (Level, TestcaseParamCast<std::vector<DMA_Test_Sequence_Parameters_t>>   (it->second)); break;
                            case TEST_MEMORY_DDR:   PrintMemoryTestSequence(Level, TestcaseParamCast<std::vector<Memory_Test_Sequence_Parameters_t>>(it->second)); break;
                            case TEST_MEMORY_HBM:   PrintMemoryTestSequence(Level, TestcaseParamCast<std::vector<Memory_Test_Sequence_Parameters_t>>(it->second)); break;
                            case TEST_POWER:        PrintPowerTestSequence (Level, TestcaseParamCast<std::vector<Power_Test_Sequence_Parameters_t>> (it->second)); break;
                            case TEST_GT_MAC:       PrintGTMACTestSequence (Level, TestcaseParamCast<std::vector<GTMAC_Test_Sequence_Parameters_t>> (it->second)); break;
                            default: break;
                        }
                        break;
                    default: break;
                }
            }
            it++;
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void InputParser::PrintDMATestSequence( LogLevel Level, std::vector<DMA_Test_Sequence_Parameters_t> dma_test_sequence_parameters )
{
    LogMessage(Level, "\t - " + TEST_SEQUENCE_MEMBER.name + ":");
    uint test_seq_param_idx = 1;
    for (auto test_seq_param: dma_test_sequence_parameters)
    {
        std::string msg = "\t\t " + std::to_string(test_seq_param_idx) + ") ";
        msg += std::to_string(test_seq_param.duration)   + ", ";
        msg += test_seq_param.mem_type                   + ", ";
        if (StrMatchNoCase(test_seq_param.test_sequence_mode, TEST_SEQUENCE_MODE_ALL) == true)
            msg += TEST_SEQUENCE_MODE_ALL + ", ";
        else
            msg += std::to_string(test_seq_param.mem_index)  + ", ";
        msg += std::to_string(test_seq_param.buffer_size); // End
        LogMessage(Level, msg);
        test_seq_param_idx++;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void InputParser::PrintMemoryTestSequence( LogLevel Level, std::vector<Memory_Test_Sequence_Parameters_t> memory_test_sequence_parameters )
{
    LogMessage(Level, "\t - " + TEST_SEQUENCE_MEMBER.name + ":");
    uint test_seq_param_idx = 1;
    for (auto test_seq_param: memory_test_sequence_parameters)
    {
        std::string msg = "\t\t " + std::to_string(test_seq_param_idx) + ") ";
        msg += test_seq_param.test_mode                      + ", ";
        if (test_seq_param.num_param == NUM_TEST_SEQ_PARAM_MEMORY_DEF)
        {
            msg += std::to_string(test_seq_param.duration); // End
        }
        else if (test_seq_param.num_param > NUM_TEST_SEQ_PARAM_MEMORY_DEF)
        {
            msg += std::to_string(test_seq_param.duration)  + ", ";
            if (StrMatchNoCase(test_seq_param.test_mode, MEM_CTRL_TEST_MODE_ONLY_RD_TEST) == true)
            {
                msg += std::to_string(test_seq_param.rd_start_addr) + ", ";
                msg += std::to_string(test_seq_param.rd_burst_size) + ", ";
                msg += std::to_string(test_seq_param.rd_num_xfer); // End
            }
            else if (StrMatchNoCase(test_seq_param.test_mode, MEM_CTRL_TEST_MODE_ONLY_WR_TEST) == true)
            {
                msg += std::to_string(test_seq_param.wr_start_addr) + ", ";
                msg += std::to_string(test_seq_param.wr_burst_size) + ", ";
                msg += std::to_string(test_seq_param.wr_num_xfer); // End
            }
            else if (StrMatchNoCase(test_seq_param.test_mode, MEM_CTRL_TEST_MODE_ALTERNATE_WR_RD_TEST) == true)
            {
                msg += std::to_string(test_seq_param.wr_start_addr) + ", ";
                msg += std::to_string(test_seq_param.wr_burst_size) + ", ";
                msg += std::to_string(test_seq_param.wr_num_xfer)   + ", ";
                msg += std::to_string(test_seq_param.rd_start_addr) + ", ";
                msg += std::to_string(test_seq_param.rd_burst_size) + ", ";
                msg += std::to_string(test_seq_param.rd_num_xfer); // End
            }
        }
        LogMessage(Level, msg);
        test_seq_param_idx++;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void InputParser::PrintPowerTestSequence( LogLevel Level, std::vector<Power_Test_Sequence_Parameters_t> power_test_sequence_parameters )
{
    LogMessage(Level, "\t - " + TEST_SEQUENCE_MEMBER.name + ":");
    uint test_seq_param_idx = 1;
    for (auto test_seq_param: power_test_sequence_parameters)
    {
        std::string msg = "\t\t " + std::to_string(test_seq_param_idx) + ") ";
        msg += std::to_string(test_seq_param.duration)   + ", ";
        msg += std::to_string(test_seq_param.power_toggle); // End
        LogMessage(Level, msg);
        test_seq_param_idx++;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void InputParser::PrintGTMACTestSequence( LogLevel Level, std::vector<GTMAC_Test_Sequence_Parameters_t> gtmac_test_sequence_parameters )
{
    LogMessage(Level, "\t - " + TEST_SEQUENCE_MEMBER.name + ":");
    uint test_seq_param_idx = 1;
    for (auto test_seq_param: gtmac_test_sequence_parameters)
    {
        std::string msg = "\t\t " + std::to_string(test_seq_param_idx) + ") ";
        msg += std::to_string(test_seq_param.duration)   + ", ";
        msg += test_seq_param.mode; // End
        LogMessage(Level, msg);
        test_seq_param_idx++;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void InputParser::SetLogMsgTestType( std::string log_msg_test_type ) { m_log_msg_test_type = log_msg_test_type; }
