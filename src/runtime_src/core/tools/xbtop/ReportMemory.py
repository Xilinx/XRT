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
    def report_name(self):
        return "Memory"

    def update(self, dev):
        memory_json = dev.get_info(pyxrt.xrt_info_device.memory)
        memory_raw = json.loads(memory_json) #read into a dictionary

        # check if xclbin is loaded
        if not memory_raw:
            self._df = None

        self._df = memory_raw

    def _print_memory_usage(self, term, lock, start_x, start_y):
        XBUtil.print_section_heading(term, lock, "Device Memory Usage", start_y)
        table_offset = 1

        if self._df == None:
            XBUtil.print_warning(term, lock, start_y + table_offset, "Data unavailable. Acceleration image not loaded")
            return table_offset + 1

        try:
            memories = self._df['board']['memory']['memories']
        except:
            XBUtil.print_warning(term, lock, start_y + table_offset, "Data unavailable. Acceleration image not loaded")
            return table_offset + 1

        mem_usage = []
        for memory in memories:
            size = int(memory['range_bytes'], 16)
            if size == 0 or memory['enabled'] == "false":
                continue
            name = memory['tag']
            usage = int(memory['extended_info']['usage']['allocated_bytes'], 0)
            mem_usage.append("%-16s %s" % (name, XBUtil.progress_bar(usage * 100 / size)))

        XBUtil.indented_print(term, lock, mem_usage, 3, start_y + table_offset)
        table_offset += len(mem_usage)
        return table_offset

    def _print_mem_topology(self, term, lock, start_x, start_y):
        XBUtil.print_section_heading(term, lock, "Memory Topology", start_y)
        table_offset = 1

        if self._df == None:
            print_warning(term, lock, start_y + table_offset, "Data unavailable. Acceleration image not loaded")
            return table_offset + 1

        try:
            memories = self._df['board']['memory']['memories']
        except:
            XBUtil.print_warning(term, lock, start_y + table_offset, "Data unavailable. Acceleration image not loaded")
            return table_offset + 1

        header = [     "",  "Tag", "Type", "Temp (C)",  "Size", "Mem Usage", "BO Count"]
        format = ["right", "left", "left",    "right", "right",     "right",    "right"]
        data = []

        memories = self._df['board']['memory']['memories']
        for i in range(len(memories)):
            line = []
            line.append(str(i))
            line.append(memories[i]['tag'])
            line.append(memories[i]['type'])
            line.append(memories[i]['extended_info']['temperature_C'] if 'temperature_C' in memories[i]['extended_info'] else "--")
            line.append(XBUtil.convert_size(int(memories[i]['range_bytes'],16)))
            line.append(XBUtil.convert_size(int(memories[i]['extended_info']['usage']['buffer_objects_count'],0)))
            line.append(memories[i]['extended_info']['usage']['buffer_objects_count'])
            data.append(line)

        table = XBUtil.Table(header, data, format)
        ascii_table = table.create_table()

        XBUtil.indented_print(term, lock, ascii_table, 3, start_y + table_offset)
        table_offset += len(ascii_table)
        return table_offset

    def _print_dma_transfer_matrics(self, term, lock, start_x, start_y):
        XBUtil.print_section_heading(term, lock, "DMA Transfer Matrics", start_y)
        table_offset = 1

        if self._df == None:
            print_warning(term, lock, start_y + table_offset, "Data unavailable. Acceleration image not loaded")
            return table_offset + 1

        try:
            dma_metrics = self._df['board']['direct_memory_accesses']['metrics']
        except:
            XBUtil.print_warning(term, lock, start_y + table_offset, "Data unavailable. Acceleration image not loaded")
            return table_offset + 1


        header = [     "", "Channel", "Host-to-Card", "Card-to-Host"]
        format = ["right",   "right",        "right",        "right"]
        data = []

        dma_metrics = self._df['board']['direct_memory_accesses']['metrics']
        for i in range(len(dma_metrics)):
            line = []
            line.append(str(i))
            line.append(dma_metrics[i]['channel_id'])
            line.append(XBUtil.convert_size(int(dma_metrics[i]['host_to_card_bytes'], 16)))
            line.append(XBUtil.convert_size(int(dma_metrics[i]['card_to_host_bytes'], 16)))
            data.append(line)

        table = XBUtil.Table(header, data, format)
        ascii_table = table.create_table()

        XBUtil.indented_print(term, lock, ascii_table, 3, start_y + table_offset)
        table_offset += len(ascii_table)
        return table_offset

    def print_report(self, term, lock, start_x, start_y):
        offset = 1 + self._print_memory_usage(term, lock, start_x, start_y)
        offset += 1 + self._print_mem_topology(term, lock, start_x, start_y + offset)
        offset += 1 + self._print_dma_transfer_matrics(term, lock, start_x, start_y + offset)
        return offset

