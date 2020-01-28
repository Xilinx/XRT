/**
 * Copyright (C) 2020 Xilinx, Inc
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

#ifndef __ESCAPE_CHAR_H__
#define __ESCAPE_CHAR_H__

#include <string>

namespace escape_codes {
const uint8_t black = 0;
const uint8_t red = 1;
const uint8_t green = 2;
const uint8_t yellow = 3;
const uint8_t blue = 4;
const uint8_t magenta = 5;
const uint8_t cyan = 6;
const uint8_t white = 7;
const uint8_t bright_black = 8;
const uint8_t bright_red = 9;
const uint8_t bright_green = 10;
const uint8_t bright_yellow = 11;
const uint8_t bright_blue = 12;
const uint8_t bright_magenta = 13;
const uint8_t bright_cyan = 14;
const uint8_t bright_white = 15;

//text colouring
std::string fgcolour(uint8_t _color) { return std::string("\033[38;5;" + std::to_string(_color) + 'm'); };
std::string bgcolour(uint8_t _color) { return std::string("\033[48;5;" + std::to_string(_color) + 'm'); };

//cursor movements
std::string move_to(uint8_t _x = 1, uint8_t _y = 1) { return std::string("\033[" + std::to_string(_x) + ";" + std::to_string(_y) + 'H'); };
std::string move_up(uint8_t _count = 1) { return std::string("\033[" + std::to_string(_count) + 'A'); };
std::string move_down(uint8_t _count = 1) { return std::string("\033[" + std::to_string(_count) + 'B'); };
std::string move_right(uint8_t _count = 1) { return std::string("\033[" + std::to_string(_count) + 'C'); };
std::string move_left(uint8_t _count = 1) { return std::string("\033[" + std::to_string(_count) + 'D'); };
std::string move_next_line(uint8_t _count = 1) { return std::string("\033[" + std::to_string(_count) + 'E'); }; //To move cursor to the beginning of the line n(down)
std::string move_previous_line(uint8_t _count = 1) { return std::string("\033[" + std::to_string(_count) + 'F'); }; //To move cursor to the beginning of the line n(up)
std::string move_horizontal(uint8_t _count = 1) { return std::string("\033[" + std::to_string(_count) + 'G'); }; //To move cursor to any column on given line

//clearing screen
const std::string& clear_screen() { static const std::string s("\033[2J"); return s; };
const std::string& clear_till_cursor() { static const std::string s("\033[1J"); return s; };
const std::string& clear_from_cursor() { static const std::string s("\033[0J"); return s; };
const std::string& clear_line() { static const std::string s("\033[2K"); return s; };
const std::string& clear_left() { static const std::string s("\033[1K"); return s; };
const std::string& clear_right() { static const std::string s("\033[0K"); return s; };

//Graphics
const std::string& bold() { static const std::string s("\033[1m"); return s; };
const std::string& faint() { static const std::string s("\033[2m"); return s; };
const std::string& underline() { static const std::string s("\033[4m"); return s; };
const std::string& underline_off() { static const std::string s("\033[24m"); return s; };
const std::string& bold_off() { static const std::string s("\033[22m"); return s; };

const std::string& show_cursor() { static const std::string s("\033[?25h"); return s; };
const std::string& hide_cursor() { static const std::string s("\033[?25l"); return s; };
const std::string& enable_alternative_screen_buffer() { static const std::string s("\033[?1049h"); return s; };
const std::string& disable_alternative_screen_buffer() { static const std::string s("\033[?1049l"); return s; };
const std::string& fgcolor_reset() { static const std::string s("\033[39m"); return s; };
const std::string& bgcolor_reset() { static const std::string s("\033[49m"); return s; };
const std::string& save_cursor_pos() { static const std::string s("\0337"); return s; };
const std::string& restore_cursor_pos() { static const std::string s("\0338"); return s; };
const std::string& text_reset() { static const std::string s("\033[0m"); return s; };
const std::string& reset_all() { static const std::string s("\033c"); return s; };

}
#endif