// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Xilinx, Inc
// Copyright (C) 2022-2024 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "Table2D.h"
#include "core/common/system.h"

// 3rd Party Library - Include Files
#include <algorithm>
#include <boost/format.hpp>

Table2D::Table2D(const std::vector<HeaderData>& headers) : m_inter_entry_padding(2)
{
  for (auto& header : headers)
      addHeader(header);
}

void
Table2D::addEntry(const std::vector<std::string>& entry)
{
  if (entry.size() < m_table.size())
    throw std::runtime_error(boost::str(boost::format("Table2D - Entry data is smaller than table. Entry size: %d Table Size: %d\n") % entry.size() % m_table.size()));
  else if (entry.size() > m_table.size())
    throw std::runtime_error(boost::str(boost::format("Table2D - Entry data is larger than table. Entry size: %d Table Size: %d\n") % entry.size() % m_table.size()));

  // Iterate through the entry data and the table adding the table entry elements in order
  for (size_t i = 0; i < entry.size(); ++i) {
    auto& column = m_table[i];
    column.max_element_size = std::max(column.max_element_size, entry[i].size());
    column.data.push_back(entry[i]);
  }
}

void
Table2D::appendToOutput(std::string& output, const std::string& prefix, const std::string& suffix, const ColumnData& column, const std::string& data) const
{
  // Format for table data
  boost::format fmt("%s|%s%s%s%s|");
  size_t left_blanks = 0;
  size_t right_blanks = 0;
  getBlankSizes(column, data.size(), left_blanks, right_blanks);
  output.append(boost::str(fmt % prefix % std::string(left_blanks, ' ') % data % std::string(right_blanks, ' ') % suffix));
}

std::string
Table2D::toString(const std::string& prefix) const
{
  // Iterate through each row and column of the table to format the output
  // Add one to account for the header row
  std::stringstream os;
  for (size_t row = 0; row < m_table[0].data.size() + 2; row++) {
    std::string output_line;
    for (size_t col = 0; col < m_table.size(); col++) {
      auto& column = m_table[col];

      // The first column must align with the user desires
      // All other columns should only use the previous lines space suffix
      const std::string column_prefix = (col == 0) ? prefix : "";

      // For the first row add the headers
      const auto space_suffix = std::string(m_inter_entry_padding, ' ');
      switch (row) {
        case 0:
          appendToOutput(output_line, column_prefix, space_suffix, column, column.header.name);
          break;
        case 1:
          appendToOutput(output_line, column_prefix, std::string(m_inter_entry_padding, '-'), column, std::string(column.max_element_size, '-'));
          break;
        default:
          appendToOutput(output_line, column_prefix, space_suffix, column, column.data[row - 2]);
          break;
      }
    }
    output_line.append("\n");
    os << output_line;
  }
  return os.str();
}

std::ostream&
Table2D::print(std::ostream& os) const
{
  os << boost::format("%s\n") % toString();
  return os;
}

void
Table2D::getBlankSizes(ColumnData col_data, size_t string_size, size_t& left_blanks, size_t& right_blanks) const
{
  const size_t required_buffer = col_data.max_element_size - string_size;
  switch (col_data.header.justification) {
    case Justification::right:
      left_blanks = required_buffer;
      right_blanks = 0;
      break;
    case Justification::left:
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

size_t 
Table2D::getTableCharacterLength() const
{
  size_t table_size = 0;
  for(const auto& col : m_table)
    table_size += col.max_element_size;

  // Account for the spaces added between the columns
  return table_size + (m_table.size() * m_inter_entry_padding);
}
