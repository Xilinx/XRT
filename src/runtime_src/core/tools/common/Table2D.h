// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Xilinx, Inc
// Copyright (C) 2022-2026 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __Table2D_h_
#define __Table2D_h_

// ------ I N C L U D E   F I L E S -------------------------------------------
// 3rd Party Library - Include Files
#include <vector>
#include <string>
#include <iostream>
#include <utility>

class Table2D {
 public:
    // The typesetting options for a column
    enum Justification {
        right = 0,
        center = 1,
        left = 2
    };

    // The header portion of a column
    typedef struct HeaderData {
        std::string name;
        Justification justification;
    } HeaderData;

    Table2D(const std::vector<HeaderData>& headers);

    /**
     * @brief Construct a table for stacked (multi-row) headers and entries.
     *
     * Each logical row of the table has one string per column. Use setStackedHeaders(),
     * addStackedEntry(), addStackedSeparator(), and stackedToString() for this layout.
     *
     * @param column_count Number of columns in the table
     * @param justification Column alignment for all columns
     */
    explicit Table2D(size_t column_count, Justification justification = Justification::left);

    /**
     * @brief Set multi-row column headers for a stacked table.
     *
     * Each inner vector is one header row and must contain exactly column_count() strings.
     * Column widths are updated from every non-empty cell across all header rows.
     *
     * @param header_rows Stacked header rows, one vector of cell values per row
     */
    void setStackedHeaders(const std::vector<std::vector<std::string>>& header_rows);

    /**
     * @brief Add a stacked entry (multiple table rows sharing the same column widths).
     *
     * Each inner vector is one row and must contain exactly column_count() strings.
     * Empty strings may be used for cells that are unused on that row.
     *
     * @param entry_rows Stacked data rows for this entry
     */
    void addStackedEntry(const std::vector<std::vector<std::string>>& entry_rows);

    /**
     * @brief Insert a separator line after the most recently added stacked entry.
     */
    void addStackedSeparator();

    /**
     * @brief Format a stacked table (headers, entries, and separators) as a string.
     *
     * @param prefix Leading whitespace before the first column delimiter on each row
     */
    std::string stackedToString(const std::string& prefix = "") const;

    /**
     * @return Number of columns in the table
     */
    size_t columnCount() const
    {
      return m_table.size();
    }

    /**
     * @return true if the stacked table has no entries (headers may still be set)
     */
    bool stackedEmpty() const
    {
      return m_stacked_blocks.empty();
    }

    /**
     * @brief Add an entry to the table. The entry must contain data for each header in the table.
     * 
     * @param entry_data A list of data elements that correspond the headers
     */
    void addEntry(const std::vector<std::string>& entry);

    std::string toString(const std::string& prefix = "") const;

    size_t getTableCharacterLength() const;

    bool empty() const
    {
        // Check if headers have been added
        if (m_table.empty())
            return true;

        // Check if anything other than headers have been added
        if (m_table[0].data.empty())
            return true;

        return false;
    }

 private:
    enum class StackedBlockKind {
        entry,
        separator
    };

    struct StackedBlock {
        StackedBlockKind kind;
        std::vector<std::vector<std::string>> rows;
    };

    typedef struct ColumnData {
        HeaderData header;
        std::vector<std::string> data;
        size_t max_element_size;
    } ColumnData;

    std::vector<ColumnData> m_table;
    size_t m_inter_entry_padding;
    bool m_stacked_mode = false;
    std::vector<std::vector<std::string>> m_stacked_header_rows;
    std::vector<StackedBlock> m_stacked_blocks;

    std::ostream& print(std::ostream& os) const;

    void getBlankSizes(ColumnData col_data, size_t string_size, size_t& left_blanks, size_t& right_blanks) const;
    void addHeader(const HeaderData& header);
    void appendToOutput(std::string& output, const std::string& prefix, const std::string& suffix, const ColumnData& column, const std::string& data) const;
    void updateStackedColumnWidth(size_t column_index, const std::string& data);
    void validateStackedRow(const std::vector<std::string>& row, const char* context) const;
    void formatDataRow(std::string& output_line, const std::string& prefix, const std::vector<std::string>& row) const;
    void formatSeparatorRow(std::string& output_line, const std::string& prefix) const;

 public:
    friend std::ostream&
    operator<<(std::ostream& os, const Table2D& table)
    {
        return table.print(os);
    };
};

#endif
