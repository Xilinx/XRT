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

    def report_name(self):
        return "Power"

    def update(self, dev, report_length):
        self.report_length = report_length
        #get power info
        electrical_json = dev.get_info(pyxrt.xrt_info_device.electrical)
        electrical_raw = json.loads(electrical_json)  #read into a dictionary

        # check if xclbin is loaded
        if not electrical_raw:
            self._df = {}
            # If data is missing set the page count to 0!
            self.page_count = 0
            return self.page_count

        # dict with all power entries
        power_status = {}
        power_status['Max Power'] = electrical_raw['power_consumption_max_watts']
        power_status['Power'] = electrical_raw['power_consumption_watts']
        power_status['Warning'] = electrical_raw['power_consumption_warning']

        self._df = power_status
        # Round up the division to leave an extra page for the last batch of data
        page_count_temp = math.ceil(len(power_status) / self.report_length)
        # We must ensure that we always have at least one page
        self.page_count = max(page_count_temp, 1)
        return self.page_count

    def print_report(self, term, lock, start_x, start_y, page):
        XBUtil.print_section_heading(term, lock, self.report_name(), start_y)
        offset = 1

        if len(self._df) == 0 or self.page_count == 0:
            XBUtil.print_warning(term, lock, start_y + offset, "Data unavailable. Acceleration image not loaded")
            return offset + 1

        # Create the complete power buffer
        all_data = [
            'Power     : %s Watts' % self._df['Power'],
            'Max Power : %s Watts' % self._df['Max Power'],
            'Warning   : %s' % self._df['Warning']
            ]

        # Extract the data elements to be displayed on the requested page
        page_offset = page * self.report_length
        # The upper offset is bounded by the size of the full power buffer
        upper_page_offset = min(self.report_length + page_offset, len(all_data))
        data = all_data[page_offset:upper_page_offset]

        if (not data):
            XBUtil.print_warning(term, lock, start_y + offset, "Data unavailable")
            return offset + 1

        XBUtil.indented_print(term, lock, data, 3, start_y + offset)
        offset += len(data)
        return offset
