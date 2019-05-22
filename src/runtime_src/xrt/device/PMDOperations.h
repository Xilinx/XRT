/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

#ifndef xrt_device_pmd_operations_h_
#define xrt_device_pmd_operations_h_

#include "pmdhal.h"

#ifdef PMD_OCL_DISABLED
#include "xclhal.h"
#include <string>

namespace xrt { namespace pmd {
  class PMDOperations {
  public:
    PMDOperations(const std::string &dll, void *handle);
    ~PMDOperations();

  public:
//    unsigned probe(int argc, char *argv[]) {
//      return probeFunc(argc, argv);
//    }
//    unsigned openPort(unsigned port) {
//      return openFunc(port);
//    }
//    unsigned getPortInfo(unsigned port, xclDeviceInfo *info) {
//      return infoFunc(port, info);
//    }
//    StreamHandle openStream(unsigned port, unsigned q, unsigned depth, unsigned dir) {
//      return openStreamFunc(port, q, depth, dir);
//    }
//    void closeStream(unsigned port, StreamHandle strm) {
//      closeStreamFunc(port, strm);
//    }
//    unsigned send(unsigned port, StreamHandle strm, PacketObject *pkts, unsigned count) {
//      return sendPktsFunc(port, strm, pkts, count);
//    }
//    unsigned recv(unsigned port, StreamHandle strm, PacketObject *pkts, unsigned count) {
//      return recvPktsFunc(port, strm, pkts, count);
//    }
//    PacketObject acquirePacket(unsigned port) {
//      return acquirePacketFunc(port);
//    }
//    void releasePacket(unsigned port, PacketObject pkt) {
//      return releasePacketFunc(port, pkt);
//    }

  private:
//    typedef unsigned (* probeFuncType)(int argc, char *argv[]);
//    typedef unsigned (* openFuncType)(unsigned port);
//    typedef StreamHandle (* openStreamFuncType)(unsigned port, unsigned q, unsigned depth, unsigned dir);
//    typedef void (* closeStreamFuncType)(unsigned port, StreamHandle strm);
//    typedef unsigned (* sendPktsFuncType)(unsigned port, StreamHandle strm, PacketObject *pkts, unsigned count);
//    typedef unsigned (* recvPktsFuncType)(unsigned port, StreamHandle strm, PacketObject *pkts, unsigned count);
//    typedef PacketObject (* acquirePktFuncType)(unsigned port);
//    typedef void (* releasePktFuncType)(unsigned port, PacketObject pkt);
//    typedef unsigned (* infoFuncType)(unsigned port, xclDeviceInfo *info);

  private:
//    probeFuncType probeFunc;
//    openFuncType openFunc;
//    infoFuncType infoFunc;
//    openStreamFuncType openStreamFunc;
//    closeStreamFuncType closeStreamFunc;
//    sendPktsFuncType sendPktsFunc;
//    recvPktsFuncType recvPktsFunc;
//    acquirePktFuncType acquirePacketFunc;
//    releasePktFuncType releasePacketFunc;
    void *dllHandle;
  };
}};

#endif
#endif


