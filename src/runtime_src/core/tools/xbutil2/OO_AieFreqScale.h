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

#ifndef __OO_AieFreqScale_h_
#define __OO_AieFreqScale_h_

#include "tools/common/OptionOptions.h"

class OO_AieFreqScale : public OptionOptions {
 public:
  virtual void execute( const SubCmdOptions &_options ) const;

 public:
  OO_AieFreqScale( const std::string &_longName, bool _isHidden = false );

 private:
   std::string m_device;
   bool m_set;
   bool m_get;
   uint32_t m_partition_id;
   std::string m_freq;
   bool m_help;
};

#endif
