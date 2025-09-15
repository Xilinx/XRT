// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved

#include <iomanip>
#include <sstream>

#define XDP_CORE_SOURCE

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/run_summary_manager.h"
#include "xdp/profile/writer/vp_base/vp_run_summary.h"

namespace xdp {

  void RunSummaryManager::addOpenedFile(const std::string& name,
					const std::string& type,
					uint64_t contextId)
  {
    {
      // Protect changes to openedFiles and creation of the run summary.
      // The write function, however, needs to query the opened files so
      // place the lock inside its own scope
      std::lock_guard<std::mutex> lock(summaryLock);

      openedFiles.push_back({name, type, contextId});

      if (runSummary == nullptr)
	runSummary = std::make_unique<VPRunSummaryWriter>("xrt.run_summary", VPDatabase::Instance());
    }
    runSummary->write(false);
  }

  std::vector<OpenedFileDescriptor>& RunSummaryManager::getOpenedFiles()
  {
    std::lock_guard<std::mutex> lock(summaryLock);
    return openedFiles;
  }

  std::vector<SystemDiagramEntry> RunSummaryManager::getSystemDiagrams()
  {
    std::lock_guard<std::mutex> lock(summaryLock);
    return systemDiagrams;
  }

  void RunSummaryManager::updateSystemDiagram(const char* systemMetadataSection,
					      size_t systemMetadataSz,
					      uint64_t contextId)
  {
    if (systemMetadataSection == nullptr || systemMetadataSz <= 0)
      return;

    // TODO: Expand this so multiple devices and multiple xclbins
    // don't overwrite the single system diagram information
    std::ostringstream buf;
    for (size_t index = 0; index < systemMetadataSz; ++index) {
      buf << std::hex << std::setw(2) << std::setfill('0')
	  << static_cast<unsigned int>(systemMetadataSection[index]);
    }

    std::lock_guard<std::mutex> lock(summaryLock);

    SystemDiagramEntry value = { contextId, buf.str() } ;
    systemDiagrams.push_back(value);
    //systemDiagram = buf.str();
  }
  
} // end namespace xdp
