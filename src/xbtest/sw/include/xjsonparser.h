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

#ifndef _XJSONPARSER_H
#define _XJSONPARSER_H

#include "xbtestcommon.h"
#include "logging.h"

class XJsonParser
{

public:

    std::string m_log_msg_test_type;

    std::atomic<bool> *m_abort;
    Logging *m_log = nullptr;
    Global_Config_t m_global_config;

    JsonParser *m_json_parser = NULL;
    JsonNode *m_json_root_node;
    JsonReader *m_json_reader = NULL;

    void LogMessage ( LogLevel Level, std::string msg );

    virtual bool Parse() = 0;

    void ClearParser();
    bool CheckReaderError();

    bool ReadMemberNoCase( std::string node_title_in );
    bool NodeExists( std::vector<std::string> node_title_in );
    bool ExtractNode( std::vector<std::string> node_title_in );
    bool GetNodeValueBool( std::string name, bool *node_value );
    bool GetNodeValueStr( std::string name, std::string *node_value );

    bool ExtractNodeValueBool( std::vector<std::string> node_title_in, bool *node_value );
    bool ExtractNodeValueStr( std::vector<std::string> node_title_in, std::string *node_value );
    bool ExtractNodeArrayStr( std::vector<std::string> node_title_in, std::vector<std::string> *node_array_value );
    bool PrintNodeValueStr( std::vector<std::string> node_title_in );

    JsonNodeType GetJsonNodeType();
    std::string JsonNodeTypeToString(JsonNodeType node_type);
    bool ListNodeMembers( gchar ***member_list );
    bool CheckMembers( Json_Definition_t json_definition );
    bool CheckMembersNextLevel( Json_Definition_t json_definition, std::vector<std::string> node_title_in );
    bool CheckMemberDefinition( Json_Definition_t json_definition, std::vector<std::string> node_title_in, JsonNodeType node_type_in );

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename T> bool GetNodeValueInt( std::string name, T *node_value )
    {
        // Get value inside node
        bool ret_failure = false;
        if (json_reader_is_value(m_json_reader))
        {
            JsonNode * node = json_reader_get_value(m_json_reader);
            if (json_node_get_value_type(node) == G_TYPE_INT64)
            {
                *node_value = (T)json_reader_get_int_value(m_json_reader);
                if (std::to_string((*node_value)) != std::to_string(json_reader_get_int_value(m_json_reader)))
                {
                    LogMessage(LOG_FAILURE, "GetNodeValueInt: Failed to convert value in " + name + " to integer");
                    ret_failure = true;
                }
            }
            else
            {
                LogMessage(LOG_FAILURE, "GetNodeValueInt: Value in " + name + " is not of type Integer");
                ret_failure = true;
            }
            ret_failure |= CheckReaderError();
        }
        else
        {
            LogMessage(LOG_FAILURE, "GetNodeValueInt: Unable to find value in " + name);
            ret_failure = true;
        }
        return ret_failure; // Fail to get
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename T> bool ExtractNodeValueInt( std::vector<std::string> node_title_in, T *node_value )
    {
        bool ret_failure = false;
        ret_failure = ExtractNode(node_title_in);
        if (ret_failure == false)
        {
            std::string full_node_title = StrVectToStr(node_title_in, ".");
            ret_failure = GetNodeValueInt(full_node_title, node_value);
            if (ret_failure == true)
            {
                std::string full_node_title = StrVectToStr(node_title_in, ".");
                LogMessage(LOG_FAILURE, "ExtractNodeValueInt: Unable to find integer value in the following node: " + full_node_title);
            }
        }

        for (uint ii = 0; ii < node_title_in.size(); ii++ )
            json_reader_end_element(m_json_reader);

        return ret_failure;
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename T> bool ExtractNodeArrayInt( std::vector<std::string> node_title_in, std::vector<T> *node_array_value )
    {
        bool ret_failure = false;
        ret_failure = ExtractNode(node_title_in);
        if (ret_failure == false)
        {
            if (json_reader_is_array(m_json_reader))
            {
                for (int j = 0; j < json_reader_count_elements(m_json_reader); j++)
                {
                    json_reader_read_element(m_json_reader, j);
                    T node_value;
                    std::string full_node_title = StrVectToStr(node_title_in, ".") + "[" + std::to_string(j) + "]";
                    ret_failure = GetNodeValueInt<T>(full_node_title, &node_value);
                    if (ret_failure == true) break;
                    node_array_value->push_back(node_value);
                    json_reader_end_element(m_json_reader);
                }
            }
            else
            {
                std::string full_node_title = StrVectToStr(node_title_in, ".");
                LogMessage(LOG_FAILURE, "ExtractNodeArrayInt: Unable to find array in the following node: " + full_node_title);
                ret_failure = true;
            }
        }

        for (uint ii = 0; ii < node_title_in.size(); ii++ )
            json_reader_end_element(m_json_reader);

        return ret_failure;
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename T> bool GetNodeValueDouble( std::string name, T *node_value )
    {
        // Get value inside node
        bool ret_failure = false;
        if (json_reader_is_value(m_json_reader))
        {
            JsonNode *node = json_reader_get_value(m_json_reader);
            if (json_node_get_value_type(node) == G_TYPE_DOUBLE)
            {
                *node_value = (T)json_reader_get_double_value(m_json_reader);
                if (std::to_string(*node_value) != std::to_string(json_reader_get_double_value(m_json_reader)))
                {
                    LogMessage(LOG_FAILURE, "GetNodeValueDouble: Failed to convert value in " + name + " to float");
                    ret_failure = true;
                }
            }
            else
            {
                LogMessage(LOG_FAILURE, "GetNodeValueDouble: Value in " + name + " is not of type Double");
                ret_failure = true;
            }
            ret_failure |= CheckReaderError();
        }
        else
        {
            LogMessage(LOG_FAILURE, "GetNodeValueDouble: Unable to find value in " + name);
            ret_failure = true;
        }
        return ret_failure;
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename T> bool ExtractNodeValueDouble( std::vector<std::string> node_title_in, T *node_value )
    {
        bool ret_failure = false;
        ret_failure = ExtractNode(node_title_in);
        if (ret_failure == false)
        {
            std::string full_node_title = StrVectToStr(node_title_in, ".");
            ret_failure = GetNodeValueDouble(full_node_title, node_value);
            if (ret_failure == true)
            {
                std::string full_node_title = StrVectToStr(node_title_in, ".");
                LogMessage(LOG_FAILURE, "ExtractNodeValueDouble: Unable to find double value in the following node: " + full_node_title);
            }
        }

        for (uint ii = 0; ii < node_title_in.size(); ii++ )
            json_reader_end_element(m_json_reader);

        return ret_failure;
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename T> bool ExtractNodeArrayDouble( std::vector<std::string> node_title_in, std::vector<T> *node_array_value )
    {
        bool ret_failure = false;
        ret_failure = ExtractNode(node_title_in);
        if (ret_failure == false)
        {
            if (json_reader_is_array(m_json_reader))
            {
                for (int j = 0; j < json_reader_count_elements(m_json_reader); j++)
                {
                    json_reader_read_element(m_json_reader, j);
                    T node_value;
                    std::string full_node_title = StrVectToStr(node_title_in, ".") + "[" + std::to_string(j) + "]";
                    ret_failure = GetNodeValueDouble<T>(full_node_title, &node_value);
                    if (ret_failure == true) break;
                    node_array_value->push_back(node_value);
                    json_reader_end_element(m_json_reader);
                }
            }
            else
            {
                std::string full_node_title = StrVectToStr(node_title_in, ".");
                LogMessage(LOG_FAILURE, "ExtractNodeArrayDouble: Unable to find array in the following node: " + full_node_title);
                ret_failure = true;
            }
        }

        for (uint ii = 0; ii < node_title_in.size(); ii++ )
            json_reader_end_element(m_json_reader);

        return false;
    }

    bool GetJsonNodeValueType( std::string name, GType *node_value_type );
    std::string JsonNodeValueTypeToString( GType node_value_type );

};

#endif /* _XBUTILDUMPPARSER_H */
