// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.

#ifndef TestSanity_h_
#define TestSanity_h_

#include "tools/common/TestRunner.h"
#include "xrt/xrt_device.h"

#include <boost/property_tree/ptree.hpp>
#include <memory>
#include <string>

class TestSanity : public TestRunner {
public:
  boost::property_tree::ptree run(const std::shared_ptr<xrt_core::device>&) override;
  boost::property_tree::ptree run(const std::shared_ptr<xrt_core::device>&, const xrt_core::archive*) override;

  TestSanity();

private:
  void
  run_sanity(const std::shared_ptr<xrt_core::device>& dev,
             const std::string& recipe_data,
             const std::string& profile_data,
             const xrt_core::runner::artifacts_repository& artifacts_repo,
             boost::property_tree::ptree& ptree) const;

  void
  run_strix(const std::shared_ptr<xrt_core::device>& dev,
            const xrt_core::archive* archive,
            boost::property_tree::ptree& ptree) const;

  void
  run_npu3(const std::shared_ptr<xrt_core::device>& dev,
           const xrt_core::archive* archive,
           boost::property_tree::ptree& ptree) const;
};

#endif
