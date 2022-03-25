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
    def __init__(self, max_report_length):
        self.report_length = max_report_length

    def report_name(self):
        return "Dynamic Region"

    def update(self, dev):
        #get cu info
        cu_json = dev.get_info(pyxrt.xrt_info_device.dynamic_regions)
        cu_raw = json.loads(cu_json) #read into a dictionary

        page_count = math.ceil(len(cu_raw) / self.report_length)
        self.dma_page_count = page_count if page_count > 0 else 1

        self._df = cu_raw
        return self.dma_page_count

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

        for i in range(self.report_length):
            line = []
            index = i + (page * self.report_length)
            if(index < len(cus)):
                line.append(str(index))
                cus_element = cus[index]
                line.append(cus_element['name'])
                line.append(cus_element['base_address'])
                line.append(cus_element['usage'])
                line.append(cus_element['status']['bit_mask'])
                line.append(cus_element['type'])
                data.append(line)

        if len(data) != 0:
            table = XBUtil.Table(header, data, format)
            ascii_table = table.create_table()

            XBUtil.indented_print(term, lock, ascii_table, 3, start_y + table_offset)
            table_offset += len(ascii_table)

        return table_offset

    def print_report(self, term, lock, start_x, start_y, page):
        offset = 1 + self._print_cu_info(term, lock, start_x, start_y, page)
        return offset