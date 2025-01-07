/**
 * Copyright (C) 2022-2024 Advanced Micro Devices, Inc. - All rights reserved
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

#ifndef AIE_DEBUG_WRITER_METADATA_H
#define AIE_DEBUG_WRITER_METADATA_H

#include "core/common/device.h"
#include "core/common/message.h"
#include "core/include/xrt/xrt_hw_context.h"
#include "xdp/config.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/database/static_info/aie_util.h"
#include "xdp/profile/database/static_info/filetypes/base_filetype_impl.h"

extern "C" {
#include <xaiengine.h>
#include <xaiengine/xaiegbl_params.h>
}

namespace xdp {

class UsedRegisters;

class AieDebugMetadata {
  public:
    AieDebugMetadata();

  private:
    // List of AIE HW generation-specific registers
    std::unique_ptr<UsedRegisters> usedRegisters;
};


/*************************************************************************************
The class UsedRegisters is what gives us AIE hw generation specific data. The base class
has virtual functions which populate the correct registers and their addresses according
to the AIE hw generation in the derived classes. Thus we can dynamically populate the
correct registers and their addresses at runtime.
**************************************************************************************/
class UsedRegisters {
  public:
    UsedRegisters() {
      populateCoreModuleMap();
      populateMemoryModuleMap();
      populateMemoryTileMap();
      populateShimTileMap();
    }

    virtual ~UsedRegisters() {
      fieldMap.clear();
      shiftMap.clear();
      maskMap.clear();
    }

    virtual void populateCoreModuleMap() {};
    virtual void populateMemoryModuleMap() {};
    virtual void populateMemoryTileMap() {};
    virtual void populateShimTileMap() {};

  protected:
    std::map<std::pair<std::string, std::string>, std::vector<RegData>> fieldMap;
    std::map<std::string, int> shiftMap;
    std::map<std::string, uint64_t> maskMap;

  private:
    struct RegData {
    std::string fieldName;
    int shift;
    uint64_t mask;
    };

};

/*************************************************************************************
 AIE1 Registers
 *************************************************************************************/
class AIE1UsedRegisters : public UsedRegisters {
public:
  AIE1UsedRegisters() {
    populateCoreModuleMap();
    populateMemoryModuleMap();
    populateMemoryTileMap();
    populateShimTileMap();
  }
  ~AIE1UsedRegisters() = default;

};

/*************************************************************************************
 AIE2 Registers
 *************************************************************************************/
class AIE2UsedRegisters : public UsedRegisters {
public:
  AIE2UsedRegisters() {
    populateCoreModuleMap();
    populateMemoryModuleMap();
    populateMemoryTileMap();
    populateShimTileMap();
  }
  ~AIE2UsedRegisters() = default;

};

/*************************************************************************************
 AIE2PS Registers
 *************************************************************************************/
class AIE2PSUsedRegisters : public UsedRegisters {
public:
  AIE2PSUsedRegisters() {
    populateCoreModuleMap();
    populateMemoryModuleMap();
    populateMemoryTileMap();
    populateShimTileMap();
  }
  ~AIE2PSUsedRegisters() = default;

};

} // end XDP namespace

#endif
