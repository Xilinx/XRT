#ifndef __EM_CONFIG_READER__
#define __EM_CONFIG_READER__

#include <cstring>
#include <sstream>
#include <list>
#include <map>
#include <tuple>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _MSC_VER
#include <boost/config/compiler/visualc.hpp>
#endif
#include <boost/algorithm/string.hpp>
#include <boost/locale.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/foreach.hpp>

#include "xbar_sys_parameters.h"
#include "xclhal2.h"
#include "xclfeatures.h"
#include "xclbin.h"
#include <chrono>

namespace xclemulation{

  // KB
  const uint64_t MEMSIZE_1K   =   0x0000000000000400;
  const uint64_t MEMSIZE_4K   =   0x0000000000001000;
  const uint64_t MEMSIZE_8K   =   0x0000000000002000;
  const uint64_t MEMSIZE_16K  =   0x0000000000004000;
  const uint64_t MEMSIZE_32K  =   0x0000000000008000;
  const uint64_t MEMSIZE_64K  =   0x0000000000010000;
  const uint64_t MEMSIZE_128K =   0x0000000000020000;
  const uint64_t MEMSIZE_256K =   0x0000000000040000;
  const uint64_t MEMSIZE_512K =   0x0000000000080000;

  // MB
  const uint64_t MEMSIZE_1M   =   0x0000000000100000;
  const uint64_t MEMSIZE_2M   =   0x0000000000200000;
  const uint64_t MEMSIZE_4M   =   0x0000000000400000;
  const uint64_t MEMSIZE_8M   =   0x0000000000800000;
  const uint64_t MEMSIZE_16M  =   0x0000000001000000;
  const uint64_t MEMSIZE_32M  =   0x0000000002000000;
  const uint64_t MEMSIZE_64M  =   0x0000000004000000;
  const uint64_t MEMSIZE_128M =   0x0000000008000000;
  const uint64_t MEMSIZE_256M =   0x0000000010000000;
  const uint64_t MEMSIZE_512M =   0x0000000020000000;

  // GB
  const uint64_t MEMSIZE_1G   =   0x0000000040000000;
  const uint64_t MEMSIZE_2G   =   0x0000000080000000;
  const uint64_t MEMSIZE_4G   =   0x0000000100000000;
  const uint64_t MEMSIZE_8G   =   0x0000000200000000;
  const uint64_t MEMSIZE_16G  =   0x0000000400000000;
  const uint64_t MEMSIZE_32G  =   0x0000000800000000;
  const uint64_t MEMSIZE_64G  =   0x0000001000000000;
  const uint64_t MEMSIZE_128G =   0x0000002000000000;
  const uint64_t MEMSIZE_256G =   0x0000004000000000;
  const uint64_t MEMSIZE_512G =   0x0000008000000000;

  // TB
  const uint64_t MEMSIZE_1T   =   0x0000010000000000;
  const uint64_t MEMSIZE_2T   =   0x0000020000000000;
  const uint64_t MEMSIZE_4T   =   0x0000040000000000;
  const uint64_t MEMSIZE_8T   =   0x0000080000000000;
  const uint64_t MEMSIZE_16T  =   0x0000100000000000;
  const uint64_t MEMSIZE_32T  =   0x0000200000000000;
  const uint64_t MEMSIZE_64T  =   0x0000400000000000;
  const uint64_t MEMSIZE_128T =   0x0000800000000000;
  const uint64_t MEMSIZE_256T =   0x0001000000000000;
  const uint64_t MEMSIZE_512T =   0x0002000000000000;
  
  //For Profiling Offsets
  const uint64_t FIFO_INFO_MESSAGES     = 0x0000000000100000;
  const uint64_t FIFO_WARNING_MESSAGES  = 0x0000000000200000;
  const uint64_t FIFO_ERROR_MESSAGES    = 0x0000000000400000;
  const uint64_t FIFO_CTRL_INFO_SIZE    = 0x64;
  const uint64_t FIFO_CTRL_WARNING_SIZE = 0x68;
  const uint64_t FIFO_CTRL_ERROR_SIZE   = 0x6C;
 
  const int VIVADO_MIN_VERSION = 2000;
  const int VIVADO_MAX_VERSION = 2100;

  //this class has only one member now. This will be extended to use all the parameters specific to each ddr.
  class DDRBank 
  {
    public:
      uint64_t ddrSize;
      DDRBank();
  };
  enum TIMEOUT_SCALE {
	NA,
	MS,
	SEC,
	MIN
  };
  class ApiWatchdog {
  private:
	  TIMEOUT_SCALE timeout_scale;
	  std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
	  std::chrono::time_point<std::chrono::high_resolution_clock> end_time;
	  bool disabled;
	  unsigned long timeout_period;
  public:
	ApiWatchdog(TIMEOUT_SCALE scale,unsigned long timeout) {
		  timeout_period=timeout;
		  timeout_scale=scale;
		  if (timeout_scale==TIMEOUT_SCALE::MIN) {
			  timeout_period=timeout*60;
		  } else if (TIMEOUT_SCALE::MS) {
			  timeout_period=timeout/1000;
		  }
		  if (timeout_scale==TIMEOUT_SCALE::NA) {
			  disabled=true;
		  } else {
			  disabled=false;
		  }
	}
	bool isTimeout() {
		end_time=std::chrono::high_resolution_clock::now();
		if (!disabled && (std::chrono::duration<double>(end_time - start_time).count() > timeout_period)){
			return true;
		} else {
			return false;
		}
	}
	void reset() {
		start_time=std::chrono::high_resolution_clock::now();
	}
	bool isDisabled() {
		return disabled;
	}
  };

  enum DEBUG_MODE {
    OFF,
    BATCH,
    GUI,
    GDB
  };
  
  enum ERTMODE {
    NONE,
    LEGACY,
    UPDATED 
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
      inline void setUserPreSimScript( std::string& userPreSimScript) {mUserPreSimScript = userPreSimScript; }
	    inline void setUserPostSimScript( std::string& userPostSimScript) {mUserPostSimScript = userPostSimScript; }
      inline void setLaunchWaveform( DEBUG_MODE lWaveform)  { mLaunchWaveform   = lWaveform;     }
      inline void suppressInfo( bool suppress)                  { mSuppressInfo     = suppress;      }
      inline void suppressWarnings( bool suppress)              { mSuppressWarnings = suppress;      }
      inline void suppressErrors( bool suppress)                { mSuppressErrors   = suppress;      }
      inline void printInfosInConsole( bool _print)             { mPrintInfosInConsole    = _print;  }
      inline void printWarningsInConsole( bool _print)          { mPrintWarningsInConsole = _print;  }
      inline void printErrorsInConsole( bool _print)            { mPrintErrorsInConsole   = _print;  }
      inline void setVerbosityLevel(unsigned int verbosity)     { mVerbosity        = verbosity;     }
      inline void setServerPort(unsigned int serverPort)        { mServerPort       = serverPort;    }
      inline void setKeepRunDir(bool _mKeepRundir)              { mKeepRunDir = _mKeepRundir;        }    
      inline void setLauncherArgs(std::string & _mLauncherArgs) { mLauncherArgs = _mLauncherArgs;    }
      inline void setSystemDPA(bool _isDPAEnabled)              { mSystemDPA    = _isDPAEnabled;     }
      inline void setLegacyErt(ERTMODE _legacyErt)              { mLegacyErt    = _legacyErt;        }
      
      inline bool isDiagnosticsEnabled()        const { return mDiagnostics;    }
      inline bool isUMRChecksEnabled()          const { return mUMRChecks;      }
      inline bool isOOBChecksEnabled()          const { return mOOBChecks;      }
      inline bool isMemLogsEnabled()            const { return mMemLogs;        }
      inline bool isDontRun()                   const { return mDontRun;        }
      inline unsigned int getPacketSize()       const { return mPacketSize;     }
      inline unsigned int getMaxTraceCount()    const { return mMaxTraceCount;  }
      inline unsigned int getPaddingFactor()    const { if(!mOOBChecks) return 0; return mPaddingFactor;  }
      inline std::string getSimDir()            const { return mSimDir;         }
      inline std::string getUserPreSimScript()  const { return mUserPreSimScript;}
  	  inline std::string getUserPostSimScript()  const { return mUserPostSimScript;}
      inline DEBUG_MODE getLaunchWaveform() const { return mLaunchWaveform; }
      inline bool isInfoSuppressed()            const { return mSuppressInfo;    }
      inline bool isWarningsuppressed()         const { return mSuppressWarnings;}
      inline bool isErrorsSuppressed()          const { return mSuppressErrors;  }
      inline bool getVerbosityLevel()           const { return mVerbosity;       }    
      inline bool isKeepRunDirEnabled()         const { return mKeepRunDir;       }    
      inline bool isInfosToBePrintedOnConsole() const { return mPrintInfosInConsole;   }  
      inline unsigned int getServerPort()       const { return mServerPort;      }
      inline bool isErrorsToBePrintedOnConsole()   const { return mPrintErrorsInConsole;  }
      inline bool isWarningsToBePrintedOnConsole() const { return mPrintWarningsInConsole;}
      inline std::string getLauncherArgs() const { return mLauncherArgs;}
      inline bool isSystemDPAEnabled() const     { return mSystemDPA;              }
      inline ERTMODE getLegacyErt() const         { return mLegacyErt;              }
      inline long long getCuBaseAddrForce() const         { return mCuBaseAddrForce;              }
      inline bool isSharedFmodel() const         {return mIsSharedFmodel; } 
      inline TIMEOUT_SCALE getTimeOutScale() const    {return mTimeOutScale;}
      void populateEnvironmentSetup(std::map<std::string,std::string>& mEnvironmentNameValueMap);

    private:
      static config* mInst;         // (singleton)instance of this class
      bool mDiagnostics;
      bool mUMRChecks;
      bool mOOBChecks;
      bool mMemLogs;
      bool mDontRun;
      DEBUG_MODE mLaunchWaveform;
      std::string mSimDir;
      std::string mUserPreSimScript;
	    std::string mUserPostSimScript;
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
      std::string mLauncherArgs;
      bool mSystemDPA;
      ERTMODE mLegacyErt;
      long long mCuBaseAddrForce;
      bool      mIsSharedFmodel;
      TIMEOUT_SCALE mTimeOutScale;
      config();
      ~config() { };//empty destructor
  };

  void getDevicesInfo(std::vector<std::tuple<xclDeviceInfo2,std::list<DDRBank> ,bool, bool, FeatureRomHeader> >& devicesInfo);
  bool copyLogsFromOneFileToAnother(const std::string &logFile, std::ofstream &ofs);
  std::string getEmDebugLogFile();
  bool isXclEmulationModeHwEmuOrSwEmu();
  bool is_sw_emulation();
  std::string getRunDirectory();
  
  std::map<std::string,std::string> getEnvironmentByReadingIni();
  //Get CU index from IP_LAYOUT section for corresponding kernel name
  int getIPName2Index(const char *name, const char* ipLayoutbuf);
}

#endif
