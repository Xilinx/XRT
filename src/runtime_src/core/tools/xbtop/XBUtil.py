#!/usr/bin/python3

#
# SPDX-License-Identifier: Apache-2.0
#
# Copyright (C) 2022 Xilinx, Inc
#

import sys
import math

# ----------------------------------------------------------------------------
# VT100 ASCII Constants
# https://en.wikipedia.org/wiki/ANSI_escape_code

ESC = '\x1b'          # Escape character
CSI = ESC + '['       # Control Sequence Introducer

CSHOW = CSI + "?25h"        # Show Cursor
CHIDE = CSI + "?25l"        # Hide Cursor
ALTSCREEN = CSI + "?1049h"  # Enable alternative screen buffer
NORMSCREEN = CSI + "?1049l" # Disable alternative screen buffer
ERASELINE = CSI + "2K"      # Erase Line

class ForegroundColor:
    reset = CSI + "0m"
    black = CSI + "30m"
    red = CSI + "31m"
    green = CSI + "32m"
    yellow = CSI + "33m"
    blue = CSI + "34m"
    magenta = CSI + "35m"
    cyan = CSI + "36m"
    white = CSI + "37m"
    lightblack = CSI + "90m"
    lightred = CSI + "91m"
    lightgreen = CSI + "92m"
    lightyellow = CSI + "93m"
    lightblue = CSI + "94m"
    lightmagenta = CSI + "95m"
    lightcyan = CSI + "96m"
    lightwhite = CSI + "97m"

fg = ForegroundColor()

class BackgroundColor:
    reset = CSI + "0m"
    black = CSI + "40m"
    red = CSI + "41m"
    green = CSI + "42m"
    yellow = CSI + "43m"
    blue = CSI + "44m"
    magenta = CSI + "45m"
    cyan = CSI + "46m"
    white = CSI + "47m"
    lightblack = CSI + "100m"
    lightred = CSI + "101m"
    lightgreen = CSI + "102m"
    lightyellow = CSI + "103m"
    lightblue = CSI + "104m"
    lightmagenta = CSI + "105m"
    lightcyan = CSI + "106m"
    lightwhite = CSI + "107m"

bg = BackgroundColor()

class Effects:
    reset = CSI + "0m"
    bold = CSI + "1m"
    dim = CSI + "2m"
    italic = CSI + "3m"
    underline = CSI + "4m"
    slowblink = CSI + "5m"
    rapidblink = CSI + "6m"
    reverse = CSI + "7m"

fx = Effects()

# Helper functions

def indented_print(term, lock, buf_list, loc_x, loc_y):
    with lock:
        for i, val in enumerate(buf_list):
            term.location(loc_x, loc_y + i)
            print(buf_list[i], end='')
            sys.stdout.flush()

def print_section_heading(term, lock, heading, loc_y):
    with lock:
        term.location(0, loc_y)
        print(fg.blue + fx.bold + fx.italic + heading + fx.reset)

def print_warning(term, lock, loc_y, msg):
    with lock:
        term.location(5, loc_y)
        print(fg.yellow + msg + fg.reset)

def clear_rows(term, lock, start_row, num_lines):
    with lock:
        for row in range(start_row, start_row + num_lines):
            term.location(0, row)
            print(ERASELINE,  end='')
            sys.stdout.flush()


def progress_bar(percent, width=24):
    import math
    segments = [b"\xe2\x96\xae".decode("utf-8"),
                b"\xe2\x96\xaf".decode("utf-8")]
    completed = math.floor(width * (percent / 100))

    bar = fg.green;
    for index in range(0, width):
        if index < completed:
            bar += segments[0]
            continue

        if index == completed:
            bar += fg.blue

        bar += segments[1]

    bar += fg.white + (" %3.2f%%" % percent)
    return bar

def pad_string(padded_string, padding_width, alignment):
    # Alignment values: 'left', 'right', 'center'
    while True:
        if alignment != "right":
            if len(padded_string) < padding_width:
                padded_string = padded_string + " "
        if alignment != "left":
            if len(padded_string) < padding_width :
                padded_string = " " + padded_string
        if len(padded_string) >= padding_width:
            break;

    return padded_string

def convert_size(size_bytes):
    if size_bytes == 0:
        return "0  B"

    size_name = ("B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB")
    i = int(math.floor(math.log(size_bytes, 1024)))
    p = math.pow(1024, i)
    s = round(size_bytes / p, 2)

    if i == 0:
        return ("%d  %s" % (s, size_name[i]))

    return "%.2f %s" % (s, size_name[i])


class Table:
    def __init__(self, header, data, format):
        self._header = header
        self._data = data
        self._format = format

    def _create_divider(self):
        line = "+"
        for col in range(len(self._header)):
            line = line + '-' * (len(self._header[col]) + 2) + "+"

        return line

    def _pad_header_and_data(self):
        # Examine each column looking for the maximum string length
        for col in range(len(self._header)):
            # Look for the maximum length
            max_length = len(self._header[col])
            for row in range(len(self._data)):
                if max_length < len(self._data[row][col]):
                    max_length = len(self._data[row][col])
            # Pad each entry
            self._header[col] = pad_string(self._header[col], max_length, "center")
            for row in range(len(self._data)):
                self._data[row][col] = pad_string(self._data[row][col], max_length, self._format[col])

    def _create_table(self):
        # Create the table
        table = []

        # Divider
        table.append(self._create_divider())

        # Add the header
        line = "|"
        for col in range(len(self._header)):
            line = line + " " + self._header[col] + " |"
        table.append(line)

        # Divider
        table.append(self._create_divider())

        # Add the data
        for row in range(len(self._data)):
            line = "|"
            for col in range(len(self._data[row])):
                line= line + " " + self._data[row][col] + " |"
            table.append(line)

        # Divider
        table.append(self._create_divider())

        return table

    def create_table(self):
        self._pad_header_and_data()
        return self._create_table()


class Terminal:
    # init method or constructor
    def __init__(self):
        self._stream = sys.stdout

    def __enter__(self):
        self._write_stream(ALTSCREEN)
        return self

    def __exit__(self, type, value, traceback):
        self.hide_cursor(False)
        self._write_stream(NORMSCREEN)

    def _write_stream(self, data):
        stream = self._stream
        stream.write(data)
        stream.flush()

    def hide_cursor(self, hide):
        if hide == True:
            self._write_stream(CHIDE)
        else:
            self._write_stream(CSHOW)

    def location(self, x_loc, y_loc):
        self._write_stream(CSI + "%d;%dH" % (y_loc, x_loc))


# ActiveState Code Recipe
# https://code.activestate.com/recipes/134892/
class _Getch:
    """Gets a single character from standard input.
       Does not echo to the screen.
    """
    def __init__(self):
        try:
            self.impl = _GetchWindows()
        except ImportError:
            self.impl = _GetchUnix()

    def __call__(self): return self.impl()


class _GetchUnix:
    def __init__(self):
        import tty

    def __call__(self):
        import sys, tty, termios
        fd = sys.stdin.fileno()
        old_settings = termios.tcgetattr(fd)
        try:
            tty.setraw(sys.stdin.fileno())
            ch = sys.stdin.read(1)
        finally:
            termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
        return ch


class _GetchWindows:
    def __init__(self):
        import msvcrt

    def __call__(self):
        import msvcrt
        return msvcrt.getch()

get_char = _Getch()
