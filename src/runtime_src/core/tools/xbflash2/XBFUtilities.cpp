/**
 * Copyright (C) 2022 Xilinx, Inc
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
#include "XBFUtilities.h"

// 3rd Party Library - Include Files
#include <boost/property_tree/json_parser.hpp>
#include <boost/tokenizer.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string/split.hpp>

// System - Include Files
#include <iostream>
#include <map>
#include <regex>


bool
XBFUtilities::can_proceed(bool force)
{
    bool proceed = false;
    std::string input;

    std::cout << "Are you sure you wish to proceed? [Y/n]: ";

    if (force)
        std::cout << "Y (Force override)" << std::endl;
    else
        std::getline(std::cin, input);

    // Ugh, the std::transform() produces windows compiler warnings due to
    // conversions from 'int' to 'char' in the algorithm header file
    boost::algorithm::to_lower(input);
    //std::transform( input.begin(), input.end(), input.begin(), [](unsigned char c){ return std::tolower(c); });
    //std::transform( input.begin(), input.end(), input.begin(), ::tolower);

    // proceeds for "y", "Y" and no input
    proceed = ((input.compare("y") == 0) || input.empty());
    if (!proceed)
        std::cout << "Action canceled." << std::endl;
    return proceed;
}

void
XBFUtilities::sudo_or_throw()
{
#ifndef _WIN32
    const char* SudoMessage = "ERROR: root privileges required.";
    if ((getuid() == 0) || (geteuid() == 0))
        return;
    std::cout << SudoMessage << std::endl;
    exit(-EPERM);
#endif
}
