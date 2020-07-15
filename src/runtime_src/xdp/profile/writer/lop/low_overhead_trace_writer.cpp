/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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
		   "1.0", 
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

  void LowOverheadTraceWriter::writeHumanReadableHeader()
  {
    VPTraceWriter::writeHeader() ;
    fout << "XRT Version," << getToolVersion() << std::endl ;
  }

  void LowOverheadTraceWriter::writeHumanReadableStructure()
  {
    fout << "STRUCTURE" << std::endl ;
    fout << "Group_Start,Host APIs" << std::endl ;
    fout << "Group_Start,OpenCL API Calls" << std::endl ;
    fout << "Dynamic_Row," << generalAPIBucket
	 << ",General,API Events not associated with a Queue" << std::endl ;

    for (auto a : (db->getStaticInfo()).getCommandQueueAddresses())
    {
      fout << "Static_Row," << commandQueueToBucket[a] << ",Queue 0x" 
	   << std::hex << a << ",API events associated with the command queue"
	   << std::dec << std::endl ;
    }
    fout << "Group_End,OpenCL API Calls" << std::endl ;
    fout << "Group_Start,Data Transfer" << std::endl ;
    fout << "Dynamic_Row," << readBucket 
	 << ",Read,Read data transfers from global memory to host" 
	 << std::endl ;
    fout << "Dynamic_Row," << writeBucket
	 << ",Write,Write data transfer from host to global memory"
	 << std::endl ;
    fout << "Group_End,Data Transfer" << std::endl ;
    fout << "Dynamic_Row_Summary," << enqueueBucket 
	 << ",Kernel Enqueues,Activity in kernel enqueues" << std::endl ;
    fout << "Group_End,Host APIs" << std::endl ;
  }

  void LowOverheadTraceWriter::writeHumanReadableStringTable()
  {
    fout << "MAPPING" << std::endl ;
    (db->getDynamicInfo()).dumpStringTable(fout) ;
  }

  void LowOverheadTraceWriter::writeHumanReadableTraceEvents()
  {
    fout << "EVENTS" << std::endl ;
    std::vector<VTFEvent*> APIEvents = 
      (db->getDynamicInfo()).filterEvents( [](VTFEvent* e)
					   {
					     return e->isOpenCLAPI() ||
					            e->isLOPHostEvent() ;
					   }
					 ) ;
    for (auto e : APIEvents)
    {
      int bucket = 0 ;
      if (e->isOpenCLAPI() && (dynamic_cast<OpenCLAPICall*>(e) != nullptr))
      {
	bucket = commandQueueToBucket[dynamic_cast<OpenCLAPICall*>(e)->getQueueAddress()] ;
	// If there was no command queue, put it in the general bucket
	if (bucket == 0) bucket = generalAPIBucket ;
      }
      else if (e->isReadBuffer())
      {
	bucket = readBucket ;
      }
      else if (e->isWriteBuffer())
      {
	bucket = writeBucket ;
      }
      else if (e->isKernelEnqueue())
      {
	bucket = enqueueBucket ;
      }
      e->dump(fout, bucket) ;
    }
  }

  void LowOverheadTraceWriter::writeHumanReadableDependencies()
  {
    fout << "DEPENDENCIES" << std::endl ;
    // No dependencies in low overhead profiling
  }

  // ************** Binary output functions ******************

  void LowOverheadTraceWriter::writeBinaryHeader()
  {
  }

  void LowOverheadTraceWriter::writeBinaryStructure()
  {
  }

  void LowOverheadTraceWriter::writeBinaryStringTable()
  {
  }

  void LowOverheadTraceWriter::writeBinaryTraceEvents()
  {
  }

  void LowOverheadTraceWriter::writeBinaryDependencies()
  {
  }

  // ************** Virtual output functions ******************

  void LowOverheadTraceWriter::writeHeader()
  {
    if (humanReadable) writeHumanReadableHeader() ;
    else               writeBinaryHeader() ;
  }

  void LowOverheadTraceWriter::writeStructure()
  {
    if (humanReadable) writeHumanReadableStructure() ;
    else               writeBinaryStructure() ;
  }

  void LowOverheadTraceWriter::writeStringTable()
  {
    if (humanReadable) writeHumanReadableStringTable() ;
    else               writeBinaryStringTable() ;
  }

  void LowOverheadTraceWriter::writeTraceEvents()
  {
    if (humanReadable) writeHumanReadableTraceEvents() ;
    else               writeBinaryTraceEvents() ;
  }

  void LowOverheadTraceWriter::writeDependencies()
  {
    if (humanReadable) writeHumanReadableDependencies() ;
    else               writeBinaryDependencies() ;
  }

  void LowOverheadTraceWriter::write(bool openNewFile)
  {
    // Before writing, set up our information for structures
    setupBuckets() ;
    //setupCommandQueueBuckets() ;

    writeHeader() ;
    if (humanReadable) fout << std::endl ;
    writeStructure() ;
    if (humanReadable) fout << std::endl ;
    writeStringTable() ;
    if (humanReadable) fout << std::endl ;
    writeTraceEvents() ;
    if (humanReadable) fout << std::endl ;
    writeDependencies() ;
    if (humanReadable) fout << std::endl ;

    if (openNewFile) switchFiles() ;
  }


}
