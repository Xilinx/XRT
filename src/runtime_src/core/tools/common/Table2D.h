// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Xilinx, Inc
// Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.

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
    typedef struct ColumnData {
        HeaderData header;
        std::vector<std::string> data;
        size_t max_element_size;
    } ColumnData;

    std::vector<ColumnData> m_table;
    size_t m_inter_entry_padding;

    std::ostream& print(std::ostream& os) const;

    void getBlankSizes(ColumnData col_data, size_t string_size, size_t& left_blanks, size_t& right_blanks) const;
    void addHeader(const HeaderData& header);
    void appendToOutput(std::string& output, const std::string& prefix, const std::string& suffix, const ColumnData& column, const std::string& data) const;

 public:
    friend std::ostream&
    operator<<(std::ostream& os, const Table2D& table)
    {
        return table.print(os);
    };
};

#endif
