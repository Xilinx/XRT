/**
 * Copyright (C) 2016-2019 Xilinx, Inc
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

#include "base_trace.h"

namespace xdp {
  //******************
  // Base Trace Writer
  //******************
  TraceWriterI::TraceWriterI()
  {
    // Reset previous values of device profile counters
    memset(&CountersPrev, 0, sizeof(xclCounterResults));
  }
  
  void TraceWriterI::openStream(std::ofstream& ofs, const std::string& fileName)
  {
    ofs.open(fileName);
    if (!ofs.is_open()) {
      throw std::runtime_error("Unable to open profile report for writing");
    }
  }

  // Write host function event to trace
  void TraceWriterI::writeFunction(double time, const std::string& functionName,
      const std::string& eventName, unsigned int functionID)
  {
    if (!Trace_ofs.is_open())
      return;

    std::stringstream timeStr;
    timeStr << std::setprecision(10) << time;

    writeTableRowStart(getStream());
    writeTableCells(getStream(), timeStr.str(), functionName, eventName,
        "", "", "", "", "", "", "", "", "", "", std::to_string(functionID));
    writeTableRowEnd(getStream());
  }

  // Write kernel event to trace
  void TraceWriterI::writeKernel(double traceTime, const std::string& commandString,
            const std::string& stageString, const std::string& eventString,
            const std::string& dependString, uint64_t objId, size_t size)
  {
    if (!Trace_ofs.is_open())
      return;

    std::stringstream timeStr;
    timeStr << std::setprecision(10) << traceTime;

    std::stringstream strObjId;
    strObjId << std::showbase << std::hex << std::uppercase << objId;

    writeTableRowStart(getStream());
    writeTableCells(getStream(), timeStr.str(), commandString,
        stageString, strObjId.str(), size, "", "", "", "", "", "",
        eventString, dependString);
    writeTableRowEnd(getStream());
  }

  // Write data transfer event to trace
  void TraceWriterI::writeTransfer(double traceTime, const std::string& commandString,
            const std::string& stageString, const std::string& eventString,
            const std::string& dependString, size_t size, uint64_t address,
            const std::string& bank, std::thread::id threadId)
  {
    if (!Trace_ofs.is_open())
      return;

    std::stringstream timeStr;
    timeStr << std::setprecision(10) << traceTime;

    // Write out DDR physical address and bank
    // NOTE: thread ID is only valid for START and END
    std::stringstream strAddress;
    strAddress << (boost::format("0X%09x") % address) << "|" << std::dec << bank;
    //strAddress << std::showbase << std::hex << std::uppercase << address
    //		   << "|" << std::dec << bank;
    if (stageString == "START" || stageString == "END")
      strAddress << "|" << std::showbase << std::hex << std::uppercase << threadId;

    writeTableRowStart(getStream());
    writeTableCells(getStream(), timeStr.str(), commandString,
        stageString, strAddress.str(), size, "", "", "", "", "", "",
        eventString, dependString);
    writeTableRowEnd(getStream());
  }

  // Write dependency event to trace
  void TraceWriterI::writeDependency(double traceTime, const std::string& commandString,
            const std::string& stageString, const std::string& eventString,
            const std::string& dependString)
  {
    if (!Trace_ofs.is_open())
      return;

    std::stringstream timeStr;
    timeStr << std::setprecision(10) << traceTime;

    writeTableRowStart(getStream());
    writeTableCells(getStream(), timeStr.str(), commandString,
        stageString, eventString, dependString);
    writeTableRowEnd(getStream());
  }

  // Functions for device counters
  void TraceWriterI::writeDeviceCounters(xclPerfMonType type, xclCounterResults& results,
      double timestamp, uint32_t sampleNum, bool firstReadAfterProgram)
  {
    if (!Trace_ofs.is_open())
      return;
    if (firstReadAfterProgram) {
      CountersPrev = results;
      return;
    }

    std::stringstream timeStr;
    timeStr << std::setprecision(10) << timestamp;

    // This version computes the avg. throughput and latency and writes those values

    static const char* slotNames[] = {
        XPAR_AXI_PERF_MON_0_SLOT0_NAME, XPAR_AXI_PERF_MON_0_SLOT1_NAME,
        XPAR_AXI_PERF_MON_0_SLOT2_NAME, XPAR_AXI_PERF_MON_0_SLOT3_NAME,
        XPAR_AXI_PERF_MON_0_SLOT4_NAME, XPAR_AXI_PERF_MON_0_SLOT5_NAME,
        XPAR_AXI_PERF_MON_0_SLOT6_NAME, XPAR_AXI_PERF_MON_0_SLOT7_NAME
    };

    uint32_t numSlots = XPAR_AXI_PERF_MON_0_NUMBER_SLOTS;
    //uint32_t numSlots = results.mNumSlots;

    for (unsigned int slot=0; slot < numSlots; slot++) {
      // Write
  #if 0
      double writeThputMBps = 0.0;
      if (results.SampleIntervalUsec > 0) {
        writeThputMBps = (results.WriteBytes[slot] - CountersPrev.WriteBytes[slot]) /
            results.SampleIntervalUsec;
      }

      std::stringstream writeThputCellStr;
      writeThputCellStr << std::setprecision(5) << writeThputMBps << " MBps";
  #else
      uint32_t writeBytes = results.WriteBytes[slot] - CountersPrev.WriteBytes[slot];
  #endif

      double writeLatency = 0.0;
      uint32_t numWriteTranx = results.WriteTranx[slot] - CountersPrev.WriteTranx[slot];
      if (numWriteTranx > 0) {
        writeLatency = (results.WriteLatency[slot] - CountersPrev.WriteLatency[slot]) /
            numWriteTranx;
      }

      // Don't report if no new transactions in this sample time window
      if (writeBytes != 0 || writeLatency != 0) {
        // TODO: the SPM we insert is not configured to report latency min/max
  #if 0
        std::stringstream writeLatencyCellStr;
        writeLatencyCellStr << results.WriteMinLatency[slot] << " / " << writeLatency << " / "
            << results.WriteMaxLatency[slot];

        writeTableRowStart(getStream());
        writeTableCells(getStream(), timeStr.str(), "Device Counters", "Write", slotNames[slot],
            writeBytes, writeLatencyCellStr.str(), "", "", "", "");
        writeTableRowEnd(getStream());
  #else
        writeTableRowStart(getStream());
        writeTableCells(getStream(), timeStr.str(), "Device Counters", "Write", slotNames[slot],
            writeBytes, writeLatency, "", "", "", "", "");
        writeTableRowEnd(getStream());
  #endif
      }

      // Read
  #if 0
      double readThputMBps = 0.0;
      if (results.SampleIntervalUsec > 0) {
        readThputMBps = (results.ReadBytes[slot] - CountersPrev.ReadBytes[slot]) /
            results.SampleIntervalUsec;
      }

      std::stringstream readThputCellStr;
      readThputCellStr << std::setprecision(5) << readThputMBps << " MBps";
  #else
      uint32_t readBytes = results.ReadBytes[slot] - CountersPrev.ReadBytes[slot];
  #endif

      double readLatency = 0.0;
      uint32_t numReadTranx = results.ReadTranx[slot] - CountersPrev.ReadTranx[slot];
      if (numReadTranx > 0) {
        readLatency = (results.ReadLatency[slot] - CountersPrev.ReadLatency[slot]) /
            numReadTranx;
      }

      // Don't report if no new transactions in this sample time window
      if (readBytes != 0 || readLatency != 0) {
        // TODO: the SPM we insert is not configured to support latency min/max
  #if 0
        std::stringstream readLatencyCellStr;
        readLatencyCellStr << results.ReadMinLatency[slot] << " / " << readLatency << " / "
            << results.ReadMaxLatency[slot];

        writeTableRowStart(getStream());
        writeTableCells(getStream(), timeStr.str(), "Device Counters", "Read", slotNames[slot],
            readBytes, readLatencyCellStr.str(), "", "", "", "");
        writeTableRowEnd(getStream());
  #else
        writeTableRowStart(getStream());
        writeTableCells(getStream(), timeStr.str(), "Device Counters", "Read", slotNames[slot],
            readBytes, readLatency, "", "", "", "");
        writeTableRowEnd(getStream());
  #endif
      }
    }

    CountersPrev = results;
  }

  // Functions for device trace
  void TraceWriterI::writeDeviceTrace(const TraceParser::TraceResultVector &resultVector,
      std::string deviceName, std::string binaryName)
  {
    if (!Trace_ofs.is_open())
      return;

    for (auto it = resultVector.begin(); it != resultVector.end(); it++) {
      DeviceTrace tr = *it;

#ifndef XDP_VERBOSE
      if (tr.Kind == DeviceTrace::DEVICE_BUFFER)
        continue;
#endif

      double deviceClockDurationUsec = (1.0 / (mPluginHandle->getKernelClockFreqMHz(deviceName)));

      std::stringstream startStr;
      startStr << std::setprecision(10) << tr.Start;
      std::stringstream endStr;
      endStr << std::setprecision(10) << tr.End;

      bool showKernelCUNames = true;
      bool showPortName = false;
      std::string memoryName;
      std::string traceName;
      std::string cuName;
      std::string argNames;

      // Populate trace name string
      if (tr.Kind == DeviceTrace::DEVICE_KERNEL) {
        if (tr.Type == "Kernel") {
          traceName = "KERNEL";
        } else if (tr.Type.find("Stall") != std::string::npos) {
          traceName = "Kernel_Stall";
          showPortName = false;
        } else if (tr.Type == "Write") {
          showPortName = true;
          traceName = "Kernel_Write";
        } else {
          showPortName = true;
          traceName = "Kernel_Read";
        }
      }
      else if (tr.Kind == DeviceTrace::DEVICE_STREAM) {
        traceName = tr.Name;
        showPortName = true;
      } else {
        showKernelCUNames = false;
        if (tr.Type == "Write")
          traceName = "Host_Write";
        else
          traceName = "Host_Read";
      }

      traceName += ("|" + deviceName + "|" + binaryName);

      if (showKernelCUNames || showPortName) {
        std::string portName;
        std::string cuPortName;
        if (tr.Kind == DeviceTrace::DEVICE_KERNEL && (tr.Type == "Kernel" || tr.Type.find("Stall") != std::string::npos)) {
          mPluginHandle->getProfileSlotName(XCL_PERF_MON_ACCEL, deviceName, tr.SlotNum, cuName);
        }
        else {
          if (tr.Kind == DeviceTrace::DEVICE_STREAM){
            mPluginHandle->getProfileSlotName(XCL_PERF_MON_STR, deviceName, tr.SlotNum, cuPortName);
          }
          else {
            mPluginHandle->getProfileSlotName(XCL_PERF_MON_MEMORY, deviceName, tr.SlotNum, cuPortName);
          }
          cuName = cuPortName.substr(0, cuPortName.find_first_of("/"));
          portName = cuPortName.substr(cuPortName.find_first_of("/")+1);
          //std::transform(portName.begin(), portName.end(), portName.begin(), ::tolower);
        }
        std::string kernelName;
        mPluginHandle->getProfileKernelName(deviceName, cuName, kernelName);

        if (showKernelCUNames)
          traceName += ("|" + kernelName + "|" + cuName);

        if (showPortName) {
          mPluginHandle->getArgumentsBank(deviceName, cuName, portName, argNames, memoryName);
          traceName += ("|" + portName + "|" + memoryName);
        }
      }

      if (tr.Type == "Kernel") {
        std::string workGroupSize;
        mPluginHandle->getTraceStringFromComputeUnit(deviceName, cuName, traceName);
        if (traceName.empty()) continue;
        size_t pos = traceName.find_last_of("|");
        workGroupSize = traceName.substr(pos + 1);
        traceName = traceName.substr(0, pos);
        
        writeTableRowStart(getStream());
        writeTableCells(getStream(), startStr.str(), traceName, "START", "", workGroupSize);
        writeTableRowEnd(getStream());

        writeTableRowStart(getStream());
        writeTableCells(getStream(), endStr.str(), traceName, "END", "", workGroupSize);
        writeTableRowEnd(getStream());
        continue;
      }

      double deviceDuration = 1000.0*(tr.End - tr.Start);
      if (!(deviceDuration > 0.0)) deviceDuration = deviceClockDurationUsec;
      writeTableRowStart(getStream());
      writeTableCells(getStream(), startStr.str(), traceName,
          tr.Type, argNames, tr.BurstLength, (tr.EndTime - tr.StartTime),
          tr.StartTime, tr.EndTime, deviceDuration,
          startStr.str(), endStr.str());
      writeTableRowEnd(getStream());
    }
  }

} // xdp
