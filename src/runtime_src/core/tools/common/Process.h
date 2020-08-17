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

#ifndef __Process_h_
#define __Process_h_

#include <vector>

namespace XBUtilities {
  unsigned int
    runPythonScript( const std::string & script, 
                 const std::vector<std::string> & args,
                 std::ostringstream & os_stdout,
                 std::ostringstream & os_stderr);
};

#endif
