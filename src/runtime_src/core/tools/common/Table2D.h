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
        std::string header;
        Justification justification;
    } HeaderData;

    Table2D();
    Table2D(const std::vector<HeaderData>& headers_data);

    /**
     * @brief Add a new header into the table. If entries already exist the column will have empty entries.
     * 
     * @param header_data 
     * @return true 
     * @return false 
     */
    bool addHeader(const HeaderData& header_data);
    bool addHeaders(const std::vector<HeaderData>& headers_data);

    /**
     * @brief Add an entry to the table. The entry should contain data for each existing header in the table.
     * 
     * @param entry_data 
     * @return true 
     * @return false 
     */
    bool addEntry(const std::vector<std::string>& entry_data);
    bool addEntries(const std::vector<std::vector<std::string>>& entries_data);

 private:
    typedef struct ColumnData {
        HeaderData header_data;
        std::vector<std::string> entry_data;
        size_t max_length;
    } ColumnData;

    size_t m_entry_count;

    std::vector<ColumnData> m_data;

    std::ostream& print(std::ostream& os);

    void getBlankSizes(ColumnData& table_data, size_t string_size, size_t& left_blanks, size_t& right_blanks);

 public:
    friend std::ostream&
    operator<<(std::ostream& os, Table2D& table)
    {
        return table.print(os);
    };
};