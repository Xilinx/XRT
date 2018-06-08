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
//
// This file contains confidential and proprietary information
// of Xilinx, Inc. and is protected under U.S. and
// international copyright and other intellectual property
// laws.
//
// DISCLAIMER
// This disclaimer is not a license and does not grant any
// rights to the materials distributed herewith. Except as
// otherwise provided in a valid license issued to you by
// Xilinx, and to the maximum extent permitted by applicable
// law: (1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND
// WITH ALL FAULTS, AND XILINX HEREBY DISCLAIMS ALL WARRANTIES
// AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING
// BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, NON-
// INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE; and
// (2) Xilinx shall not be liable (whether in contract or tort,
// including negligence, or under any other theory of
// liability) for any loss or damage of any kind or nature
// related to, arising under or in connection with these
// materials, including for any direct, or any indirect,
// special, incidental, or consequential loss or damage
// (including loss of data, profits, goodwill, or any type of
// loss or damage suffered as a result of any action brought
// by a third party) even if such damage or loss was
// reasonably foreseeable or Xilinx had been advised of the
// possibility of the same.
//
// CRITICAL APPLICATIONS
// Xilinx products are not designed or intended to be fail-
// safe, or for use in any application requiring fail-safe
// performance, such as life-support or safety devices or
// systems, Class III medical devices, nuclear facilities,
// applications related to the deployment of airbags, or any
// other applications that could lead to death, personal
// injury, or severe property or environmental damage
// (individually and collectively, "Critical
// Applications"). Customer assumes the sole risk and
// liability of any use of Xilinx products in Critical
// Applications, subject only to applicable laws and
// regulations governing limitations on product liability.
//
// THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS
// PART OF THIS FILE AT ALL TIMES.

#ifndef _WINDOWS
// TODO: Windows build support
// This seems to be linux only file

#ifndef __XCLHOST_UNIXSOCKET__
#define __XCLHOST_UNIXSOCKET__
#include <iostream>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <cstring>
#include <sstream>
#include <list>
#include <map>
#include <tuple>
#include <vector>
#include "xcl_macros.h"
#ifdef USE_HAL2
#include "xclhal2.h"
#else
#include "xclhal.h"
#endif
#include "xclbin.h"
#include "xbar_sys_parameters.h"
#ifdef _MSC_VER
#include <boost/config/compiler/visualc.hpp>
#endif
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/foreach.hpp>

namespace systemUtil {
  
  enum systemOperation 
  {
    CREATE      = 0,
    REMOVE      = 1,
    COPY        = 2,
    APPEND      = 3,
    UNZIP       = 4,
    PERMISSIONS = 5
  };

  void makeSystemCall(std::string &operand1, systemOperation operation, std::string operand2 = "");

}

namespace xclemulation{
  //this class has only one member now. This will be extended to use all the parameters specific to each ddr.
  class DDRBank 
  {
    public:
      uint64_t ddrSize;
      DDRBank();
  };

  enum LAUNCHWAVEFORM {
    OFF,
    BATCH,
    GUI
  };

  class config 
  {
    public:
      static config* getInstance();    //get the instance of singleton class
      static void destroy();           //destruct the class.
      
      inline void enableDiagnostics(bool diagnostics)           { mDiagnostics      = diagnostics ;  } 
      inline void enableUMRChecks(bool umrChecks)               { mUMRChecks        = umrChecks;     }
      inline void enableOOBChecks(bool oobChecks)               { mOOBChecks        = oobChecks;     }
      inline void enableMemLogs (bool memLogs)                  { mMemLogs          = memLogs;       }
      inline void setDontRun( bool dontRun)                     { mDontRun          = dontRun;       }
      inline void setPacketSize( unsigned int packetSize)       { mPacketSize       = packetSize;    }
      inline void setMaxTraceCount( unsigned int maxTraceCount) { mMaxTraceCount    = maxTraceCount; }
      inline void setPaddingFactor( unsigned int paddingFactor) { mPaddingFactor    = paddingFactor; }
      inline void setSimDir( std::string& simDir)               { mSimDir           = simDir;        }
      inline void setLaunchWaveform( LAUNCHWAVEFORM lWaveform)  { mLaunchWaveform   = lWaveform;     }
      inline void suppressInfo( bool suppress)                  { mSuppressInfo     = suppress;      }
      inline void suppressWarnings( bool suppress)              { mSuppressWarnings = suppress;      }
      inline void suppressErrors( bool suppress)                { mSuppressErrors   = suppress;      }
      inline void printInfosInConsole( bool _print)             { mPrintInfosInConsole    = _print;  }
      inline void printWarningsInConsole( bool _print)          { mPrintWarningsInConsole = _print;  }
      inline void printErrorsInConsole( bool _print)            { mPrintErrorsInConsole   = _print;  }
      inline void setVerbosityLevel(unsigned int verbosity)     { mVerbosity        = verbosity;     }
      inline void setServerPort(unsigned int serverPort)        { mServerPort       = serverPort;    }
      inline void setKeepRunDir(bool _mKeepRundir)              { mKeepRunDir = _mKeepRundir;        }    
      
      inline bool isDiagnosticsEnabled()        const { return mDiagnostics;    }
      inline bool isUMRChecksEnabled()          const { return mUMRChecks;      }
      inline bool isOOBChecksEnabled()          const { return mOOBChecks;      }
      inline bool isMemLogsEnabled()            const { return mMemLogs;        }
      inline bool isDontRun()                   const { return mDontRun;        }
      inline unsigned int getPacketSize()       const { return mPacketSize;     }
      inline unsigned int getMaxTraceCount()    const { return mMaxTraceCount;  }
      inline unsigned int getPaddingFactor()    const { if(!mOOBChecks) return 0; return mPaddingFactor;  }
      inline std::string getSimDir()            const { return mSimDir;         }
      inline LAUNCHWAVEFORM getLaunchWaveform() const { return mLaunchWaveform; }
      inline bool isInfoSuppressed()            const { return mSuppressInfo;    }
      inline bool isWarningsuppressed()         const { return mSuppressWarnings;}
      inline bool isErrorsSuppressed()          const { return mSuppressErrors;  }
      inline bool isInfosToBePrintedOnConsole()    const { return mPrintInfosInConsole;   }  
      inline bool isWarningsToBePrintedOnConsole() const { return mPrintWarningsInConsole;}
      inline bool isErrorsToBePrintedOnConsole()   const { return mPrintErrorsInConsole;  }
      inline bool getVerbosityLevel()           const { return mVerbosity;       }    
      inline unsigned int getServerPort()             { return mServerPort;      }
      inline bool isKeepRunDirEnabled()           const { return mKeepRunDir;       }    
      
      void populateEnvironmentSetup(std::map<std::string,std::string>& mEnvironmentNameValueMap);

    private:
      static config* mInst;         // (singleton)instance of this class
      bool mDiagnostics;
      bool mUMRChecks;
      bool mOOBChecks;
      bool mMemLogs;
      bool mDontRun;
      LAUNCHWAVEFORM mLaunchWaveform;
      std::string mSimDir;
      unsigned int mPacketSize;
      unsigned int mMaxTraceCount;
      unsigned int mPaddingFactor;
      bool mSuppressInfo;
      bool mSuppressWarnings;
      bool mSuppressErrors;
      bool mPrintInfosInConsole;
      bool mPrintWarningsInConsole;
      bool mPrintErrorsInConsole;
      bool mVerbosity;
      unsigned int mServerPort;
      bool mKeepRunDir;
      
      config()  { mDiagnostics = true; mUMRChecks = false; mOOBChecks = false; mMemLogs = false;  mLaunchWaveform = LAUNCHWAVEFORM::OFF; mDontRun = false; mSimDir = ""; mPacketSize = 0x800000; mMaxTraceCount = 1; mPaddingFactor = 1; mSuppressInfo = false ; mSuppressWarnings = false; mSuppressErrors = false; mPrintInfosInConsole = true; mPrintWarningsInConsole = true; mPrintErrorsInConsole = true; mVerbosity = 0; mServerPort = 0; mKeepRunDir=false; };
      ~config() { };//empty destructor
  };

  void getDevicesInfo(std::vector<std::tuple<xclDeviceInfo2,std::list<DDRBank> ,bool, bool> >& devicesInfo);
  bool copyLogsFromOneFileToAnother(const std::string &logFile, std::ofstream &ofs);
  std::string getEmDebugLogFile();
  bool isXclEmulationModeHwEmuOrSwEmu();
  std::string getRunDirectory();
  
  std::map<std::string,std::string> getEnvironmentByReadingIni();
}

class unix_socket {
  private:
    int fd;
    void start_server(const std::string sk_desc);
    std::string name;
public:
    bool server_started;
    void set_name(std::string &sock_name) { name = sock_name;}
    std::string get_name() { return name;} 
    unix_socket();
    ~unix_socket()
    {
       server_started = false;
       close(fd);
    }
    size_t sk_write(const void *wbuf, size_t count);
    size_t sk_read(void *rbuf, size_t count);
};


#endif

#endif


