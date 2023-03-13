/**
 * Copyright (C) 2016-2020 Xilinx, Inc
 * Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
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

#include "xdp/profile/writer/opencl/opencl_trace_writer.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/dynamic_event_database.h"

#include "xdp/profile/database/events/opencl_api_calls.h"
#include "xdp/profile/database/events/opencl_host_events.h"

#include "xdp/profile/plugin/vp_base/utility.h"

namespace xdp {

  OpenCLTraceWriter::OpenCLTraceWriter(const char* filename) :
    VPTraceWriter(filename,
                   "1.1",
                   getCurrentDateTime(),
                   9 /* ns */),
    generalAPIBucket(-1), readBucket(-1), writeBucket(-1), copyBucket(-1)
  {
  }

  OpenCLTraceWriter::~OpenCLTraceWriter()
  {
  }

  void OpenCLTraceWriter::setupBuckets()
  {
    int rowID = 1 ;
    generalAPIBucket = rowID ;
    ++rowID ;
    for (auto a : (db->getStaticInfo()).getCommandQueueAddresses())
    {
      commandQueueToBucket[a] = rowID ;
      ++rowID ;
    }
    readBucket = rowID ;
    ++rowID ;
    writeBucket = rowID ;
    ++rowID ;
    copyBucket = rowID ;
    ++rowID ;
    for (auto& e : (db->getStaticInfo()).getEnqueuedKernels())
    {
      enqueueBuckets[e] = rowID ;
      ++rowID ;
    }
  }

  // ************** Human Readable output functions ******************

  void OpenCLTraceWriter::writeHeader()
  {
    VPTraceWriter::writeHeader() ;
    fout << "TraceID," << traceID << "\n"
         << "XRT Version," << getToolVersion() << "\n";
  }

  void OpenCLTraceWriter::writeStructure()
  {
    fout << "STRUCTURE\n";
    fout << "Group_Start,OpenCL Host Trace\n";
    fout << "Group_Start,OpenCL API Calls\n";
    fout << "Dynamic_Row," << generalAPIBucket
         << ",General,API Events not associated with a Queue\n";

    for (auto a : (db->getStaticInfo()).getCommandQueueAddresses()) {
      fout << "Static_Row," << commandQueueToBucket[a] << ",Queue 0x" 
           << std::hex << a << ",API events associated with the command queue\n"
           << std::dec;
    }
    fout << "Group_End,OpenCL API Calls\n";
    fout << "Group_Start,Data Transfer\n";
    fout << "Dynamic_Row," << readBucket 
         << ",Read,Read data transfers from global memory to host\n";
    fout << "Dynamic_Row," << writeBucket
         << ",Write,Write data transfer from host to global memory\n";
    fout << "Dynamic_Row," << copyBucket
         << ",Copy,Copy data transfers from global memory to global memory\n";
    fout << "Group_End,Data Transfer\n";
    fout << "Group_Start,Kernel Enqueues\n";

    for (auto& b : enqueueBuckets) {
      fout << "Dynamic_Row_Summary," << b.second << "," << b.first 
           << ",Kernel Enqueue\n";
    }
    fout << "Group_End,Kernel Enqueues\n";
    fout << "Group_End,OpenCL Host Trace\n";
  }

  void OpenCLTraceWriter::writeStringTable()
  {
    fout << "MAPPING\n";
    (db->getDynamicInfo()).dumpStringTable(fout) ;
  }

  void OpenCLTraceWriter::writeTraceEvents()
  {
    fout << "EVENTS\n";
    auto APIEvents = 
      (db->getDynamicInfo()).moveSortedHostEvents( [](VTFEvent* e)
                                                   {
                                                     return e->isOpenCLHostEvent();
                                                   }
                                                 );
    for (auto& e : APIEvents) {
      int bucket = 0 ;
      if (e->isOpenCLAPI() && (dynamic_cast<OpenCLAPICall*>(e.get()) != nullptr)) {
        bucket = commandQueueToBucket[dynamic_cast<OpenCLAPICall*>(e.get())->getQueueAddress()] ;
        // If there was no command queue, put it in the general bucket
        if (bucket == 0)
          bucket = generalAPIBucket ;
      }
      else if (e->isReadBuffer())
        bucket = readBucket ;
      else if (e->isWriteBuffer())
        bucket = writeBucket ;
      else if (e->isCopyBuffer())
        bucket = copyBucket ;
      else if (e->isKernelEnqueue()) {
        // Construct the name
        KernelEnqueue* ke = dynamic_cast<KernelEnqueue*>(e.get()) ;
        if (ke != nullptr)
          bucket = enqueueBuckets[ke->getIdentifier()] ;
        else
          bucket = generalAPIBucket; // Should never happen
      }
      e->dump(fout, bucket) ;
    }
  }

  void OpenCLTraceWriter::writeDependencies()
  {
    fout << "DEPENDENCIES\n";
    std::map<uint64_t, std::vector<uint64_t>> dependencies = 
      (db->getDynamicInfo()).getDependencyMap() ;

    collapseDependencyChains(dependencies) ;

    for (auto& dependency : dependencies) {
      for (auto dependent : dependency.second) {
        // We have logged all of the dependencies of XRT side events.
        //  There is the possibility that these events don't correspond to
        //  any XDP event that we have logged.  We should just ignore those
        //  dependencies.
        std::pair<uint64_t, uint64_t> firstValue =
          (db->getDynamicInfo()).lookupOpenCLMapping(dependency.first) ;
        std::pair<uint64_t, uint64_t> secondValue =
          (db->getDynamicInfo()).lookupOpenCLMapping(dependent) ;

        // We are specifying where arrows appear in the final visualization
        //  between two transactions.  There are four separate events,
        //  we need to output the end event ID of the first transaction
        //  followed by the start event ID of the second transaction.
        if (firstValue.second != 0 && secondValue.first != 0)
          fout << secondValue.first << "," << firstValue.second << "\n" ;
      }
    }
  }

  void OpenCLTraceWriter::collapseDependencyChains(std::map<uint64_t, std::vector<uint64_t>>& dependencies)
  {
    // The purpose of this function is to collapse dependency chains such
    //  as 3->4 and 4->5 to 3->5 when we do not have any mapping for 4

    std::pair<uint64_t, uint64_t> zero = std::make_pair(0, 0) ;

    for (auto& iter : dependencies) {
      auto xrtID = iter.first ;

      std::pair<uint64_t, uint64_t> mapping =
        (db->getDynamicInfo()).lookupOpenCLMapping(xrtID) ;
      if (mapping == zero)
        continue ;

      // If we are here, then we know the first ID does have a mapping to XDP
      for (auto dependentID : iter.second) {
        std::pair<uint64_t, uint64_t> depMapping =
          (db->getDynamicInfo()).lookupOpenCLMapping(dependentID) ;
        if (depMapping == zero) {
          for (auto chainedID : dependencies[dependentID]) {
            dependencies[xrtID].push_back(chainedID) ;
          }
        }
      }
    }
  }

  bool OpenCLTraceWriter::traceEventsExist()
  {
    return (
      db->getDynamicInfo().hostEventsExist (
        [](VTFEvent* e)
        {
          return e->isOpenCLHostEvent();
        }
      )
    );
  }

  bool OpenCLTraceWriter::write(bool openNewFile)
  {
    if (openNewFile && !traceEventsExist())
      return false;

    // Before writing, set up our information for structures
    setupBuckets() ;
    //setupCommandQueueBuckets() ;

    writeHeader() ;
    fout << "\n";
    writeStructure() ;
    fout << "\n";
    writeStringTable() ;
    fout << "\n";
    writeTraceEvents() ;
    fout << "\n";
    writeDependencies() ;
    fout << "\n";

    fout.flush();

    if (openNewFile)
      switchFiles() ;

    return true;
  }

}
