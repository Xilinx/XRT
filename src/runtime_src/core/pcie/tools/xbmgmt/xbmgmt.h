/**
 * Copyright (C) 2016-2019 Xilinx, Inc
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
#ifndef XBUTIL_H
#define XBUTIL_H

#include <string>

// Helper functions that can be used by all command handlers
//
void printHelp(bool printExpHelp);
void printSubCmdHelp(const std::string& subCmd, bool show_expert);
bool canProceed(void);
void sudoOrDie(void);
unsigned int bdf2index(const std::string& bdfStr);
std::string getBDF(unsigned index);
std::string driver_version(std::string driver);
bool getenv_or_null(const char* env);
int xrt_xbmgmt_version_cmp() ;

// Subcommand handlers
int helpHandler(int argc, char *argv[]);
extern const char *subCmdHelpDesc;
extern const char *subCmdHelpUsage;

int versionHandler(int argc, char *argv[]);
extern const char *subCmdVersionDesc;
extern const char *subCmdVersionUsage;

int scanHandler(int argc, char *argv[]);
extern const char *subCmdScanDesc;
extern const char *subCmdScanUsage;

int flashXbutilFlashHandler(int argc, char *argv[]);
extern const char *subCmdXbutilFlashDesc;
extern const char *subCmdXbutilFlashUsage;
int flashHandler(int argc, char *argv[]);
extern const char *subCmdFlashDesc;
extern const char *subCmdFlashUsage;
extern const char *subCmdFlashExpUsage;

int resetHandler(int argc, char *argv[]);
extern const char *subCmdResetDesc;
extern const char *subCmdResetUsage;

int clockHandler(int argc, char *argv[]);
extern const char *subCmdClockDesc;
extern const char *subCmdClockUsage;

int partHandler(int argc, char *argv[]);
extern const char *subCmdPartDesc;
extern const char *subCmdPartUsage;
extern const char *subCmdPartExpUsage;

int configHandler(int argc, char *argv[]);
extern const char *subCmdConfigDesc;
extern const char *subCmdConfigUsage;
extern const char *subCmdConfigExpUsage;

int nifdHandler(int argc, char *argv[]);
extern const char *subCmdNifdDesc;
extern const char *subCmdNifdUsage;

int hotplugHandler(int argc, char *argv[]);
extern const char *subCmdHotplugDesc;
extern const char *subCmdHotplugUsage;

#endif /* XBMGMT_H */
