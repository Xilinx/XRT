// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved

#ifndef RUN_SUMMARY_MANAGER_DOT_H
#define RUN_SUMMARY_MANAGER_DOT_H

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "xdp/config.h"

namespace xdp {
  constexpr uint64_t HOST_PL_CONTEXT = 0;

  // Forward declaration of XDP constructs
  class VPWriter;

  struct OpenedFileDescriptor
  {
    std::string name;
    std::string type;

    // Each file is associated with a specific group, either Host + PL or
    // a specific portion of AIE.  From the host code this is uniquely
    // associated with a hw_context.
    uint64_t contextId;
  };

  struct SystemDiagramEntry
  {
    uint64_t contextId;
    std::string systemDiagram;
  };

  class RunSummaryManager
  {
  private:
    std::unique_ptr<VPWriter> runSummary;

    std::mutex summaryLock;
    std::vector<OpenedFileDescriptor> openedFiles;

    // Each individual context will have a system diagram string
    std::vector<SystemDiagramEntry> systemDiagrams;
    
  public:
    RunSummaryManager() = default;
    ~RunSummaryManager() = default;

    // There should only be one instance of the RunSummaryManager
    // owned by the singleton VPDatabase object.  So explicitly
    // disable copys and moves.

    // Copy constructor
    RunSummaryManager(const RunSummaryManager& x) = delete;
    // Copy assignment
    RunSummaryManager& operator=(const RunSummaryManager& x) = delete;
    // Move constructor
    RunSummaryManager(RunSummaryManager&& r) = delete;
    // Move assignment
    RunSummaryManager& operator=(RunSummaryManager&& r) = delete;

    XDP_CORE_EXPORT
    void addOpenedFile(const std::string& name, const std::string& type,
		       uint64_t deviceId);
    std::vector<OpenedFileDescriptor>& getOpenedFiles();
    std::vector<SystemDiagramEntry> getSystemDiagrams();
    void updateSystemDiagram(const char*, size_t, uint64_t);
  };
  
} // end namespace xdp

#endif
