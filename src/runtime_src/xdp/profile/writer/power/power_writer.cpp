/**
 * Copyright (C) 2020 Xilinx, Inc
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

#include <vector>

#include "xdp/profile/writer/power/power_writer.h"
#include "xdp/profile/database/database.h"

namespace xdp {

  PowerProfilingWriter::PowerProfilingWriter(const char* filename,
					     const char* d,
					     uint64_t index) :
    VPWriter(filename), deviceName(d), deviceIndex(index)
  {
  }

  PowerProfilingWriter::~PowerProfilingWriter()
  {    
  }

  void PowerProfilingWriter::write(bool openNewFile)
  {
    // Write header
    fout << "Target device: " << deviceName << std::endl ;
    fout << "timestamp"    << ","
	 << "12v_aux_curr" << ","
	 << "12v_aux_vol"  << ","
	 << "12v_pex_curr" << ","
	 << "12v_pex_vol"  << ","
	 << "vccint_curr"  << ","
	 << "vccint_vol"   << ","
	 << "3v3_pex_curr" << ","
	 << "3v3_pex_vol"  << ","
	 << "cage_temp0"   << ","
	 << "cage_temp1"   << ","
	 << "cage_temp2"   << ","
	 << "cage_temp3"   << ","
	 << "dimm_temp0"   << ","
	 << "dimm_temp1"   << ","
	 << "dimm_temp2"   << ","
	 << "dimm_temp3"   << ","
	 << "fan_temp"     << ","
	 << "fpga_temp"    << ","
	 << "hbm_temp"     << ","
	 << "se98_temp0"   << ","
	 << "se98_temp1"   << ","
	 << "se98_temp2"   << ","
	 << "vccint_temp"  << ","
	 << "fan_rpm"
	 << std::endl;

    // Write all of the data elements
    std::vector<VPDynamicDatabase::CounterSample> samples = 
      (db->getDynamicInfo()).getPowerSamples(deviceIndex) ;

    for (auto sample : samples)
    {
      fout << sample.first << "," ; // Timestamp
      for (auto value : sample.second)
      {
	fout << value << "," ;
      }
      fout << std::endl ;
    }
  }

} // end namespace xdp
