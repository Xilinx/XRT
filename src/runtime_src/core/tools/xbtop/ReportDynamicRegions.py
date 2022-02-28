#!/usr/bin/python3

#
# SPDX-License-Identifier: Apache-2.0
#
# Copyright (C) 2022 Xilinx, Inc
#

import json
import XBUtil

# found in PYTHONPATH
import pyxrt


class ReportDynamicRegions:

    def report_name(self):
        return "Dynamic Region"

    def update(self, dev):
        #get cu info
        cu_json = dev.get_info(pyxrt.xrt_info_device.dynamic_regions)
        cu_raw = json.loads(cu_json) #read into a dictionary

        self._df = cu_raw

    def _print_cu_info(self, term, lock, start_x, start_y):
        XBUtil.print_section_heading(term, lock, "Compute Usage", start_y)
        table_offset = 1
        if self._df is None:
            XBUtil.print_warning(term, lock, start_y + table_offset, "Data unavailable. Acceleration image not loaded")
            return table_offset + 1

        try:
            cus = self._df['dynamic_regions'][0]['compute_units']
        except:
            XBUtil.print_warning(term, lock, start_y + table_offset, "Data unavailable. Acceleration image not loaded")
            return table_offset + 1


        with lock:
            term.location(3, start_y+table_offset)
            print("Xclbin UUID: %s" % self._df['dynamic_regions'][0]['xclbin_uuid'])
            table_offset += 2

        header = [     "", "Name", "Base Address", "Usage", "Status", "Type"]
        format = ["right", "left",        "right", "right", "center", "center"]
        data = []

        cus = self._df['dynamic_regions'][0]['compute_units']
        for i in range(len(cus)):
            line = []
            line.append(str(i))
            line.append(cus[i]['name'])
            line.append(cus[i]['base_address'])
            line.append(cus[i]['usage'])
            line.append(cus[i]['status']['bit_mask'])
            line.append(cus[i]['type'])

            data.append(line)

        if len(data) != 0:
            table = XBUtil.Table(header, data, format)
            ascii_table = table.create_table()

            XBUtil.indented_print(term, lock, ascii_table, 3, start_y + table_offset)
            table_offset += len(ascii_table)

        return table_offset

    def print_report(self, term, lock, start_x, start_y):
        offset = 1 + self._print_cu_info(term, lock, start_x, start_y)
        return offset