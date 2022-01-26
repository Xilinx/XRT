#!/usr/bin/python3

#
# SPDX-License-Identifier: Apache-2.0
#
# Copyright (C) 2022 Xilinx, Inc
#

# Note: Only standard and local packages are permitted  in this script.

# Python Standard Packages
import argparse
import threading
import time
import json
from enum import IntEnum
from shutil import get_terminal_size

# XRT Package (Discovered in PYTHONPATH)
import pyxrt

# Local Packages
import XBUtil

# Global Variables
g_refresh_rate = 0
g_refresh_counter = 0
g_report_number = 0

# Initialize the avaible reports
# Note: Once the report is added to the g_report array, it will automaticaly
#       be supported.
from ReportPower import ReportPower
from ReportMemory import ReportMemory
from ReportDynamicRegions import ReportDynamicRegions

g_reports = []
g_reports.append(ReportMemory())
g_reports.append(ReportDynamicRegions())
g_reports.append(ReportPower())


# Running clock thread (upper left corner)
def running_clock(term, lock):
    while True:
        with lock:
            term.location(0, 2)
            print(time.asctime(time.localtime(time.time())))

        time.sleep(0.5)

# Running counter for each of the reports (upper right corner)
def running_counter(term, lock, len_x):
    global g_refresh_rate
    global g_refresh_counter

    while True:
        with lock:
            # Add left 'padding' when counts and rates drop
            print_count = "     Refresh Count: %s (Rate: %s s)" % (g_refresh_counter, g_refresh_rate)
            term.location(len_x - len(print_count) - 2, 2)
            print(print_count)

        time.sleep(0.5)

# Key table
def print_footer(term, lock, y_len):
    footer_buf = [
        XBUtil.fg.yellow + "'n'/'p' - Next/Previous Report" + XBUtil.fg.reset,
        XBUtil.fg.yellow + "    'q' - Quit" + XBUtil.fg.reset
    ]
    XBUtil.indented_print(term, lock, footer_buf, 5, y_len - len(footer_buf))

# Common platform information
def bdf_header(term, lock, dev):
    platform_json = dev.get_info(pyxrt.xrt_info_device.platform)
    platform_raw = json.loads(platform_json)
    platform = platform_raw['platforms'][0]['static_region']

    header_buf = [
        '  Shell : %s' % platform['vbnv'],
        '  UUID  : %s' % platform['logic_uuid'],
        '  BDF   : %s' % dev.get_info(pyxrt.xrt_info_device.bdf),
    ]
    longest_string = max(header_buf, key=len)
    header_buf.insert(0, '-' * (len(longest_string)))
    header_buf.append('-' * (len(longest_string)))
    XBUtil.indented_print(term, lock, header_buf, 0, 4)

# Prints both the BDF table and the current report being displayed
def print_header(term, lock, dev, report_name, len_x):
    # Add padding to overwrite previous report
    padding_width = 30
    report_name = XBUtil.pad_string(report_name, padding_width, "center")

    with lock:
        center_align = int((len_x - len(report_name)) / 2)

        term.location(center_align, 2)
        print(XBUtil.fg.blue + XBUtil.fx.bold + XBUtil.fx.italic + report_name + XBUtil.fx.reset)

    bdf_header(term, lock, dev)

# Running thread used to driver the reports.
def running_reports(term, lock, dev, x_len):
    global g_report_number

    report_start_row = 10
    num_lines_printed = 0
    current_report = -1
    while True:
        global g_refresh_counter
        global g_reports
        global g_refresh_rate

        g_refresh_counter += 1
        # Determine if our report has changed
        if current_report != g_report_number:
            g_refresh_counter = 0

            # Clear the previous reports
            XBUtil.clear_rows(term, lock, report_start_row, num_lines_printed)

            # Point to the next report
            if g_report_number < 0:
                g_report_number = len(g_reports) - 1
            if g_report_number >= len(g_reports):
                g_report_number = 0
            current_report = g_report_number

            # Update the report header on which report that is currently being displayed
            report_name = g_reports[current_report].report_name()
            report_header = "%s (%d/%d)" % (report_name, current_report + 1, len(g_reports))
            print_header(term, lock, dev, report_header, x_len)

        g_reports[current_report].update(dev)
        # Clear the previous reports
        XBUtil.clear_rows(term, lock, report_start_row, num_lines_printed)
        num_lines_printed = g_reports[current_report].print_report(term, lock, 0, report_start_row)

        # Wait for either for the refresh time to expire or for a new report
        counter = 0
        while counter < (g_refresh_rate * 4):  # Note: The sleep time is 0.25 seconds, hence x4.
            counter += 1
            if current_report != g_report_number:
                break
            time.sleep(0.25)    # Check 4 times a second

# Parse the user's input
def options_parser():
    parser = argparse.ArgumentParser(description="xbtop: Process Viewer & Monitor")
    optional = parser._action_groups.pop()
    required = parser.add_argument_group('required arguments')
    required.add_argument("-d", "--device", dest = "bdf", help="The device bdf", required = True)
    optional.add_argument("-r", "--refresh_rate", dest = "s", default = 1, help="Refresh rate in seconds")
    parser._action_groups.append(optional)
    return parser.parse_args()

# Main processing thread
def main():
    global g_refresh_rate
    global g_report_number

    # Get and validate the options
    opt = options_parser()

    g_refresh_rate = float(opt.s)

    if g_refresh_rate < 1:
        raise RuntimeError("Please specify a refresh rate greater than 1 second")

    if g_refresh_rate > 60:
        raise RuntimeError("Please specify a refresh rate less than 60 seconds")

    dev = pyxrt.device(opt.bdf)

    # Check the terminal size
    term_size = get_terminal_size()
    x_len = term_size.columns
    y_len = term_size.lines
    MIN_X = 100
    MIN_Y = 44
    if x_len < MIN_X or y_len < MIN_Y:
        raise RuntimeError("Please resize the terminal window.  The current size %dx%d is smaller then the required size of %dx%d" % (x_len, y_len, MIN_X, MIN_Y))

    with XBUtil.Terminal() as term:
        term.hide_cursor(True)

        # creating a lock
        lock = threading.Lock()

        # Print the key
        print_footer(term, lock, y_len)

        # Running clock
        t1 = threading.Thread(target=running_clock, args=(term, lock))
        t1.daemon = True
        t1.start()

        # Running counter
        t1 = threading.Thread(target=running_counter, args=(term, lock, x_len))
        t1.daemon = True
        t1.start()

        # Running reports
        t2 = threading.Thread(target=running_reports, args=(term, lock, dev, x_len))
        t2.daemon = True
        t2.start()

        # Main thread that consumes the keys pressed by the user.
        while True:
            key = XBUtil.get_char()
            if key in ['q', 'Q']:
                break

            if key in ['n', 'N']:
                g_report_number += 1

            if key in ['p', 'P']:
                g_report_number -= 1

            if key in ['+']:             # Hidden option
                g_refresh_rate += 1
                if g_refresh_rate > 60:
                    g_refresh_rate = 60

            if key in ['-']:             # Hidden option
                g_refresh_rate -= 1
                if g_refresh_rate < 1:
                    g_refresh_rate = 1


if __name__ == '__main__':
    try:
        main()

    except RuntimeError as r:
        print("ERROR: %s" % r)
        exit(1)

    except Exception as e:
        print("ERROR: %s" % e)
        exit(1)
