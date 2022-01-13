/**
 * Copyright (C) 2022 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */
// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "Table2D.h"

// 3rd Party Library - Include Files
#include <algorithm>
#include <boost/format.hpp>

Table2D::Table2D() : m_entry_count(0)
{
}

Table2D::Table2D(const std::vector<HeaderData>& headers_data) : m_entry_count(0)
{
    addHeaders(headers_data);
}

bool
Table2D::addHeader(const HeaderData& header_data)
{
    // Format the header into a table data struct
    ColumnData column;
    column.header_data = header_data;
    column.entry_data = std::vector<std::string>();
    // The only element in the new column is the header, so it defines the max length
    column.max_length = header_data.header.size();
    m_data.push_back(column);
    return true;
}

bool
Table2D::addHeaders(const std::vector<HeaderData>& headers_data)
{
    for (auto& header_data : headers_data) {
        if (!addHeader(header_data))
            return false;
    }
    return true;
}

bool
Table2D::addEntry(const std::vector<std::string>& entry_data)
{
    // Keep track of each new entry
    ++m_entry_count;

    if (entry_data.size() < m_data.size())
        std::cout << "Warning: Given entry data is smaller than table. Later values will be N/A.\n";
    else if (entry_data.size() > m_data.size())
        std::cout << "Warning: Given entry data is larger than table. Later values will not be appended.\n";

    // Determine the maximum number of elements to add from the entry
    const size_t vector_size = std::max(entry_data.size(), m_data.size());

    // Iterate through the entry data and the table adding the table enetry elements in order
    for (size_t data_index = 0; data_index < vector_size; ++data_index) {
        auto& table_entry = m_data[data_index];
        if (data_index < entry_data.size()) {
            const std::string& entry = entry_data[data_index];
            table_entry.entry_data.push_back(entry);
            table_entry.max_length = std::max(table_entry.max_length, entry.size());
        }
        else
            table_entry.entry_data.push_back("N/A");
    }
    return true;
}

bool
Table2D::addEntries(const std::vector<std::vector<std::string>>& entries_data)
{
    for (auto& entry_data : entries_data) {
        if(!addEntry(entry_data))
            return false;
    }
    return true;
}

std::ostream& 
Table2D::print(std::ostream& os)
{
    // Stores each row of the table
    std::vector<std::string> output;

    // Add an empty string for each entry plus a header row
    for (size_t i = 0; i < m_entry_count + 1; i++) {
        output.push_back("");
    }

    // Iterate through the table columns while adding each element to the appropriate row
    for (auto& table_column : m_data) {
        size_t left_blanks = 0;
        size_t right_blanks = 0;
        
        // Add header for current entry to the top row
        std::string& header = table_column.header_data.header;
        getBlankSizes(table_column, header.size(), left_blanks, right_blanks);
        output[0].append(boost::str(boost::format("%s%s%s  ") % std::string(left_blanks, ' ') % header % std::string(right_blanks, ' ')));

        // Iterate through each entry in the data column and append the data to the appropriate row
        std::vector<std::string>& data = table_column.entry_data;
        for (size_t data_index = 0; data_index < data.size(); ++data_index) {
            std::string& entry = data[data_index];
            getBlankSizes(table_column, entry.size(), left_blanks, right_blanks);
            // Plus one as the 0 element is the header row
            output[data_index + 1].append(boost::str(boost::format("%s%s%s  ") % std::string(left_blanks, ' ') % entry % std::string(right_blanks, ' ')));
        }
    }

    // Add a newline to each output row
    // Place row into the output stream
    for (auto& output_line : output) {
        output_line.append("\n");
        os << output_line;
    }

    return os;
}

void 
Table2D::getBlankSizes(ColumnData& table_data, size_t string_size, size_t& left_blanks, size_t& right_blanks)
{
    const size_t max_string_size = table_data.max_length;
    const size_t required_buffer = max_string_size - string_size;
    switch (table_data.header_data.justification) {
        case Justification::left:
            left_blanks = required_buffer;
            right_blanks = 0;
            break;
        case Justification::right:
            left_blanks = 0;
            right_blanks = required_buffer;
            break;
        case Justification::center:
            left_blanks = (required_buffer)/2;
            right_blanks = (required_buffer)/2;
            // If the number of required blanks is an odd number add a space to account for the division loss
            if (required_buffer % 2 != 0)
                ++left_blanks;
            break;
    }
}
