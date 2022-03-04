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
#include "core/common/system.h"

// 3rd Party Library - Include Files
#include <algorithm>
#include <boost/format.hpp>

Table2D::Table2D(const std::vector<HeaderData>& headers)
{
    for (auto& header : headers)
        addHeader(header);
}

void
Table2D::addEntry(const std::vector<std::string>& entry)
{
    if (entry.size() < m_table.size())
        throw std::runtime_error("Table2D - Entry data is smaller than table.\n");
    else if (entry.size() > m_table.size())
        throw std::runtime_error("Table2D - Entry data is larger than table.\n");

    // Iterate through the entry data and the table adding the table entry elements in order
    for (size_t i = 0; i < entry.size(); ++i) {
        auto& column = m_table[i];
        column.max_element_size = std::max(column.max_element_size, entry[i].size());
        column.data.push_back(entry[i]);
    }
}

void
Table2D::appendToOutput(std::string& output, const ColumnData& column, const std::string& data)
{
    // Format for table data
    boost::format fmt("%s%s%s  ");
    size_t left_blanks = 0;
    size_t right_blanks = 0;
    getBlankSizes(column, data.size(), left_blanks, right_blanks);
    output.append(boost::str(fmt % std::string(left_blanks, ' ') % data % std::string(right_blanks, ' ')));
}

std::ostream&
Table2D::print(std::ostream& os)
{
    // Iterate through each row and column of the table to format the output
    // Add one to account for the header row
    for(size_t row = 0; row < m_table[0].data.size() + 1; row++) {
        std::string output_line;
        for (size_t col = 0; col < m_table.size(); col++) {
            auto& column = m_table[col];

            // For the first row add the headers
            if (row == 0)
                appendToOutput(output_line, column, column.header.name);
            // For all other output lines add the entry associated with the current row/col index
            else
                // Minus 1 to account for the first row being the headers
                appendToOutput(output_line, column, column.data[row - 1]);
        }
        output_line.append("\n");
        os << output_line;
    }
    return os;
}

void
Table2D::getBlankSizes(ColumnData col_data, size_t string_size, size_t& left_blanks, size_t& right_blanks)
{
    const size_t required_buffer = col_data.max_element_size - string_size;
    switch (col_data.header.justification) {
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

void
Table2D::addHeader(const HeaderData& header)
{
    // Format the header into a table data struct
    ColumnData column;
    column.header = header;
    column.max_element_size = header.name.size();
    column.data = std::vector<std::string>();
    m_table.push_back(column);
}
