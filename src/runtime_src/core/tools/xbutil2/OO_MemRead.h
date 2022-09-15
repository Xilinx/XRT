// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __OO_MemRead_h_
#define __OO_MemRead_h_

#include "tools/common/OptionOptions.h"
#include <vector>

class OO_MemRead : public OptionOptions {
 public:
  virtual void execute( const SubCmdOptions &_options ) const;

 public:
  OO_MemRead( const std::string &_longName, bool _isHidden = false );

 private:
   std::string m_device;
   std::string m_baseAddress;
   std::string m_sizeBytes;
   int m_count;
   std::string m_outputFile;
   bool m_help;
};

#endif
