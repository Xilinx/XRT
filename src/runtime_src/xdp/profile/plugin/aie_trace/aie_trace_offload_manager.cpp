// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved
#define XDP_PLUGIN_SOURCE
#include "xdp/profile/plugin/aie_trace/aie_trace_offload_manager.h"

namespace xdp {
  using severity_level = xrt_core::message::severity_level;

  void AIETraceOffloadManager::startPLIOOffload(bool continuousTrace, uint64_t offloadIntervalUs) {
    if (plio.offloader && continuousTrace) {
      plio.offloader->setContinuousTrace();
      plio.offloader->setOffloadIntervalUs(offloadIntervalUs);
    }
    if (plio.offloader)
      plio.offloader->startOffload();
  }

  void AIETraceOffloadManager::startGMIOOffload(bool continuousTrace, uint64_t offloadIntervalUs) {
    if (gmio.offloader && continuousTrace) {
      gmio.offloader->setContinuousTrace(); // GMIO trace offload does not support continuous trace
      gmio.offloader->setOffloadIntervalUs(offloadIntervalUs);
    }
    if (gmio.offloader)
      gmio.offloader->startOffload();
  }

uint64_t AIETraceOffloadManager::checkAndCapToBankSize(uint8_t memIndex, uint64_t desired)
{
  auto* memory = db->getStaticInfo().getMemory(deviceID, memIndex);
  if (!memory)
    return desired;

  const uint64_t fullBankSize = static_cast<uint64_t>(memory->size) * 1024ULL;
  if ((fullBankSize > 0) && (desired > fullBankSize)) {
    xrt_core::message::send(severity_level::warning, "XRT",
      "Requested AIE trace buffer is too big for memory resource. Limiting to "
      + std::to_string(fullBankSize) + ".");
    return fullBankSize;
  }
  return desired;
}

  AIETraceOffloadManager::AIETraceOffloadManager(uint64_t device_id, VPDatabase* database, AieTraceImpl* impl)
    : deviceID{device_id},
      db{database},
      aieTraceImpl{impl},
      offloadEnabledPLIO(true),
      offloadEnabledGMIO(true)
  {}

  void AIETraceOffloadManager::initPLIO(void* handle, PLDeviceIntf* deviceIntf, uint64_t bufSize, uint64_t numStreams, XAie_DevInst* devInst) {
    if (!offloadEnabledPLIO)
      return;

    plio.logger = std::make_unique<AIETraceDataLogger>(deviceID, io_type::PLIO);
#ifndef XDP_CLIENT_BUILD
    plio.offloader = std::make_unique<AIETraceOffload>(handle, deviceID, deviceIntf, plio.logger.get(), true, bufSize, numStreams, devInst);
#else
    // Suppress unused parameter warnings in client build
    (void)handle;
    (void)deviceIntf;
    (void)bufSize;
    (void)numStreams;
    (void)devInst;
#endif
    plio.valid = true;
    std::stringstream msg;
    msg << "Total size of " << std::fixed << std::setprecision(3)
        << (bufSize / (1024.0 * 1024.0))
        << " MB is used for AIE trace buffer for "
        << numStreams << " PLIO streams.";
    xrt_core::message::send(severity_level::debug, "XRT", msg.str());

  }

  #ifdef XDP_CLIENT_BUILD
  void AIETraceOffloadManager::initGMIO(void* handle, PLDeviceIntf* deviceIntf,
              uint64_t bufSize, uint64_t numStreams, xrt::hw_context context, 
              std::shared_ptr<AieTraceMetadata> metadata) {
    if (!offloadEnabledGMIO)
      return;

    gmio.logger = std::make_unique<AIETraceDataLogger>(deviceID, io_type::GMIO);
    // Use the client-specific AIETraceOffload constructor
    gmio.offloader = std::make_unique<AIETraceOffload>(
        handle, deviceID, deviceIntf, gmio.logger.get(), false, // isPLIO = false
        bufSize, numStreams, context, metadata);
    gmio.valid = true;
    std::stringstream msg;
    msg << "Total size of " << std::fixed << std::setprecision(3)
        << (bufSize / (1024.0 * 1024.0))
        << " MB is used for AIE trace buffer for "
        << numStreams << " GMIO streams.";
    xrt_core::message::send(severity_level::debug, "XRT", msg.str());
  }
#else
  void AIETraceOffloadManager::initGMIO(void* handle, PLDeviceIntf* deviceIntf, 
                                        uint64_t bufSize, uint64_t numStreams, XAie_DevInst* devInst) {
    if (!offloadEnabledGMIO) {
      return;
    }

    gmio.logger = std::make_unique<AIETraceDataLogger>(deviceID, io_type::GMIO);
    gmio.offloader = std::make_unique<AIETraceOffload>(handle, deviceID, deviceIntf, gmio.logger.get(), false, bufSize, numStreams, devInst);
    gmio.valid = true;
    std::stringstream msg;
    msg << "Total size of " << std::fixed << std::setprecision(3)
        << (bufSize / (1024.0 * 1024.0))
        << " MB is used for AIE trace buffer for "
        << numStreams << " GMIO streams.";
    xrt_core::message::send(severity_level::debug, "XRT", msg.str());
  }
#endif

  void AIETraceOffloadManager::startOffload(bool continuousTrace, uint64_t offloadIntervalUs){
    if (!offloadEnabledPLIO && !offloadEnabledGMIO)
      return;

    if (offloadEnabledPLIO)
      startPLIOOffload(continuousTrace, offloadIntervalUs);
    if (offloadEnabledGMIO)
      startGMIOOffload(continuousTrace, offloadIntervalUs);
  }

  bool AIETraceOffloadManager::initReadTraces() {
    bool readStatus = true;

    if (offloadEnabledPLIO && plio.offloader)
      readStatus &= plio.offloader->initReadTrace();

    if (offloadEnabledGMIO && gmio.offloader)
      readStatus &= gmio.offloader->initReadTrace();

    return readStatus;
  }

  void AIETraceOffloadManager::flushAll(bool warn) {
    if (offloadEnabledPLIO && plio.offloader)
      flushOffloader(plio.offloader, warn);

    if (offloadEnabledGMIO && gmio.offloader)
      flushOffloader(gmio.offloader, warn);

  }

  void AIETraceOffloadManager::flushOffloader(const std::unique_ptr<AIETraceOffload>& offloader, bool warn) {
    if (offloader->continuousTrace()) {
      offloader->stopOffload();
      while (offloader->getOffloadStatus() != AIEOffloadThreadStatus::STOPPED) {}
    } else {
      offloader->readTrace(true);
      offloader->endReadTrace();
    }
    if (warn && offloader->isTraceBufferFull()) {
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", AIE_TS2MM_WARN_MSG_BUF_FULL);
    }
  }

  void AIETraceOffloadManager::createTraceWriters(uint64_t numStreamsPLIO, uint64_t numStreamsGMIO, std::vector<VPWriter*>& writers) {
    if (offloadEnabledPLIO) {
      // Add writer for every PLIO stream
      for (uint64_t n = 0; n < numStreamsPLIO; ++n) {
        std::string fileName = "aie_trace_plio_" + std::to_string(deviceID) + "_" +
                              std::to_string(n) + ".txt";
        VPWriter *writer = new AIETraceWriter(
          fileName.c_str(),
          deviceID,
          n,  // stream id
          "", // version
          "", // creation time
          "", // xrt version
          "",  // tool version
          io_type::PLIO // offload type
        );
        writers.push_back(writer);
        db->addOpenedFile(writer->getcurrentFileName(), "AIE_EVENT_TRACE", deviceID);

        std::stringstream msg;
        msg << "Creating AIE trace file " << fileName << " for device " << deviceID;
        xrt_core::message::send(severity_level::info, "XRT", msg.str());
      }
    }

    if (offloadEnabledGMIO) {
      // Add writer for every GMIO stream
      for (uint64_t n = 0; n < numStreamsGMIO; ++n) {
        std::string fileName = "aie_trace_gmio_" + std::to_string(deviceID) + "_" +
                              std::to_string(n) + ".txt";
        VPWriter *writer = new AIETraceWriter(
          fileName.c_str(),
          deviceID,
          n,  // stream id
          "", // version
          "", // creation time
          "", // xrt version
          "",  // tool version
          io_type::GMIO // offload type
        );
        writers.push_back(writer);
        db->addOpenedFile(writer->getcurrentFileName(), "AIE_EVENT_TRACE", deviceID);

        std::stringstream msg;
        msg << "Creating AIE trace file " << fileName << " for device " << deviceID;
        xrt_core::message::send(severity_level::info, "XRT", msg.str());
      }
    }
  }


bool AIETraceOffloadManager::configureAndInitPLIO(void* handle, PLDeviceIntf* deviceIntf,
                  uint64_t desiredBufSize, uint64_t numStreamsPLIO, XAie_DevInst* devInst)
{
  uint8_t memIndex = 0;
  if (deviceIntf)
    memIndex = deviceIntf->getAIETs2mmMemIndex(0);

  desiredBufSize = checkAndCapToBankSize(memIndex, desiredBufSize);
  desiredBufSize = aieTraceImpl->checkTraceBufSize(desiredBufSize);

  if (!devInst) {
    xrt_core::message::send(severity_level::warning, "XRT",
      "Unable to get AIE device instance. AIE event trace will not be available.");
    return false;
  }

  initPLIO(handle, deviceIntf, desiredBufSize, numStreamsPLIO, devInst);
  return true;
}

bool AIETraceOffloadManager::configureAndInitGMIO(
  void* handle, PLDeviceIntf* deviceIntf,
  uint64_t desiredBufSize, uint64_t numStreamsGMIO
#ifdef XDP_CLIENT_BUILD
  , const xrt::hw_context& hwctx, const std::shared_ptr<AieTraceMetadata>& md
#else
  , XAie_DevInst* devInst
#endif
  )
{
  desiredBufSize = checkAndCapToBankSize(/*bank 0*/ 0, desiredBufSize);
  desiredBufSize = aieTraceImpl->checkTraceBufSize(desiredBufSize);

#ifdef XDP_CLIENT_BUILD
  initGMIO(handle, deviceIntf, desiredBufSize, numStreamsGMIO, hwctx, md);
  return true;
#else
  if (!devInst) {
    xrt_core::message::send(severity_level::warning, "XRT",
      "Unable to get AIE device instance. AIE event trace will not be available.");
    return false;
  }
  initGMIO(handle, deviceIntf, desiredBufSize, numStreamsGMIO, devInst);
  return true;
#endif
}

} // namespace xdp
