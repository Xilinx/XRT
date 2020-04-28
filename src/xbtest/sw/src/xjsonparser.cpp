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

#include "xjsonparser.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void XJsonParser::LogMessage ( LogLevel Level, std::string msg ) { m_log->LogMessage(Level, m_log_msg_test_type + msg, m_global_config.verbosity); }

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void XJsonParser::ClearParser()
{
    g_clear_object(&m_json_reader);
    g_clear_object(&m_json_parser);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool XJsonParser::CheckReaderError()
{
    bool ret_failure = false;

    const GError *error = json_reader_get_error(m_json_reader);

    if (error)
    {
        LogMessage(LOG_FAILURE, "JsonReaderError: " + std::string(error->message));
        ret_failure = true;
    }

   return ret_failure;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool XJsonParser::ReadMemberNoCase( std::string node_title_in )
{
    gchar **member_list;
    std::string node_name = "ThisNameIsNotAllowedInJSONs"; // This means we expected this name not allowed in all parsed jsons
    if (ListNodeMembers(&member_list) == false)
    {
        if (json_reader_count_members(m_json_reader) > 0)
        {
            for (int j = 0; member_list[j] != NULL; j++)
            {
                // Check that node is defined, without case sensitivity
                if (StrMatchNoCase(node_title_in, std::string(member_list[j])) == true)
                {
                    node_name = std::string(member_list[j]);
                    break;
                }
            }
        }
    }

    g_strfreev (member_list);
    // Get get node at next hierarchy level
    if (json_reader_read_member(m_json_reader, (const gchar *)node_name.c_str()) == true)
        return false;

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool XJsonParser::NodeExists( std::vector<std::string> node_title_in )
{
    bool node_exists = true;
    if (node_title_in.size() <= 0)
    {
        LogMessage(LOG_FAILURE, "NodeExists: Wrong title vector size");
        return false;
    }
    LogMessage(LOG_DESIGNER, "NodeExists: Checking node exists: " + StrVectToStr(node_title_in, "."));

    uint reference_count = 0;
    for (uint ii = 0; ii < node_title_in.size(); ii++ )
    {
        reference_count++;
        if (ReadMemberNoCase(node_title_in[ii]) == true) // Failed to get node
        {
            node_exists = false;
            break;
        }
    }

    for (uint ii = 0; ii < reference_count; ii++ )
        json_reader_end_element(m_json_reader);

    return node_exists;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool XJsonParser::ExtractNode( std::vector<std::string> node_title_in )
{
    bool ret_failure = false;
    if (node_title_in.size() <= 0)
    {
        LogMessage(LOG_FAILURE, "ExtractNode: Wrong title vector size");
        return true;
    }

    LogMessage(LOG_DESIGNER, "ExtractNode: Extracting node: " + StrVectToStr(node_title_in, "."));
    for (uint ii = 0; ii < node_title_in.size(); ii++ )
    {
        ret_failure = ReadMemberNoCase(node_title_in[ii]);
        if (ret_failure == true) break;
    }

    if (ret_failure == true)
    {
        std::string full_node_title = StrVectToStr(node_title_in, ".");
        LogMessage(LOG_FAILURE, "ExtractNode: Unable to find the following node: " + full_node_title);
    }

    return ret_failure;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool XJsonParser::GetNodeValueBool( std::string name, bool *node_value )
{
    // Get value inside node
    bool ret_failure = false;
    if (json_reader_is_value(m_json_reader))
    {
        JsonNode *node = json_reader_get_value(m_json_reader);
        if (json_node_get_value_type(node) == G_TYPE_BOOLEAN)
        {
            *node_value = json_reader_get_boolean_value(m_json_reader);
        }
        else
        {
            LogMessage(LOG_FAILURE, "GetNodeValueBool: Value in " + name + " is not of type Boolean");
            ret_failure = true;
        }
        ret_failure |= CheckReaderError();
    }
    else
    {
        LogMessage(LOG_FAILURE, "GetNodeValueBool: Unable to find value in " + name);
        ret_failure = true;
    }
    return ret_failure;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool XJsonParser::GetNodeValueStr( std::string name, std::string *node_value )
{
    // Get value inside node
    bool ret_failure = false;
    if (json_reader_is_value(m_json_reader))
    {
        JsonNode *node = json_reader_get_value(m_json_reader);
        if (json_node_get_value_type(node) == G_TYPE_STRING)
        {
            *node_value = json_reader_get_string_value(m_json_reader);
        }
        else
        {
            LogMessage(LOG_FAILURE, "GetNodeValueStr: Value in " + name + " is not of type String");
            ret_failure = true;
        }
        ret_failure |= CheckReaderError();
    }
    else
    {
        LogMessage(LOG_FAILURE, "GetNodeValueStr: Unable to find value in " + name);
        ret_failure = true;
    }
    return ret_failure;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool XJsonParser::ExtractNodeValueBool( std::vector<std::string> node_title_in, bool *node_value )
{
    bool ret_failure = false;
    ret_failure = ExtractNode(node_title_in);
    if (ret_failure == false)
    {
        std::string full_node_title = StrVectToStr(node_title_in, ".");
        ret_failure = GetNodeValueBool(full_node_title, node_value);
        if (ret_failure == true)
        {
            std::string full_node_title = StrVectToStr(node_title_in, ".");
            LogMessage(LOG_FAILURE, "ExtractNodeValueBool: Unable to find boolean value in the following node: " + full_node_title);
        }
    }

    for (uint ii = 0; ii < node_title_in.size(); ii++ )
        json_reader_end_element(m_json_reader);

    return ret_failure;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool XJsonParser::ExtractNodeValueStr( std::vector<std::string> node_title_in, std::string *node_value )
{
    bool ret_failure = false;
    ret_failure = ExtractNode(node_title_in);
    if (ret_failure == false)
    {
        std::string full_node_title = StrVectToStr(node_title_in, ".");
        ret_failure = GetNodeValueStr(full_node_title, node_value);
        if (ret_failure == true)
        {
            LogMessage(LOG_FAILURE, "ExtractNodeValueStr: Unable to find string value in the following node: " + full_node_title);
        }
    }
    for (uint ii = 0; ii < node_title_in.size(); ii++ )
        json_reader_end_element(m_json_reader);
    return ret_failure;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool XJsonParser::ExtractNodeArrayStr( std::vector<std::string> node_title_in, std::vector<std::string> *node_array_value )
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
                std::string node_value;
                std::string full_node_title = StrVectToStr(node_title_in, ".") + "[" + std::to_string(j) + "]";
                ret_failure = GetNodeValueStr(full_node_title, &node_value);
                if (ret_failure == true) break;
                node_array_value->push_back(node_value);
                json_reader_end_element(m_json_reader);
            }
        }
        else
        {
            ret_failure = true;
        }
    }

    for (uint ii = 0; ii < node_title_in.size(); ii++ )
        json_reader_end_element(m_json_reader);

    return ret_failure;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool XJsonParser::PrintNodeValueStr( std::vector<std::string> node_title_in )
{
    bool ret_failure = false;
    std::string node_value;

    ret_failure = ExtractNodeValueStr( node_title_in, &node_value );
    if (ret_failure == false)
        LogMessage(LOG_DEBUG, StrVectToStr(node_title_in, ".") + " = " + node_value);

    return ret_failure;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

JsonNodeType XJsonParser::GetJsonNodeType()
{
    JsonNodeType node_type = JSON_NODE_NULL;
    if      (json_reader_is_object(m_json_reader))  node_type = JSON_NODE_OBJECT;
    else if (json_reader_is_array(m_json_reader))   node_type = JSON_NODE_ARRAY;
    else if (json_reader_is_value(m_json_reader))   node_type = JSON_NODE_VALUE;
    return node_type;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::string XJsonParser::JsonNodeTypeToString(JsonNodeType node_type)
{
    std::string node_type_str   = "UNKNOWN";
    if      (node_type == JSON_NODE_OBJECT) node_type_str = "object";
    else if (node_type == JSON_NODE_ARRAY)  node_type_str = "array";
    else if (node_type == JSON_NODE_VALUE)  node_type_str = "value";
    else if (node_type == JSON_NODE_NULL)   node_type_str = "null";
    return node_type_str;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool XJsonParser::ListNodeMembers( gchar ***member_list )
{
    *member_list = json_reader_list_members(m_json_reader);
    if (member_list == NULL) {
        json_reader_end_member (m_json_reader);
        LogMessage(LOG_FAILURE, "ListNodeMembers: Unable to list member");
        return true;
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool XJsonParser::CheckMembers( Json_Definition_t json_definition )
{
    bool ret_failure = false;
    gchar **member_list;
    if (ListNodeMembers(&member_list) == true) {
        LogMessage(LOG_FAILURE, "CheckMembers: Unable to get root member list");
        ret_failure = true;
    }

    if (json_reader_count_members(m_json_reader) > 0)
    {
        for (int j = 0; member_list[j] != NULL && (ret_failure == false); j++)
        {
            // Check that nodes in first level title+type are defined
            std::string node_name(member_list[j]);
            std::vector<std::string> node_title = {node_name};

            LogMessage(LOG_DESIGNER, "Checking node: " + StrVectToStr(node_title, "."));
            if (ReadMemberNoCase(node_name) == true) ret_failure = true;
            JsonNodeType node_type;
            if (ret_failure == false)
            {
                node_type = GetJsonNodeType();
                if (CheckMemberDefinition(json_definition, node_title, node_type) == true)
                    ret_failure = true;
            }
            if (ret_failure == false)
            {
                if (node_type == JSON_NODE_OBJECT)
                {
                    if (CheckMembersNextLevel(json_definition, node_title) == true)
                        ret_failure = true;
                }
            }
            json_reader_end_member(m_json_reader);
        }
    }
    g_strfreev (member_list);
    return ret_failure;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool XJsonParser::CheckMembersNextLevel( Json_Definition_t json_definition, std::vector<std::string> node_title_in )
{
    bool ret_failure = false;
    gchar **member_list;
    if (ListNodeMembers(&member_list) == true) {
        std::string full_node_title = StrVectToStr(node_title_in, ".");
        LogMessage(LOG_FAILURE, "CheckMembersNextLevel: Unable to get member list for node: " + full_node_title);
        ret_failure = true;
    }

    if (json_reader_count_members(m_json_reader) > 0)
    {
        for (int j = 0; (member_list[j] != NULL) && (ret_failure == false); j++)
        {
            // Recursively check that all nodes title+type are defined
            std::string node_name(member_list[j]);
            std::vector<std::string> node_title = node_title_in;
            node_title.push_back(node_name);

            LogMessage(LOG_DESIGNER, "Checking node: " + StrVectToStr(node_title, "."));
            if (ReadMemberNoCase(node_name) == true)
                ret_failure = true;
            JsonNodeType node_type;
            if (ret_failure == false)
            {
                node_type = GetJsonNodeType();
                if (CheckMemberDefinition(json_definition, node_title, node_type) == true)
                    ret_failure = true;
            }
            if (ret_failure == false)
            {
                if (node_type == JSON_NODE_OBJECT)
                {
                    if (CheckMembersNextLevel(json_definition, node_title) == true)
                        ret_failure = true;
                }
            }
            json_reader_end_member(m_json_reader);
        }
    }
    g_strfreev (member_list);
    return ret_failure;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool XJsonParser::CheckMemberDefinition( Json_Definition_t json_definition, std::vector<std::string> node_title_in, JsonNodeType node_type_in )
{
    bool ret_failure = false;
    bool name_correct;
    bool type_correct;
    JsonNodeType expected_type;

    Json_Definition_t::iterator it = json_definition.begin();
    while(it != json_definition.end())
    {
        name_correct = false;
        type_correct = false;

        std::vector<std::string>    node_title  = it->first;
        JsonNodeType                node_type   = it->second;

        if (node_type == node_type_in)
        {
            type_correct = true;
        }
        if (StrMatchNoCase(StrVectToStr(node_title, "."), StrVectToStr(node_title_in, ".")))
        {
            name_correct = true;
            expected_type = node_type;
            break;
        }
        it++;
    }
    if (name_correct == false)
    {
        ret_failure = true;
        LogMessage(LOG_FAILURE, "Invalid node name: \"" + StrVectToStr(node_title_in, ".") + "\"");
    }
    else
    {
        if (type_correct == false)
        {
            ret_failure = true;
            LogMessage(LOG_FAILURE, "Invalid node type for node \"" + StrVectToStr(node_title_in, ".") + "\": type found: \"" + JsonNodeTypeToString(node_type_in) + "\", expected: \"" + JsonNodeTypeToString(expected_type) + "\"");
        }
    }

    return ret_failure;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool XJsonParser::GetJsonNodeValueType( std::string name, GType *node_value_type )
{
    // Get value inside node
    bool ret_failure = RET_SUCCESS;
    if (json_reader_is_value(m_json_reader))
    {
        JsonNode *node = json_reader_get_value(m_json_reader);
        *node_value_type = json_node_get_value_type(node);
        ret_failure |= CheckReaderError();
    }
    else
    {
        LogMessage(LOG_FAILURE, "GetJsonNodeValueType: Unable to find value in " + name);
        ret_failure = RET_FAILURE;
    }
    return ret_failure;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::string XJsonParser::JsonNodeValueTypeToString( GType node_value_type )
{
    std::string node_value_type_str   = "UNKNOWN";
    if      (node_value_type == G_TYPE_STRING)    node_value_type_str = "string";
    else if (node_value_type == G_TYPE_BOOLEAN)   node_value_type_str = "boolean";
    else if (node_value_type == G_TYPE_INT64)     node_value_type_str = "integer";
    else if (node_value_type == G_TYPE_DOUBLE)    node_value_type_str = "double";
    return node_value_type_str;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

