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

// Include files

// Please keep these to the bare minimum
#include "XBHelpMenus.h"
#include <string>
    
// ---------------------- F U N C T I O N S ----------------------------------
void main_(int argc, char** argv, 
           const std::string & _executable,
           const std::string & _description,
           const SubCmdsCollection & _SubCmds);
#endif
