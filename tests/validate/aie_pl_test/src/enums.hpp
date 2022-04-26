/**
* Copyright (C) 2019-2022 Xilinx, Inc
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

#ifndef _XF_PLCTRLAIE_ENUMS_HPP_
#define _XF_PLCTRLAIE_ENUMS_HPP_
namespace xf {
namespace plctrl {
namespace enums {
enum class aie_cmd {
  ADF_GRAPH_RUN = 0,
  ADF_GRAPH_WAIT = 1,
  ADF_GRAPH_RTP_UPDATE = 2,
  ADF_GRAPH_RTP_READ = 3,
  WAIT_FOR_DMA_IDLE = 4,
  SET_DMA_BD_LENGTH = 5,
  ENQUEUE_DMA_BD = 6,
  LOAD_AIE_PM = 7,
  SLEEP = 8,
  HALT = 9,
  SET_AIE_ITERATION = 10,
  ENABLE_AIE_CORES = 11,
  DISABLE_AIE_CORES = 12,
  SYNC = 13,
  LOOP_BEGIN = 14,
  LOOP_END = 15,
  SET_DMA_BD = 16,
  UPDATE_AIE_RTP = 17
};
} // end of namespace enums
  using namespace enums;
} // end of namespace plctrl
} // end of pl
#endif
