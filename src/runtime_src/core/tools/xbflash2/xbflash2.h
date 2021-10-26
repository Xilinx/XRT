/**
 * Copyright (C) 2021 Xilinx, Inc
 * Author: Sonal Santan, Ryan Radjabi, Chien-Wei Lan
 * Simple command line utility to inetract with SDX PCIe devices
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

#ifndef _XBFLASH2_H_
#define _XBFLASH2_H_
#include <string>

#include "pcidev.h"
#include "xspi.h"
#include "xqspips.h"
#include "firmware_image.h"

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>

namespace po = boost::program_options;

// Helper functions that can be used by all command handlers
//
void printHelp(bool printExpHelp);
void printSubCmdHelp(const std::string& subCmd);
bool canProceed(void);
void sudoOrDie(void);

// Subcommand handlers
int helpHandler(po::variables_map vm);
extern const char *subCmdHelpDesc;
extern const char *subCmdHelpUsage;

int programHandler(po::variables_map vm);
extern const char *subCmdProgramDesc;
extern const char *subCmdProgramUsage;

int dumpHandler(po::variables_map vm);
extern const char *subCmdDumpDesc;
extern const char *subCmdDumpUsage;
#endif
