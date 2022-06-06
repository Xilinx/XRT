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


class ReportMemory:
    # Python value used as second parameter for int() to convert hexadecimal strings into ints
    hexadecimal_converion = 16

    def report_name(self):
        return "Memory"

    def update(self, dev, report_length):
        self.report_length = report_length
        memory_json = dev.get_info(pyxrt.xrt_info_device.memory)
        memory_raw = json.loads(memory_json) #read into a dictionary

        # check if xclbin is loaded
        if not memory_raw:
            self._df = None

        self._df = memory_raw

        # Generate the number of pages used for the report
        try:
            # Round up the division to leave an extra page for the last batch of data
            page_count = math.ceil(len(self._df['board']['memory']['memories']) / report_length)
            # We must ensure that we always have at least one page
            self.topology_page_count = max(page_count, 1)
        except:
            self.topology_page_count = 0

        try:
            page_count = math.ceil(len(self._df['board']['direct_memory_accesses']['metrics']) / report_length)
            self.dma_page_count = max(page_count, 1)
        except:
            self.dma_page_count = 0

        self.page_count = max(self.topology_page_count + self.dma_page_count, 1)
        return self.page_count

    def _print_memory_usage(self, term, lock, start_x, start_y, page):
        XBUtil.print_section_heading(term, lock, "Device Memory Usage", start_y)
        offset = 1

        if self._df == None:
            XBUtil.print_warning(term, lock, start_y + offset, "Data unavailable. Acceleration image not loaded")
            return offset + 1

        data = []
        memories = []
        try:
            memories = self._df['board']['memory']['memories']
        except:
            XBUtil.print_warning(term, lock, start_y + offset, "Data unavailable. Acceleration image not loaded")
            return offset + 1

        for i in range(self.report_length):
            # The current element to be parsed depends on what page has been requested
            index = i + (page * self.report_length)
            # Ensure that our index does not exceed the input data size. This may happen on the last page
            if (index < len(memories)):
                memory = memories[index]
                size = int(memory['range_bytes'], self.hexadecimal_converion)
                if size == 0 or memory['enabled'] == "false":
                    continue
                name = memory['tag']
                usage = int(memory['extended_info']['usage']['allocated_bytes'], 0)
                data.append("%-16s %s" % (name, XBUtil.progress_bar(usage * 100 / size)))
            # If the index exceeds the input data size leave the for loop as everything is populated on the
            # last page
            else:
                break

        if (not data):
            XBUtil.print_warning(term, lock, start_y + offset, "Data unavailable")
            return offset + 1

        XBUtil.indented_print(term, lock, data, 3, start_y + offset)
        offset += len(data)
        return offset

    def _print_mem_topology(self, term, lock, start_x, start_y, page):
        XBUtil.print_section_heading(term, lock, "Memory Topology", start_y)
        offset = 1

        if self._df == None:
            print_warning(term, lock, start_y + offset, "Data unavailable. Acceleration image not loaded")
            return offset + 1

        header = [     "",  "Tag", "Type", "Temp (C)",  "Size", "Mem Usage", "Utilization", "BO Count"]
        format = ["right", "left", "left",    "right", "right",     "right",       "right",    "right"]
        data = []

        memories = []
        try:
            memories = self._df['board']['memory']['memories']
        except:
            XBUtil.print_warning(term, lock, start_y + offset, "Data unavailable. Acceleration image not loaded")
            return offset + 1

        for i in range(self.report_length):
            line = []
            # The current data element to be parsed depends on what page has been requested
            index = i + (page * self.report_length)
            # Ensure that our index does not exceed the data size. This may happen on the last page
            if (index < len(memories)):
                line.append(str(index))
                memory = memories[index]
                line.append(memory['tag'])
                line.append(memory['type'])
                line.append(memory['extended_info']['temperature_C'] if 'temperature_C' in memory['extended_info'] else "--")
                line.append(XBUtil.convert_size(int(memory['range_bytes'], self.hexadecimal_converion)))
                line.append(XBUtil.convert_size(int(memory['extended_info']['usage']['allocated_bytes'],0)))
                
                size = int(memory['range_bytes'], self.hexadecimal_converion)
                if size == 0 or memory['enabled'] == "false":
                    line.append("--  ")
                else:
                    usage = int(memory['extended_info']['usage']['allocated_bytes'], 0)
                    line.append(XBUtil.get_percentage(usage, size))

                
                line.append(memory['extended_info']['usage']['buffer_objects_count'])
                data.append(line)
            # If the index exceeds the input data size leave the for loop as everything is populated on the
            # last page
            else:
                break

        if (not data):
            XBUtil.print_warning(term, lock, start_y + offset, "Data unavailable")
            return offset + 1

        table = XBUtil.Table(header, data, format)
        ascii_table = table.create_table()

        XBUtil.indented_print(term, lock, ascii_table, 3, start_y + offset)
        offset += len(ascii_table)
        return offset

    def _print_dma_transfer_metrics(self, term, lock, start_x, start_y, page):
        XBUtil.print_section_heading(term, lock, "DMA Transfer Metrics", start_y)
        offset = 1

        if self._df == None:
            print_warning(term, lock, start_y + offset, "Data unavailable. Acceleration image not loaded")
            return offset + 1

        header = [     "", "Channel", "Host-to-Card", "Card-to-Host"]
        format = ["right",   "right",        "right",        "right"]
        data = []

        dma_metrics = []
        try:
            dma_metrics = self._df['board']['direct_memory_accesses']['metrics']
        except:
            XBUtil.print_warning(term, lock, start_y + offset, "Data unavailable. Acceleration image not loaded")
            return offset + 1

        for i in range(self.report_length):
            line = []
            # The current element to be parsed depends on what page has been requested
            index = i + (page * self.report_length)
            # Ensure that our index does not exceed the input data size. This may happen on the last page
            if (index < len(dma_metrics)):
                dma_metric = dma_metrics[index]
                line.append(str(i))
                line.append(dma_metric['channel_id'])
                line.append(XBUtil.convert_size(int(dma_metric['host_to_card_bytes'], self.hexadecimal_converion)))
                line.append(XBUtil.convert_size(int(dma_metric['card_to_host_bytes'], self.hexadecimal_converion)))
                data.append(line)
            # If the index exceeds the input data size leave the for loop as everything is populated on the
            # last page
            else:
                break

        if (not data):
            XBUtil.print_warning(term, lock, start_y + offset, "Data unavailable")
            return offset + 1

        table = XBUtil.Table(header, data, format)
        ascii_table = table.create_table()

        XBUtil.indented_print(term, lock, ascii_table, 3, start_y + offset)
        offset += len(ascii_table)
        return offset

    def print_report(self, term, lock, start_x, start_y, page):
        offset = 0
        # The pages in order are:
        # 1. Memory Topology
        # 2. DMA tranfer data
        # The page values in the individual reports range from 0 - X and 
        # the input page refers to the total number of pages in the memory report 
        # Ex. (0 - (topology_page_count + dma_page_count)).
        # So the previous page count must be subtracted before passing the 
        # page parameter into the appropriate print function
        if (page < self.topology_page_count):
            offset = 1 + self._print_mem_topology(term, lock, start_x, start_y + offset, page)
        elif (page < self.page_count):
            offset = 1 + self._print_dma_transfer_metrics(term, lock, start_x, start_y + offset, page - self.topology_page_count)
        else:
            XBUtil.print_warning(term, lock, start_y + offset, "Something went wrong! Please report this issue and its conditions.")
        return offset

