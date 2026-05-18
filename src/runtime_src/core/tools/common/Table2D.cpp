// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Xilinx, Inc
// Copyright (C) 2022-2026 Advanced Micro Devices, Inc. All rights reserved.

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

Table2D::Table2D(size_t column_count, Justification justification)
  : m_inter_entry_padding(2)
  , m_stacked_mode(true)
{
  for (size_t i = 0; i < column_count; ++i) {
    HeaderData header;
    header.name = "";
    header.justification = justification;
    addHeader(header);
  }
}

void
Table2D::updateStackedColumnWidth(size_t column_index, const std::string& data)
{
  auto& column = m_table[column_index];
  column.max_element_size = std::max(column.max_element_size, data.size());
}

void
Table2D::validateStackedRow(const std::vector<std::string>& row, const char* context) const
{
  if (!m_stacked_mode)
    throw std::runtime_error(boost::str(boost::format("Table2D - %s requires a stacked table.\n") % context));

  if (row.size() < m_table.size())
    throw std::runtime_error(boost::str(boost::format("Table2D - %s row is smaller than table. Row size: %d Table Size: %d\n")
      % context % row.size() % m_table.size()));
  if (row.size() > m_table.size())
    throw std::runtime_error(boost::str(boost::format("Table2D - %s row is larger than table. Row size: %d Table Size: %d\n")
      % context % row.size() % m_table.size()));
}

void
Table2D::setStackedHeaders(const std::vector<std::vector<std::string>>& header_rows)
{
  if (!m_stacked_mode)
    throw std::runtime_error("Table2D - setStackedHeaders requires a stacked table.\n");

  if (header_rows.empty())
    throw std::runtime_error("Table2D - setStackedHeaders requires at least one header row.\n");

  m_stacked_header_rows.clear();
  for (const auto& row : header_rows) {
    validateStackedRow(row, "setStackedHeaders");
    m_stacked_header_rows.push_back(row);
    for (size_t col = 0; col < row.size(); ++col)
      updateStackedColumnWidth(col, row[col]);
  }
}

void
Table2D::addStackedEntry(const std::vector<std::vector<std::string>>& entry_rows)
{
  if (entry_rows.empty())
    throw std::runtime_error("Table2D - addStackedEntry requires at least one entry row.\n");

  for (const auto& row : entry_rows) {
    validateStackedRow(row, "addStackedEntry");
    for (size_t col = 0; col < row.size(); ++col)
      updateStackedColumnWidth(col, row[col]);
  }

  StackedBlock block;
  block.kind = StackedBlockKind::entry;
  block.rows = entry_rows;
  m_stacked_blocks.push_back(std::move(block));
}

void
Table2D::addStackedSeparator()
{
  if (!m_stacked_mode)
    throw std::runtime_error("Table2D - addStackedSeparator requires a stacked table.\n");

  StackedBlock block;
  block.kind = StackedBlockKind::separator;
  m_stacked_blocks.push_back(std::move(block));
}

void
Table2D::formatDataRow(std::string& output_line, const std::string& prefix, const std::vector<std::string>& row) const
{
  const auto space_suffix = std::string(m_inter_entry_padding, ' ');
  for (size_t col = 0; col < m_table.size(); ++col) {
    std::string column_prefix;
    if (col == 0)
      column_prefix = prefix + '|';
    appendToOutput(output_line, column_prefix, space_suffix, m_table[col], row[col]);
  }
  output_line.append("\n");
}

void
Table2D::formatSeparatorRow(std::string& output_line, const std::string& prefix) const
{
  const auto dash_suffix = std::string(m_inter_entry_padding, '-');
  for (size_t col = 0; col < m_table.size(); ++col) {
    std::string column_prefix;
    if (col == 0)
      column_prefix = prefix + '|';
    const auto& column = m_table[col];
    appendToOutput(output_line, column_prefix, dash_suffix, column, std::string(column.max_element_size, '-'));
  }
  output_line.append("\n");
}

std::string
Table2D::stackedToString(const std::string& prefix) const
{
  if (!m_stacked_mode)
    throw std::runtime_error("Table2D - stackedToString requires a stacked table.\n");

  std::stringstream os;
  for (const auto& header_row : m_stacked_header_rows) {
    std::string output_line;
    formatDataRow(output_line, prefix, header_row);
    os << output_line;
  }

  if (!m_stacked_header_rows.empty()) {
    std::string separator_line;
    formatSeparatorRow(separator_line, prefix);
    os << separator_line;
  }

  for (const auto& block : m_stacked_blocks) {
    switch (block.kind) {
      case StackedBlockKind::entry:
        for (const auto& row : block.rows) {
          std::string output_line;
          formatDataRow(output_line, prefix, row);
          os << output_line;
        }
        break;
      case StackedBlockKind::separator:
        {
          std::string separator_line;
          formatSeparatorRow(separator_line, prefix);
          os << separator_line;
        }
        break;
    }
  }

  return os.str();
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
  boost::format fmt("%s%s%s%s%s|");
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
      std::string column_prefix = "";
      if (col == 0)
         column_prefix = prefix + '|';

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
