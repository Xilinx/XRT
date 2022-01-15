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

const std::string no_data_string = "N/A";

Table2D::Table2D() : m_entry_count(0)
{
}

Table2D::Table2D(const std::vector<HeaderData>& headers) : m_entry_count(0)
{
    addHeaders(headers);
}

void
Table2D::addHeader(const HeaderData& header)
{
    // Format the header into a table data struct
    ColumnData column;
    column.header = header;
    // Populate the header with the appropriate amount of N/A
    column.entry_slices = std::vector<std::string>();
    for (size_t entry_count = 0; entry_count < m_entry_count; entry_count++)
        column.entry_slices.push_back(no_data_string);
    m_table.push_back(column);
}

void
Table2D::addHeaders(const std::vector<HeaderData>& headers)
{
    for (auto& header : headers)
        addHeader(header);
}

bool
Table2D::removeHeader(const std::string& header)
{
    // Search through each clumn looking for a matching header
    for (size_t column_index = 0; column_index < m_table.size(); ++column_index) {
        // If a matching header is found remove the entire column
        if (m_table[column_index].header.name.compare(header) == 0) {
            m_table.erase(m_table.begin() + column_index);
            return true;
        }
    }
    // Return false when no match is found
    return false;
}

size_t
Table2D::addEntry(const std::vector<std::string>& entry)
{
    // Keep track of each new entry
    ++m_entry_count;

    if (entry.size() < m_table.size())
        std::cout << "Warning: Given entry data is smaller than table. Later values will be " << no_data_string << "\n";
    else if (entry.size() > m_table.size())
        std::cout << "Warning: Given entry data is larger than table. Later values will not be appended.\n";

    // Determine the maximum number of elements to add from the entry
    const size_t vector_size = std::max(entry.size(), m_table.size());

    // Iterate through the entry data and the table adding the table entry elements in order
    for (size_t i = 0; i < vector_size; ++i) {
        auto& column = m_table[i];
        if (i < entry.size()) {
            column.entry_slices.push_back(entry[i]);
        }
        else
            column.entry_slices.push_back(no_data_string);
    }

    // Return the actual index of the newly added entry
    return m_entry_count - 1;
}

std::vector<size_t>
Table2D::addEntries(const std::vector<std::vector<std::string>>& entries)
{
    std::vector<size_t> entries_indicies;
    for (auto& entry : entries)
        entries_indicies.push_back(addEntry(entry));
    return entries_indicies;
}

void
Table2D::removeEntry(size_t entry_index)
{
    // Verify the index is valid
    if (entry_index >= m_entry_count)
        throw xrt_core::error(boost::str(boost::format(
            "ERROR: Table2D - Given index: %d is larger than data table: %d") % entry_index % m_entry_count));
    // Remove appropriate row element from each column
    for (auto& column : m_table)
        column.entry_slices.erase(column.entry_slices.begin() + entry_index);
}

std::ostream&
Table2D::print(std::ostream& os)
{
    // Stores each row of the table
    std::vector<std::string> output;

    // Add an empty string for each entry plus a header row
    for (size_t i = 0; i < m_entry_count + 1; i++)
        output.push_back("");

    // Iterate through the table columns while adding each element to the appropriate row
    for (auto& column : m_table) {
        size_t left_blanks = 0;
        size_t right_blanks = 0;

        auto& header = column.header;

        // Find the largest string in the column and use it for justification
        size_t max_string_size = header.name.size();
        for(auto& entry : column.entry_slices)
            max_string_size = std::max(entry.size(), max_string_size);

        // Add header for current entry to the top row
        getBlankSizes(max_string_size, header.justification, header.name.size(), left_blanks, right_blanks);
        output[0].append(boost::str(boost::format("%s%s%s  ") % std::string(left_blanks, ' ') % header.name % std::string(right_blanks, ' ')));

        // Iterate through each entry in the data column and append the data to the appropriate row
        auto& entry_slices = column.entry_slices;
        for (size_t i = 0; i < entry_slices.size(); ++i) {
            auto& entry = entry_slices[i];
            getBlankSizes(max_string_size, header.justification, entry.size(), left_blanks, right_blanks);
            // Plus one as the 0 element is the header row
            output[i + 1].append(boost::str(boost::format("%s%s%s  ") % std::string(left_blanks, ' ') % entry % std::string(right_blanks, ' ')));
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
Table2D::getBlankSizes(size_t max_string_size, Justification justification, size_t string_size, size_t& left_blanks, size_t& right_blanks)
{
    const size_t required_buffer = max_string_size - string_size;
    switch (justification) {
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
