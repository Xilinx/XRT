// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020 Xilinx, Inc
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __SubCmdExamine_h_
#define __SubCmdExamine_h_

#include "tools/common/SubCmd.h"
#include "tools/common/SubCmdExamineInternal.h"
#include <boost/property_tree/ptree.hpp>

class SubCmdExamine : public SubCmdExamineInternal {
 public:
  SubCmdExamine(bool _isHidden, bool _isDepricated, bool _isPreliminary, const boost::property_tree::ptree& configurations);

 private:
  std::string               m_device;
  std::vector<std::string>  m_reportNames; // Default set of report names are determined if there is a device or not
  std::vector<std::string>  m_elementsFilter;
  std::string               m_format; // Don't define default output format.  Will be defined later.
  std::string               m_output;
  bool                      m_help;
};

#endif

