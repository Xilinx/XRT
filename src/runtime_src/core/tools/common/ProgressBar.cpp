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

static void
update_batch(int percent, std::ostream& ostr) {
  if (percent % 10 == 0) {
    ostr << "....." << percent << "%";
  } else { ostr << "."; }

  if (percent == 100)
      ostr << " [--Complete--]\n";
  ostr.flush();
}

ProgressBar::ProgressBar(std::string _op_name, bool _is_batch, std::ostream& _ostr) 
    : op_name(_op_name), is_batch(_is_batch), ostr(_ostr)
{
  percent_done = 0; 
  elapsed_time = std::chrono::seconds(0);

  if (!is_batch) {
    ostr <<  ec::cursor().hide() << ec::fgcolor(ec::FGC_IN_PROGRESS).string() << "[" << std::string(20, ' ') 
            << "] " << percent_done << "% ("<< format_time(elapsed_time) << "): " << op_name 
            << "\n" << ec::fgcolor::reset() << ec::cursor().prev_line();
  } else {
      ostr << op_name << ": ";
  }
  ostr.flush();
}

void 
ProgressBar::update(int percent, std::chrono::duration<double> duration) {
  elapsed_time += duration;
  percent_done += percent;

  if(is_batch) {
      update_batch(percent_done, ostr);
      return;
  }
    
  if (percent_done == 100) {
    ostr << ec::fgcolor(ec::FGC_PASS).string() << "[-------Complete------] 100% " << "(" 
            << format_time(elapsed_time) << "): " << op_name << "\n" << ec::fgcolor::reset() 
            << ec::cursor().show();
    ostr.flush();
    return;
  }
  ostr <<  ec::cursor().hide() << ec::fgcolor(ec::FGC_IN_PROGRESS).string() << "[" 
        << std::string(percent_done/5, '=') << ">" << std::string(20 - (percent_done / 5), ' ') 
        << "] " << percent_done << "% ("<< format_time(elapsed_time) << "): " << op_name 
        << "\n" << ec::fgcolor::reset() << ec::cursor().prev_line();
  ostr.flush();
}