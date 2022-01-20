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

#ifndef __SubCmd_h_
#define __SubCmd_h_

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

    Table2D();
    Table2D(const std::vector<HeaderData>& headers);

    /**
     * @brief Add a new header into the table. If entries already exist the column will have empty entries.
     * 
     * @param header The header to be added into the table
     */
    void addHeader(const HeaderData& header);
    void addHeaders(const std::vector<HeaderData>& headers);

    /**
     * @brief Remove the specified header from the table if it exists
     * 
     * This will remove the column and all data associated with that column from the table.
     * 
     * @param header The ehader name to remove
     * @return true When the header is found and removed
     * @return false If the header was not found in the table
     */
    bool removeHeader(const std::string& header);

    /**
     * @brief Add an entry to the table. The entry should contain data for each existing header in the table.
     * 
     * @param entry_data A list of data elements that correspond the headers
     * @return size_t The index the entry data was placed into. This value will change if entries are removed
     */
    size_t addEntry(const std::vector<std::string>& entry);
    std::vector<size_t> addEntries(const std::vector<std::vector<std::string>>& entries);

    /**
     * @brief Remove an entry from at a specified index
     * 
     * @param entry_index The index whose entry should be removed
     */
    void removeEntry(size_t entry_index);

 private:
    typedef struct ColumnData {
        HeaderData header;
        std::vector<std::string> entry_slices;
    } ColumnData;

    size_t m_entry_count;

    std::vector<ColumnData> m_table;

    std::ostream& print(std::ostream& os);

    void getBlankSizes(size_t max_string_size, Justification justification, size_t string_size, size_t& left_blanks, size_t& right_blanks);

 public:
    friend std::ostream&
    operator<<(std::ostream& os, Table2D& table)
    {
        return table.print(os);
    };
};

#endif
