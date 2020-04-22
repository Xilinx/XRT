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

// ------ S T A T I C   V A R I A B L E S -------------------------------------
static unsigned int ProgressBarWidth = 20;

static boost::format fmtUpdate(EscapeCodes::cursor().hide()+
                               EscapeCodes::fgcolor::reset()+ "[" +
                               EscapeCodes::fgcolor(EscapeCodes::FGC_IN_PROGRESS).string()+ "%-" + std::to_string(ProgressBarWidth)+ "s" +
                               EscapeCodes::fgcolor::reset()+ "]" +
                               EscapeCodes::fgcolor(EscapeCodes::FGC_IN_PROGRESS).string()+ "%3d%%" +
                               EscapeCodes::fgcolor::reset()+ ": %s... < %s >    "
                              );

static boost::format fmtPassed(EscapeCodes::cursor().hide()+
                               EscapeCodes::fgcolor(EscapeCodes::FGC_PASS).string()+ "[PASSED]" +
                               EscapeCodes::fgcolor::reset()+ " : %s < %s >"
                              );

static boost::format fmtFailed(EscapeCodes::cursor().hide()+
                               EscapeCodes::fgcolor(EscapeCodes::FGC_FAIL).string()+ "[FAILED]" +
                               EscapeCodes::fgcolor::reset()+ " : %s < %s >"
                              );

static boost::format fmtBatchPF("\n[%s]: %s < %s >\n");


// ------ S T A T I C   F U N C T I O N S -------------------------------------

static std::string
format_time(std::chrono::duration<double> duration) 
{
  auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
  auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration);
  auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);

  std::string formattedTime;
  if (hours.count() != 0) 
    formattedTime += std::to_string(hours.count()) + "h ";

  if (hours.count() != 0 || minutes.count() != 0) 
    formattedTime += std::to_string(minutes.count() % 60) + "m ";

  formattedTime += std::to_string(seconds.count() % 60) + "s";

  return formattedTime;
}



ProgressBar::ProgressBar(const std::string &_opNname, unsigned int _maxNumIterations,
                         bool _isBatch, std::ostream &_ostr)
    : m_opName(_opNname)
    , m_maxNumIterations(_maxNumIterations)
    , m_isBatch(_isBatch)
    , m_ostr(_ostr)
    , m_runningIteration(0)
    , m_finished(false)
    , m_elapsedTime(std::chrono::seconds(0))
    , m_lastUpdated(std::chrono::high_resolution_clock::now()) 
{
  if (!m_isBatch) 
    m_ostr << fmtUpdate % "" % /*Percent*/ 0 % m_opName % format_time(m_elapsedTime) << std::endl;
  else 
    m_ostr << m_opName << ": ";

  m_ostr.flush();
}

ProgressBar::~ProgressBar() 
{
  // If the class is destroyed and the developer has NOT executed the 
  // finished operation, restore the cursor
  if ((m_finished == false) && (m_isBatch == false)) 
    m_ostr << EscapeCodes::cursor().show() << std::flush;
}

void
ProgressBar::finish(bool _successful, const std::string &_msg) 
{
  // Should only be called once (not more then once)
  assert(m_finished == false);

  // Update the runningTime
  m_elapsedTime = m_timer.stop();

  // -- Batch --
  if (m_isBatch) {
    m_ostr << fmtBatchPF % (_successful ? "PASSED" : "FAILED") % _msg % format_time(m_elapsedTime);
    m_ostr.flush();
    return;
  }

  // -- Non-Batch --
  boost::format &fmt = _successful ? fmtPassed : fmtFailed;
  m_ostr << EscapeCodes::cursor().prev_line()
         << EscapeCodes::cursor().clear_line()
         << fmt % _msg % format_time(m_elapsedTime)
         << std::endl << EscapeCodes::cursor().show();

  m_ostr.flush();
  m_finished = true;
}



void
ProgressBar::update(unsigned int _iteration) 
{
  // Some simple DRC checks
  assert(_iteration <= m_maxNumIterations);
  if (_iteration > m_maxNumIterations) 
    _iteration = m_maxNumIterations;

  // Going back in time (e.g., the iteration values are getting smaller)
  assert(_iteration >= m_runningIteration);
  if (_iteration > m_maxNumIterations) 
    _iteration = m_maxNumIterations;

  // -- Batch --
  if (m_isBatch) {
    // Has progress been made?
    if (_iteration == m_runningIteration) 
      return;

    // Bring the current iterator up to the the latest
    for (auto currentIteration = m_runningIteration + 1;
         currentIteration <= _iteration;
         ++currentIteration) {
      m_ostr << ".";            // Progressd period '.'

      // Now determine percentage progress values
      static const std::vector<unsigned int> reportPercentages = { 25, 50, 75, 100 };
      unsigned int prevPercent = currentIteration == 0 ? 0 : (100 * (currentIteration - 1)) / m_maxNumIterations;
      unsigned int nextPercent = (100 * currentIteration) / m_maxNumIterations;

      for (const auto &reportPercent : reportPercentages) {
        if ((reportPercent > prevPercent) && (reportPercent <= nextPercent)) {
          m_ostr << std::to_string(reportPercent) << "%";
        }
      }
    }
    m_runningIteration = _iteration;
    m_ostr.flush();
    return;
  }

  // -- Non-batch --
  // Now check to see if we need to "refresh" the progress message.
  // Note: Currently if nothing has changed in the past 0.5 seconds then do nothing.
  static unsigned int MaxRefreshMSec = 500;
  if ((_iteration == m_maxNumIterations) &&
      (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - m_lastUpdated).count() < MaxRefreshMSec)) 
    return;

  // Process the data
  m_runningIteration = _iteration;
  unsigned int runningPercent = (100 * m_runningIteration) / m_maxNumIterations;

  // Get the runningTime
  m_elapsedTime = m_timer.stop();
  m_lastUpdated = std::chrono::high_resolution_clock::now();

  // Create the progress bar
  std::string progressBar = std::string(runningPercent / (100 / ProgressBarWidth), '=');
  if (runningPercent < 100) 
    progressBar += '>';

  // Write the new progress bar
  m_ostr << EscapeCodes::cursor().prev_line()
         << fmtUpdate % progressBar % runningPercent % m_opName % format_time(m_elapsedTime)
         << std::endl;

  m_ostr.flush();
}

