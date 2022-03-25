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

class ReportPower:
    def __init__(self, max_report_length):
        self.report_length = max_report_length

    def report_name(self):
        return "Power"

    def update(self, dev):
        #get power info
        electrical_json = dev.get_info(pyxrt.xrt_info_device.electrical)
        electrical_raw = json.loads(electrical_json) #read into a dictionary

        # check if xclbin is loaded
        if not electrical_raw:
            self._df.clear()
            self.page_count = 0
            return self.page_count

        # dict with all power entries
        power_status = {}
        power_status['Max Power'] = electrical_raw['power_consumption_max_watts']
        power_status['Power'] = electrical_raw['power_consumption_watts']
        power_status['Warning'] = electrical_raw['power_consumption_warning']

        self._df = power_status
        self.page_count = 1
        return self.page_count

    def print_report(self, term, lock, start_x, start_y, page):
        XBUtil.print_section_heading(term, lock, self.report_name(), start_y)
        offset = 1

        if len(self._df) == 0:
            print_warning(term, lock, start_y + offset, "Data unavailable. Acceleration image not loaded")
            return offset + 1

        if (self.page_count == 0):
            XBUtil.print_warning(term, lock, start_y + table_offset, "Data unavailable. Acceleration image not loaded")
            return table_offset + 1

        power_buf=[
            'Power     : %s Watts' % self._df['Power'],
            'Max Power : %s Watts' % self._df['Max Power'],
            'Warning   : %s' % self._df['Warning']
            ]

        XBUtil.indented_print(term, lock, power_buf, 3, start_y + offset)
        offset += len(power_buf)
        return offset
