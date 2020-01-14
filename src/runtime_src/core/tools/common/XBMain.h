/**
 * Copyright (C) 2019-2020 Xilinx, Inc
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

#ifndef __XBMgmtMain_h_
#define __XBMgmtMain_h_

#include <string>
#include <vector>
#include <memory>

// ------------ F O R W A R D - D E C L A R A T I O N S ----------------------
// Forward declarations - use these instead whenever possible...
class SubCmd;

// ----------------------- T Y P E D E F S -----------------------------------
typedef std::vector<std::shared_ptr<SubCmd>> SubCmdsCollection;
    
// ---------------------- F U N C T I O N S ----------------------------------
void main_(int argc, char** argv, 
           const std::string & _description,
           const SubCmdsCollection & _SubCmds);
#endif
