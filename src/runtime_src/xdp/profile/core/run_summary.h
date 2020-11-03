/**
 * Copyright (C) 2019 Xilinx, Inc
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

#ifndef __XDP_CORE_RUN_SUMMARY_H
#define __XDP_CORE_RUN_SUMMARY_H

// ----------------------- I N C L U D E S -----------------------------------

// #includes here - please keep these to a bare minimum!
#include "xclbin.h"
#include "xdp/config.h"

#include <string>
#include <fstream>
#include <memory>
#include <vector>
#include <boost/property_tree/ptree.hpp>

// ------------ F O R W A R D - D E C L A R A T I O N S ----------------------
// Forward declarations - use these instead whenever possible...

// --------------- C L A S S :   R u n S u m m a r y -------------------------

class RunSummary {
  public:
    // File types supported in the run_summary file
    enum FileType {
      FT_UNKNOWN,
      FT_PROFILE,
      FT_TRACE,
      FT_WDB,
      FT_WDB_CONFIG,
      FT_POWER_PROFILE,
      FT_KERNEL_PROFILE,
      FT_KERNEL_TRACE,
      FT_VP_TRACE
    };

  public:
    XDP_EXPORT
    RunSummary();
    XDP_EXPORT
    ~RunSummary();

  public:
    XDP_EXPORT
    void addFile(const std::string & fileName, FileType eFileType);
    XDP_EXPORT
    void setProfileTree(std::shared_ptr<boost::property_tree::ptree> tree);

    XDP_EXPORT
    void extractSystemProfileMetadata(const axlf * pXclbinImage, const std::string & xclbinContainerName = "");
    void writeContent();

  protected:
    const std::string getFileTypeAsStr(FileType eFileType);

  private:
    std::vector< std::pair< std::string, FileType> > mFiles;
    std::string mSystemMetadata;
    std::string mXclbinContainerName;
    std::shared_ptr<boost::property_tree::ptree> mProfileTree;

  private:
    // Purposefully private and undefined ctors...
    RunSummary(const RunSummary& obj);
    RunSummary& operator=(const RunSummary& obj);
};



#endif
