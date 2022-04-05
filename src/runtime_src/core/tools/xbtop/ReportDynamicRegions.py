#!/usr/bin/python3

#
# SPDX-License-Identifier: Apache-2.0
#
# Copyright (C) 2022 Xilinx, Inc
#

import json
import math
import XBUtil

# found in PYTHONPATH
import pyxrt


class ReportDynamicRegions:
    def report_name(self):
        return "Dynamic Region"

    def update(self, dev, report_length):
        self.report_length = report_length
        #get cu info
        cu_json = dev.get_info(pyxrt.xrt_info_device.dynamic_regions)
        cu_raw = json.loads(cu_json) #read into a dictionary

        # Round up the division to leave an extra page for the last batch of data
        page_count_temp = math.ceil(len(cu_raw) / self.report_length)
        # We must ensure that we always have at least one page
        self.page_count = max(page_count_temp, 1)

        self._df = cu_raw
        return self.page_count

    def _print_cu_info(self, term, lock, start_x, start_y, page):
        XBUtil.print_section_heading(term, lock, "Compute Usage", start_y)
        table_offset = 1
        if self._df is None:
            XBUtil.print_warning(term, lock, start_y + table_offset, "Data unavailable. Acceleration image not loaded")
            return table_offset + 1

        with lock:
            term.location(3, start_y+table_offset)
            print("Xclbin UUID: %s" % self._df['dynamic_regions'][0]['xclbin_uuid'])
            table_offset += 2

        header = [     "", "Name", "Base Address", "Usage", "Status", "Type"]
        format = ["right", "left",        "right", "right", "center", "center"]
        data = []

        cus = []
        try:
            cus = self._df['dynamic_regions'][0]['compute_units']
        except:
            XBUtil.print_warning(term, lock, start_y + table_offset, "Data unavailable. Acceleration image not loaded")
            return table_offset + 1

        # Each page should display however many items a report page can hold
        for i in range(self.report_length):
            line = []
            # The current element to be parsed depends on what page has been requested
            index = i + (page * self.report_length)
            # Ensure that our index does not exceed the input data size. This may happen on the last page
            if(index < len(cus)):
                line.append(str(index))
                cus_element = cus[index]
                line.append(cus_element['name'])
                line.append(cus_element['base_address'])
                line.append(cus_element['usage'])
                line.append(cus_element['status']['bit_mask'])
                line.append(cus_element['type'])
                data.append(line)
            # If the index exceeds the input data size leave the for loop as everything is populated on the
            # last page
            else:
                break

        if len(data) != 0:
            table = XBUtil.Table(header, data, format)
            ascii_table = table.create_table()

            XBUtil.indented_print(term, lock, ascii_table, 3, start_y + table_offset)
            table_offset += len(ascii_table)

        return table_offset

    def print_report(self, term, lock, start_x, start_y, page):
        offset = 1 + self._print_cu_info(term, lock, start_x, start_y, page)
        return offset
