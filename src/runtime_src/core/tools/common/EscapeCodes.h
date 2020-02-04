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

// Include files
// Please keep these to the bare minimum
#include <string>

namespace EscapeCodes {
  class fgcolor
    {
    public:
      fgcolor(uint8_t _color) : m_color(_color) {};
      std::string string() const { return "\033[38;5;" + std::to_string(m_color) + "m"; }
      static const std::string reset() { return "\033[39m"; };
      friend std::ostream& operator <<(std::ostream& os, const fgcolor & _obj) { return os << _obj.string(); }
  
   private:
     uint8_t m_color;
  };

  class cursor
    {
    public:
      std::string hide() const { return std::string("\033[?25l"); };
      std::string show() const { return std::string("\033[?25h"); };
      std::string prev_line() const { return std::string("\033[F"); };
      std::string clear_line() const { return std::string("\033[2K"); };
  };
  
  // ------ C O L O R S -------------------------------------------------------
  static const uint8_t FGC_IN_PROGRESS   = 111;
  static const uint8_t FGC_PASS          = 2;
  static const uint8_t FGC_FAIL          = 1;
}
