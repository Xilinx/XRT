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

 private:
    typedef struct ColumnData {
        HeaderData header;
        std::vector<std::string> data;
        size_t max_element_size;
    } ColumnData;

    std::vector<ColumnData> m_table;

    std::ostream& print(std::ostream& os);

    void getBlankSizes(ColumnData col_data, size_t string_size, size_t& left_blanks, size_t& right_blanks);
    void addHeader(const HeaderData& header);
    void appendToOutput(std::string& output, const ColumnData& column, const std::string& data);

 public:
    friend std::ostream&
    operator<<(std::ostream& os, Table2D& table)
    {
        return table.print(os);
    };
};

#endif
