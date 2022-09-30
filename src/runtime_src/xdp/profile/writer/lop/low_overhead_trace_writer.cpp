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

#include "xdp/profile/writer/lop/low_overhead_trace_writer.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/dynamic_event_database.h"

#include "xdp/profile/database/events/opencl_api_calls.h"

#include "xdp/profile/plugin/vp_base/utility.h"

namespace xdp {

  LowOverheadTraceWriter::LowOverheadTraceWriter(const char* filename) :
    VPTraceWriter(filename,
                   "1.1",
                   getCurrentDateTime(),
                   9 /* ns */),
    generalAPIBucket(-1), readBucket(-1), writeBucket(-1), enqueueBucket(-1)
  {
  }

  LowOverheadTraceWriter::~LowOverheadTraceWriter()
  {
  }

  void LowOverheadTraceWriter::setupBuckets()
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
    enqueueBucket = rowID ;
  }

  // ************** Human Readable output functions ******************

  void LowOverheadTraceWriter::writeHeader()
  {
    VPTraceWriter::writeHeader() ;
    fout << "TraceID," << traceID << "\n"
         << "XRT Version," << getToolVersion() << "\n";
  }

  void LowOverheadTraceWriter::writeStructure()
  {
    fout << "STRUCTURE\n";
    fout << "Group_Start,Low Overhead OpenCL Host Trace\n";
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
    fout << "Group_End,Data Transfer\n";
    fout << "Dynamic_Row_Summary," << enqueueBucket 
         << ",Kernel Enqueues,Activity in kernel enqueues\n";
    fout << "Group_End,Low Overhead OpenCL Host Trace\n";
  }

  void LowOverheadTraceWriter::writeStringTable()
  {
    fout << "MAPPING\n";
    (db->getDynamicInfo()).dumpStringTable(fout) ;
  }

  void LowOverheadTraceWriter::writeTraceEvents()
  {
    fout << "EVENTS\n";
    auto APIEvents = 
      (db->getDynamicInfo()).moveSortedHostEvents( [](VTFEvent* e)
                                                   {
                                                     return e->isLOPAPI() ||
                                                            e->isLOPHostEvent();
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
      else if (e->isKernelEnqueue())
        bucket = enqueueBucket ;

      e->dump(fout, bucket) ;
    }
  }

  void LowOverheadTraceWriter::writeDependencies()
  {
    fout << "DEPENDENCIES\n";
    // No dependencies in low overhead profiling
  }

  bool LowOverheadTraceWriter::traceEventsExist()
  {
    return (
      db->getDynamicInfo().hostEventsExist (
        [](VTFEvent* e)
        {
          return e->isOpenCLAPI() ||
                 e->isLOPHostEvent() ;
        }
      )
    );
  }

  bool LowOverheadTraceWriter::write(bool openNewFile)
  {
    if (openNewFile && !traceEventsExist())
      return false;

    // Before writing, set up our information for structures
    setupBuckets() ;
    //setupCommandQueueBuckets() ;

    writeHeader() ;
    fout << "\n";
    writeStructure() ;
    fout << "\n" ;
    writeStringTable() ;
    fout << "\n" ;
    writeTraceEvents() ;
    fout << "\n" ;
    writeDependencies() ;
    fout << "\n" ;

    fout.flush();

    if (openNewFile)
      switchFiles() ;
    return true;
  }


}
