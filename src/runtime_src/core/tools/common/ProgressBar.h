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
#include <chrono>

// ------ N A M E S P A C E ---------------------------------------------------

namespace XBUtilities {

  class Timer 
  {
    std::chrono::high_resolution_clock::time_point mTimeStart;
    public:
      Timer() {
        reset();
      }
      std::chrono::duration<double> stop() {
        std::chrono::high_resolution_clock::time_point timeEnd = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double>(timeEnd - mTimeStart);
      }
      void reset() {
        mTimeStart = std::chrono::high_resolution_clock::now();
      }
  };

  class ProgressBar
  {
    private:
	  std::string op_name;
	  int percent_done;
    unsigned int max_iter;
	  bool is_batch;
    std::ostream& ostr;
    Timer timer;
	  std::chrono::duration<double> elapsed_time;

    public:
	    ProgressBar(std::string _op_name, unsigned int _max_iter, bool _is_batch, std::ostream& _ostr);
	  void 
      update(int iteration);
    void
      finish();
  };
}
