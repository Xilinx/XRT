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

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "ProgressBar.h"
#include "EscapeCodes.h"

// 3rd Party Library - Include Files
#include <boost/format.hpp>

// System - Include Files
#include <iostream>
#include <thread>

// ------ N A M E S P A C E ---------------------------------------------------
using namespace XBUtilities;

// ------ F U N C T I O N S ---------------------------------------------------

static std::string 
format_time(std::chrono::duration<double> duration) {
  auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
  auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration - hours);
  auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration - minutes);

  return boost::str(boost::format("%02d:%02d:%02d") % hours.count() % minutes.count() % seconds.count());
}

ProgressBar::ProgressBar(std::string _op_name, unsigned int _max_iter, bool _is_batch, std::ostream& _ostr) 
    : op_name(_op_name), max_iter(_max_iter), is_batch(_is_batch), ostr(_ostr)
{
  percent_done = 0;
  elapsed_time = std::chrono::seconds(0);

  if (!is_batch) {
    ostr << EscapeCodes::cursor().hide() << EscapeCodes::fgcolor::reset() << "[ " << EscapeCodes::fgcolor(EscapeCodes::FGC_IN_PROGRESS).string() << std::string(20, ' ') 
            << EscapeCodes::fgcolor::reset() << "]  " << EscapeCodes::fgcolor(EscapeCodes::FGC_IN_PROGRESS).string() << percent_done << "%" 
            << EscapeCodes::fgcolor::reset() << " ("<< format_time(elapsed_time) << "): " << op_name 
            << "\n" << EscapeCodes::fgcolor::reset();
  } else {
      ostr << op_name << ": ";
  }
  ostr.flush();
}

void 
ProgressBar::finish() {
  std::string status = percent_done == 100 ? EscapeCodes::fgcolor(EscapeCodes::FGC_PASS).string() + "[PASSED] " : EscapeCodes::fgcolor(EscapeCodes::FGC_FAIL).string() + "[FAILED] ";
  if(is_batch) {
    status = percent_done == 100 ? "[PASSED]\n" : "[FAILED]\n";
    ostr << status;
  } else {
    ostr << EscapeCodes::cursor().prev_line() << EscapeCodes::cursor().clear_line() << status << EscapeCodes::fgcolor::reset() << "(" 
          << format_time(elapsed_time) << "): " << op_name << "\n" << EscapeCodes::fgcolor::reset() << EscapeCodes::cursor().show();
  }
  ostr.flush();
}

void 
ProgressBar::update(int iteration) {
  elapsed_time = timer.stop();

  percent_done = 100*iteration/max_iter;

  if (percent_done > 100) {
    finish();
    ostr <<  EscapeCodes::fgcolor::reset() << EscapeCodes::cursor().show();
    throw std::runtime_error("Passed in iteration number is greater than the max iteration");
  }

  if(is_batch) {
    if (percent_done % 5 == 0) {
      ostr << ".." << percent_done << "%";
    } else { ostr << ".."; }
    ostr.flush();
    return;
  }

  ostr << EscapeCodes::cursor().hide() << EscapeCodes::cursor().prev_line() << EscapeCodes::fgcolor::reset() << "[" << EscapeCodes::fgcolor(EscapeCodes::FGC_IN_PROGRESS).string() 
        << std::string(percent_done/5, '=') << ">" << std::string(20 - (percent_done / 5), ' ') 
        << EscapeCodes::fgcolor::reset() << "] " << EscapeCodes::fgcolor(EscapeCodes::FGC_IN_PROGRESS).string() << percent_done << "%" 
        << EscapeCodes::fgcolor::reset() << " ("<< format_time(elapsed_time) << "): " << op_name 
        << "\n" << EscapeCodes::fgcolor::reset();
  ostr.flush();
}
