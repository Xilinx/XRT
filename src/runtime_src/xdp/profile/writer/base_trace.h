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

// Copyright 2014 Xilinx, Inc. All rights reserved.
#ifndef __XDP_BASE_TRACE_WRITER_H
#define __XDP_BASE_TRACE_WRITER_H

#include <boost/format.hpp>
#include "../device/trace_parser.h"
#include "xdp/profile/plugin/base_plugin.h"

#include <cstdlib>
#include <cstdio>
#include <string>
#include <chrono>
#include <cstring>
#include <iostream>
#include <map>

// Use this class to build run time user services functions
// such as debugging and profiling

namespace xdp {
    // Writer interface for generating trace data
    class TraceWriterI {

    public:
      TraceWriterI();
	  virtual ~TraceWriterI() {};

    public:
	    // Functions for timeline trace log
	    // Write timeline trace of a function call such as cl API call
	    void writeFunction(double time, const std::string& functionName,
	        const std::string& eventName, unsigned int functionID);
	    // Write timeline trace of kernel execution
	    void writeKernel(double traceTime, const std::string& commandString,
            const std::string& stageString, const std::string& eventString,
            const std::string& dependString, uint64_t objId, size_t size);
	    // Write timeline trace of read/write data transfer
	    void writeTransfer(double traceTime, const std::string& commandString,
            const std::string& stageString, const std::string& eventString,
            const std::string& dependString, size_t size, uint64_t address,
            const std::string& bank, std::thread::id threadId);
	    // Write timeline trace of dependency
	    void writeDependency(double traceTime, const std::string& commandString,
            const std::string& stageString, const std::string& eventString,
            const std::string& dependString);

	    // Write device counters
	    void writeDeviceCounters(xclPerfMonType type, xclCounterResults& results,
		      double timestamp, uint32_t sampleNum, bool firstReadAfterProgram);
	    // Write device trace
	    void writeDeviceTrace(const TraceParser::TraceResultVector &resultVector,
	          std::string deviceName, std::string binaryName);

    protected:
      // Variadic args function to take n number of any type of args and
      // stream it to a file
      // TODO: Windows doesn't support variadic functions till VS 2013.
      template<typename T>
      void writeTableCells(std::ofstream& ofs, T value)
      {
        ofs << cellStart();
        ofs << value;
        ofs << cellEnd();
      }

      template<typename T, typename... Args>
      void writeTableCells(std::ofstream& ofs, T first, Args... args)
      {
        writeTableCells(ofs, first);
        writeTableCells(ofs, args...);
      }

	protected:
	    void openStream(std::ofstream& ofs, const std::string& fileName);
	    std::ofstream& getStream(){return Trace_ofs;}
	    
	protected:
	    // Document is assumed to consist of document header and document footer
	    // A binary writer such as write to Xilinx's WDB format (say WDBWriter class) could be
	    // derived from TraceWriterI. The writing of such a file can still be organized in following steps.
	    // Any additional intelligence such as streaming compression can be built in the WDBWriter
	    virtual void writeDocumentHeader(std::ofstream& ofs, const std::string& docName)  { ofs << docName;}
	    virtual void writeTableHeader(std::ofstream& ofs, const std::string& caption,
	        const std::vector<std::string>& columnLabels) = 0;
	    virtual void writeTableRowStart(std::ofstream& ofs) { ofs << rowStart(); }
	    virtual void writeTableRowEnd(std::ofstream& ofs)   { ofs << rowEnd() << newLine(); }
	    virtual void writeDocumentFooter(std::ofstream& ofs) {}

	    // Cell and Row marking tokens
        virtual const char* cellStart()  { return ""; }
        virtual const char* cellEnd()  { return ""; }
        virtual const char* rowStart() { return ""; }
        virtual const char* rowEnd()  { return ""; }
        virtual const char* newLine()  { return "\n"; }

	protected:
	    std::ofstream Trace_ofs;
	    xclCounterResults CountersPrev;

    protected:
      XDPPluginI * mPluginHandle;
    };

} // xdp

#endif
