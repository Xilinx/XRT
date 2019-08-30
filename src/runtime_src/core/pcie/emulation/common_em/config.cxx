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

#include "config.h"
#include "core/common/config_reader.h"

namespace xclemulation{

  DDRBank::DDRBank()
  {
    ddrSize = 0;
  }

  config* config::mInst= NULL;

  //get the instance of singleton class
  config* config::getInstance()
  {
    if( !mInst )
    {
      mInst = new config();
    }
    return mInst;
  }

  //destroy the singleton class
  void config::destroy()
  {
    delete mInst;
    mInst = NULL;
  }

  //constructor
  config::config()
  {
    mDiagnostics = true;
    mUMRChecks = false;
    mOOBChecks = false;
    mMemLogs = false;
    mLaunchWaveform = LAUNCHWAVEFORM::OFF;
    mDontRun = false;
    mSimDir = "";
    mPacketSize = 0x800000;
    mMaxTraceCount = 1;
    mPaddingFactor = 1;
    mSuppressInfo = false ;
    mSuppressWarnings = false;
    mSuppressErrors = false;
    mPrintInfosInConsole = true;
    mPrintWarningsInConsole = true;
    mPrintErrorsInConsole = true;
    mVerbosity = 0;
    mServerPort = 0;
    mKeepRunDir=false;
    mLauncherArgs = "";
    mSystemDPA = false;
  }

  static bool getBoolValue(std::string& value,bool defaultValue)
  {
    if(value.empty())
      return defaultValue;
    if (boost::iequals(value,"true" ))
    {
      return true;
    }
    if (boost::iequals(value,"false" ))
    {
      return false;
    }
    return defaultValue;
  }

  void config::populateEnvironmentSetup(std::map<std::string,std::string>& mEnvironmentNameValueMap)
  {
    for (auto i : mEnvironmentNameValueMap)
    {
      std::string name  = i.first;
      std::string value = i.second;
      if(value.empty() || name.empty())
        continue;

      if(name == "diagnostics")
      {
        enableDiagnostics(getBoolValue(value,false));
      }
      else if(name == "enable_umr")
      {
        enableUMRChecks(getBoolValue(value,false));
      }
      else if(name == "enable_oob")
      {
        enableOOBChecks(getBoolValue(value,false));
      }
      else if (name == "enable_mem_logs")
      {
        enableMemLogs(getBoolValue(value,false));
      }
      else if(name == "suppress_infos")
      {
        suppressInfo(getBoolValue(value,false));
      }
      else if(name == "suppress_errors")
      {
        suppressErrors(getBoolValue(value,false));
      }
      else if(name == "suppress_warnings")
      {
        suppressWarnings(getBoolValue(value,false));
      }
      else if(name == "print_infos_in_console")
      {
        printInfosInConsole(getBoolValue(value,true));
      }
      else if(name == "print_warnings_in_console")
      {
        printWarningsInConsole(getBoolValue(value,true));
      }
      else if(name == "print_errors_in_console")
      {
        printErrorsInConsole(getBoolValue(value,true));
      }
      else if(name == "dont_run")
      {
        setDontRun(getBoolValue(value,false));
      }
      else if(name == "enable_shared_memory")
      {
        //this is temporary solution to use legacy DDR model in emulation. We should remove this switch Once all issues in latest model is fixed
        if(!getBoolValue(value,true))
        {
         setenv("SDX_USE_LEGACY_FMODEL","true",true);
        }
      }
      else if(name == "keep_run_dir")
      {
        setKeepRunDir(getBoolValue(value,false));
      }
      else if(name == "sim_dir")
      {
        setSimDir(value);
      }
      else if(name == "verbosity")
      {
        unsigned int verbosity = strtoll(value.c_str(),NULL,0);
        if(verbosity > 0 )
          setVerbosityLevel(verbosity);
      }
      else if(name == "packet_size")
      {
        unsigned int packetSize = strtoll(value.c_str(),NULL,0);
        if(packetSize > 0 )
          setPacketSize(packetSize);
      }
      else if(name == "max_trace_count")
      {
        unsigned int maxTraceCount = strtoll(value.c_str(),NULL,0);
        if(maxTraceCount > 0 )
          setMaxTraceCount(maxTraceCount);
      }
      else if (name == "padding_factor")
      {
        unsigned int paddingFactor = atoi(value.c_str());
        if(paddingFactor > 0)
          setPaddingFactor(paddingFactor);
      }
      else if (name == "launcher_args")
      {
        setLauncherArgs(value);
      }
      else if(name == "launch_waveform")
      {
        if (boost::iequals(value,"gui" ))
        {
          setLaunchWaveform(LAUNCHWAVEFORM::GUI);
        }
        else if (boost::iequals(value,"batch" ))
        {
          setLaunchWaveform(LAUNCHWAVEFORM::BATCH);
        }
        else
        {
          setLaunchWaveform(LAUNCHWAVEFORM::OFF);
        }
      }
      else if(name == "Debug.sdx_server_port")
      {
        unsigned int serverPort = strtoll(value.c_str(),NULL,0);
        if(serverPort> 0 )
          setServerPort(serverPort);
      }
      else if(name == "enable_arbitration")
      {
        //Nothing to do
      }
      else if(name == "system_dpa")
      {
        setSystemDPA(getBoolValue(value,false));
      }
      else if(name.find("Debug.") == std::string::npos)
      {
        std::cout<<"WARNING: [SDx-EM 08] Invalid option '"<<name<<"` specified in sdaccel.ini"<<std::endl;
      }
    }
    //this code has to be removed once gui generates ini file by adding launch_waveform property
    const char* simMode = getenv("HW_EM_LAUNCH_WAVEFORM");
    if(simMode)
    {
      std::string simulationMode = simMode;
      if (boost::iequals(simulationMode,"gui" ))
      {
        setLaunchWaveform(LAUNCHWAVEFORM::GUI);
      }
      else if (boost::iequals(simulationMode,"batch" ))
      {
        setLaunchWaveform(LAUNCHWAVEFORM::BATCH);
      }
    }

  }


  static std::string getSelfPath()
  {
    char buff[PATH_MAX];
    ssize_t len = ::readlink("/proc/self/exe", buff, sizeof(buff)-1);
    if (len != -1)
    {
      buff[len] = '\0';
      return std::string(buff);
    }
    return "";
  }

  static const char* valueOrEmpty(const char* cstr)
  {
    return cstr ? cstr : "";
  }

  static std::string getExecutablePath()
  {
    std::string hostBinaryPath = getSelfPath();
    if(hostBinaryPath.empty())
    {
      std::cout<<"unable to findout the host binary path in emulation driver "<<std::endl;
    }
    std::string directory;
    const size_t last_slash_idx = hostBinaryPath.rfind("/");
    if (std::string::npos != last_slash_idx)
    {
      directory = hostBinaryPath.substr(0, last_slash_idx);
    }
    return directory;
  }

  static std::string getEmConfigFilePath()
  {
    std::string executablePath = getExecutablePath();
    std::string emConfigPath = valueOrEmpty(std::getenv("EMCONFIG_PATH"));
    if (!emConfigPath.empty()) {
      executablePath = emConfigPath;
    }
    std::string xclEmConfigfile = executablePath.empty()? "emconfig.json" :executablePath+ "/emconfig.json";
    return xclEmConfigfile;
  }

  bool isXclEmulationModeHwEmuOrSwEmu()
  {
    static auto xem = std::getenv("XCL_EMULATION_MODE");
    if(xem)
    {
      if((std::strcmp(xem,"hw_emu") == 0) || (std::strcmp(xem,"sw_emu") == 0))
      {
        return true;
      }
    }
    return false;
  }

  std::string getEmDebugLogFile()
  {
    std::string executablePath = getExecutablePath();
    std::string xclEmConfigfile = executablePath.empty()? "emulation_debug.log" :executablePath+ "/emulation_debug.log";
    return xclEmConfigfile;
  }

  static std::string getCurrenWorkingDir()
  {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
      return std::string(cwd);
    }
    return "";
  }

  static bool checkWritable(std::string &sDir)
  {
    if(sDir.empty())
      return false;
    std::string sPermissionCheckFile = sDir +"/.permission_check.txt";
    FILE *fp = fopen(sPermissionCheckFile.c_str(), "w");
    if (fp == NULL)
    {
      return false;
    }
    fclose(fp);
    int rV = std::remove(sPermissionCheckFile.c_str());
    if(rV < 0 )
      return false;
    return true;
  }

  std::string getRunDirectory()
  {
    std::string executablePath = getExecutablePath();
    std::string sUserRunDir = valueOrEmpty(std::getenv("SDACCEL_EM_RUN_DIR"));
    if(!sUserRunDir.empty())
      executablePath = sUserRunDir;
    bool bWritable = checkWritable(executablePath);
    if(!bWritable)
    {
      std::string sCurrWorkDir = getCurrenWorkingDir();
      bWritable = checkWritable(sCurrWorkDir);
      if(bWritable)
      {
        executablePath = sCurrWorkDir;
      }
    }
    if(!bWritable)
    {
      std::cout<<"Unable to find writable directory. Please provide writable directory using SDACCEL_EM_RUN_DIR"<<std::endl;
    }
    std::string sRunDir = executablePath.empty()? ".run" :executablePath+ "/.run";
    return sRunDir;
  }

  /* Use the common INI file reader */
  std::map<std::string,std::string> getEnvironmentByReadingIni()
  {
    std::map<std::string,std::string> environmentNameValueMap;
    const boost::property_tree::ptree &e_tree = xrt_core::config::detail::get_ptree_value("Emulation");
    for (auto& key : e_tree)
    {
      environmentNameValueMap[key.first] = key.second.get_value<std::string>();
    }
    const boost::property_tree::ptree &d_tree = xrt_core::config::detail::get_ptree_value("Debug");
    for (auto& key : d_tree)
    {
      environmentNameValueMap["Debug." + key.first] = key.second.get_value<std::string>();
    }
    return environmentNameValueMap;
  }

  //initialize memMap
  static void initializeMemMap(std::map<std::string, uint64_t>& memMap)
  {
    memMap["1K"]    = xclemulation::MEMSIZE_1K;
    memMap["4K"]    = xclemulation::MEMSIZE_4K;
    memMap["8K"]    = xclemulation::MEMSIZE_8K;
    memMap["16K"]   = xclemulation::MEMSIZE_16K;
    memMap["32K"]   = xclemulation::MEMSIZE_32K;
    memMap["64K"]   = xclemulation::MEMSIZE_64K;
    memMap["128K"]  = xclemulation::MEMSIZE_128K;
    memMap["256K"]  = xclemulation::MEMSIZE_256K;
    memMap["512K"]  = xclemulation::MEMSIZE_512K;

    memMap["1M"]    = xclemulation::MEMSIZE_1M;
    memMap["2M"]    = xclemulation::MEMSIZE_2M;
    memMap["4M"]    = xclemulation::MEMSIZE_4M;
    memMap["8M"]    = xclemulation::MEMSIZE_8M;
    memMap["16M"]   = xclemulation::MEMSIZE_16M;
    memMap["32M"]   = xclemulation::MEMSIZE_32M;
    memMap["64M"]   = xclemulation::MEMSIZE_64M;
    memMap["128M"]  = xclemulation::MEMSIZE_128M;
    memMap["256M"]  = xclemulation::MEMSIZE_256M;
    memMap["512M"]  = xclemulation::MEMSIZE_512M;

    memMap["1G"]    = xclemulation::MEMSIZE_1G;
    memMap["2G"]    = xclemulation::MEMSIZE_2G;
    memMap["4G"]    = xclemulation::MEMSIZE_4G;
    memMap["8G"]    = xclemulation::MEMSIZE_8G;
    memMap["16G"]   = xclemulation::MEMSIZE_16G;
    memMap["32G"]   = xclemulation::MEMSIZE_32G;
    memMap["64G"]   = xclemulation::MEMSIZE_64G;
    memMap["128G"]  = xclemulation::MEMSIZE_128G;
    memMap["256G"]  = xclemulation::MEMSIZE_256G;
    memMap["512G"]  = xclemulation::MEMSIZE_512G;

    memMap["1T"]    = xclemulation::MEMSIZE_1T;
    memMap["2T"]    = xclemulation::MEMSIZE_2T;
    memMap["4T"]    = xclemulation::MEMSIZE_4T;
    memMap["8T"]    = xclemulation::MEMSIZE_8T;
    memMap["16T"]   = xclemulation::MEMSIZE_16T;
    memMap["32T"]   = xclemulation::MEMSIZE_32T;
    memMap["64T"]   = xclemulation::MEMSIZE_64T;
    memMap["128T"]  = xclemulation::MEMSIZE_128T;
    memMap["256T"]  = xclemulation::MEMSIZE_256T;
    memMap["512T"]  = xclemulation::MEMSIZE_512T;

    memMap["1KB"]    = xclemulation::MEMSIZE_1K;
    memMap["4KB"]    = xclemulation::MEMSIZE_4K;
    memMap["8KB"]    = xclemulation::MEMSIZE_8K;
    memMap["16KB"]   = xclemulation::MEMSIZE_16K;
    memMap["32KB"]   = xclemulation::MEMSIZE_32K;
    memMap["64KB"]   = xclemulation::MEMSIZE_64K;
    memMap["128KB"]  = xclemulation::MEMSIZE_128K;
    memMap["256KB"]  = xclemulation::MEMSIZE_256K;
    memMap["512KB"]  = xclemulation::MEMSIZE_512K;

    memMap["1MB"]    = xclemulation::MEMSIZE_1M;
    memMap["2MB"]    = xclemulation::MEMSIZE_2M;
    memMap["4MB"]    = xclemulation::MEMSIZE_4M;
    memMap["8MB"]    = xclemulation::MEMSIZE_8M;
    memMap["16MB"]   = xclemulation::MEMSIZE_16M;
    memMap["32MB"]   = xclemulation::MEMSIZE_32M;
    memMap["64MB"]   = xclemulation::MEMSIZE_64M;
    memMap["128MB"]  = xclemulation::MEMSIZE_128M;
    memMap["256MB"]  = xclemulation::MEMSIZE_256M;
    memMap["512MB"]  = xclemulation::MEMSIZE_512M;

    memMap["1GB"]    = xclemulation::MEMSIZE_1G;
    memMap["2GB"]    = xclemulation::MEMSIZE_2G;
    memMap["4GB"]    = xclemulation::MEMSIZE_4G;
    memMap["8GB"]    = xclemulation::MEMSIZE_8G;
    memMap["16GB"]   = xclemulation::MEMSIZE_16G;
    memMap["32GB"]   = xclemulation::MEMSIZE_32G;
    memMap["64GB"]   = xclemulation::MEMSIZE_64G;
    memMap["128GB"]  = xclemulation::MEMSIZE_128G;
    memMap["256GB"]  = xclemulation::MEMSIZE_256G;
    memMap["512GB"]  = xclemulation::MEMSIZE_512G;

    memMap["1TB"]    = xclemulation::MEMSIZE_1T;
    memMap["2TB"]    = xclemulation::MEMSIZE_2T;
    memMap["4TB"]    = xclemulation::MEMSIZE_4T;
    memMap["8TB"]    = xclemulation::MEMSIZE_8T;
    memMap["16TB"]   = xclemulation::MEMSIZE_16T;
    memMap["32TB"]   = xclemulation::MEMSIZE_32T;
    memMap["64TB"]   = xclemulation::MEMSIZE_64T;
    memMap["128TB"]  = xclemulation::MEMSIZE_128T;
    memMap["256TB"]  = xclemulation::MEMSIZE_256T;

  }

  static void populateDDRBankInfo(boost::property_tree::ptree const& ddrBankTree, xclDeviceInfo2& info, std::list<DDRBank>& DDRBankList, std::map<std::string, uint64_t>& memMap)
  {
    info.mDDRSize = 0;
    info.mDDRBankCount = 0;
    DDRBankList.clear();
    using boost::property_tree::ptree;
    for (auto& prop : ddrBankTree)
    {
      for (auto& prop1 : prop.second)//we have only one property as of now which is Size of each DDRBank
      {
        std::string name = prop1.first;
        std::string value = prop1.second.get_value<std::string>();
        if(name == "Size")
        {
          uint64_t size =  0;
          std::map<std::string,uint64_t>::iterator it = memMap.find(value);
          if(it != memMap.end())
          {
            size = (*it).second;
          }
          info.mDDRSize = info.mDDRSize + size;
          DDRBank bank;
          bank.ddrSize = size;
          DDRBankList.push_back(bank);
        }
      }

      info.mDDRBankCount = info.mDDRBankCount + 1;
    }
    //if no ddr exists, create a default DDR of 16GB
    if(DDRBankList.size() == 0)
    {
      DDRBank bank;
      bank.ddrSize = 0x400000000;
      DDRBankList.push_back(bank);
      info.mDDRBankCount = info.mDDRBankCount + 1;
    }
  }

  static void populateFeatureRom(boost::property_tree::ptree const& featureRomTree, FeatureRomHeader& fRomHeader)
  {
    for (auto& prop : featureRomTree)
    {
      std::string name = prop.first;
      if(name == "Major_Version")
      {
        unsigned int majorVersion = prop.second.get_value<unsigned>();
        fRomHeader.MajorVersion = majorVersion;
      }
      else if(name == "Minor_Version")
      {
        unsigned int minorVersion = prop.second.get_value<unsigned>();
        fRomHeader.MinorVersion = minorVersion;
      }
      else if(name == "Vivado_Build_Id")
      {
        unsigned int vivadoBuildId = prop.second.get_value<unsigned long>();
        fRomHeader.VivadoBuildID = vivadoBuildId;
      }
      else if(name == "Ip_Build_Id")
      {
        unsigned long ipBuildId = prop.second.get_value<unsigned long>();
        fRomHeader.IPBuildID = ipBuildId;
      }
      else if(name == "Time_Since_Epoch")
      {
        unsigned long long timeSinceEpoch = prop.second.get_value<unsigned long long>();
        fRomHeader.TimeSinceEpoch = timeSinceEpoch;
      }
      else if(name == "Ddr_Channel_Count")
      {
        unsigned int ddrChannelCount = prop.second.get_value<unsigned>();
        fRomHeader.DDRChannelCount = ddrChannelCount;
      }
      else if(name == "Ddr_Channel_Size")
      {
        unsigned int ddrChannelSize = prop.second.get_value<unsigned>();
        fRomHeader.DDRChannelSize = ddrChannelSize;
      }
      else if(name == "Dr_Base_Address")
      {
        unsigned long long drBaseAddress = prop.second.get_value<unsigned long long>();
        fRomHeader.DRBaseAddress = drBaseAddress;
      }
      else if(name == "Feature_Bitmap")
      {
        unsigned long long featureBitMap = prop.second.get_value<unsigned long long>();
        fRomHeader.FeatureBitMap= featureBitMap;
      }
      else if(name == "Cdma_Base_Address0")
      {
        fRomHeader.CDMABaseAddress[0] = prop.second.get_value<unsigned long long>();
      }
      else if(name == "Cdma_Base_Address1")
      {
        fRomHeader.CDMABaseAddress[1] = prop.second.get_value<unsigned long long>();
      }
      else if(name == "Cdma_Base_Address2")
      {
        fRomHeader.CDMABaseAddress[2] = prop.second.get_value<unsigned long long>();
      }
      else if(name == "Cdma_Base_Address3")
      {
        fRomHeader.CDMABaseAddress[3] = prop.second.get_value<unsigned long long>();
      }
    }
  }

  static void populateHwDevicesOfSingleBoard(boost::property_tree::ptree & deviceTree, std::vector<std::tuple<xclDeviceInfo2,std::list<DDRBank> ,bool, bool,FeatureRomHeader> >& devicesInfo,std::map<std::string, uint64_t>& memMap, bool bUnified, bool bXPR)
  {

    for (auto& device : deviceTree)
    {
      xclDeviceInfo2 info;

      //fill info with default values
      info.mMagic = 0X586C0C6C;
      //info.mHALMajorVersion = XCLHAL_MAJOR_VER;
      //info.mHALMinorVersion= XCLHAL_MINOR_VER;
      info.mVendorId = 0x10ee;
      info.mSubsystemVendorId = 0x0000;
      info.mDeviceVersion = 0x0000;
      info.mDDRSize = MEMSIZE_4G;
      info.mDataAlignment = DDR_BUFFER_ALIGNMENT;
      info.mDDRBankCount = 1;
      for(unsigned int i = 0; i < 4 ;i++)
        info.mOCLFrequency[i] = 300;
      unsigned numDevices = 1;
      std::list<DDRBank> DDRBankList;
      DDRBank bank;
      bank.ddrSize = MEMSIZE_4G;
      DDRBankList.push_back(bank);
      FeatureRomHeader fRomHeader;
      std::memset(&fRomHeader, 0, sizeof(FeatureRomHeader));


      //iterate over all the properties of device and fill the info structure. This info object gets used to create  device object
      for (auto& prop : device.second)
      {
        if(prop.first == "Name")
        {
          std::string mName = prop.second.get_value<std::string>();
          if(mName.empty() == false)
          {
            if(strlen(mName.c_str()) < 256)//info.mName is static array of size 256
              std::strcpy(info.mName, mName.c_str());
          }
        }
        else if(prop.first == "HalMajorVersion")
        {
          unsigned short halMajorVersion = prop.second.get_value<unsigned short>();
          info.mHALMajorVersion = halMajorVersion;
        }
        else if(prop.first == "HalMinorVersion")
        {
          unsigned short halMinorVersion = prop.second.get_value<unsigned short>();
          info.mHALMinorVersion = halMinorVersion;
        }
        else if(prop.first == "VendorId")
        {
          unsigned short vendorId = prop.second.get_value<unsigned short>();
          info.mVendorId = vendorId;
        }
        else if(prop.first == "SubsystemVendorId")
        {
          unsigned short subsystemVendorId = prop.second.get_value<unsigned short>();
          info.mSubsystemVendorId = subsystemVendorId;
        }
        else if(prop.first == "DeviceVersion")
        {
          unsigned deviceVersion = prop.second.get_value<unsigned>();
          info.mDeviceVersion = deviceVersion;
        }
        else if(prop.first == "DataAlignment")
        {
          size_t dataAlignment = prop.second.get_value<unsigned>();
          info.mDataAlignment = dataAlignment;
        }
        else if(prop.first == "DdrBanks")
        {
          boost::property_tree::ptree ddrBankTree = prop.second;
          populateDDRBankInfo(ddrBankTree, info, DDRBankList,memMap);
        }
        else if(prop.first == "FeatureRom")
        {
          boost::property_tree::ptree featureRomTree = prop.second;
          populateFeatureRom(featureRomTree,fRomHeader);
        }
        else if(prop.first == "OclFreqency")
        {
          unsigned oclFrequency = prop.second.get_value<unsigned>();
          info.mOCLFrequency[0] = oclFrequency;
        }
        else if(prop.first == "NumDevices")
        {
          numDevices = prop.second.get_value<unsigned>();
        }

      }
      //get the number of times this device is instantiated using numDevices variable.
      //iterate using this variable and create that many number of devices.
      for(unsigned int i = 0; i < numDevices;i++)
      {
        devicesInfo.push_back(make_tuple(info,DDRBankList,bUnified, bXPR,fRomHeader));
      }
    }
    return;
  }

  //create all the devices If devices child is present in this tree otherwise call this function recursively for all the child trees
  //iterate over devices tree and create all the device objects.
  static void populateHwEmDevices(boost::property_tree::ptree const& platformTree,std::vector<std::tuple<xclDeviceInfo2,std::list<DDRBank> ,bool, bool, FeatureRomHeader > >& devicesInfo,std::map<std::string, uint64_t>& memMap)
  {
    using boost::property_tree::ptree;
    ptree::const_iterator platformEnd = platformTree.end();
    bool bUnified = false;
    bool bXPR = false;
    for (ptree::const_iterator it = platformTree.begin(); it != platformEnd; ++it)
    {
      if(it->first == "UnifiedPlatform")
      {
        std::string unified = it->second.get_value<std::string>();
        bUnified = getBoolValue(unified,bUnified);
      }
      else if(it->first == "ExpandedPR")
      {
        std::string expandedPR = it->second.get_value<std::string>();
        bXPR = getBoolValue(expandedPR,bXPR);
      }
    }

    if(platformTree.count("Boards") != 0)// Boards child is present
    {
      for (auto& board : platformTree.get_child("Boards"))
      {
        unsigned int numBoards = 1;
        boost::property_tree::ptree deviceTree;
        //iterate over all the properties of device and fill the info structure. This info object gets used to create  device object
        for (auto& prop : board.second)
        {
          if(prop.first == "NumBoards")
          {
            numBoards = prop.second.get_value<unsigned>();
          }
          else if(prop.first == "Devices")
          {
            deviceTree = prop.second;
          }
        }
        for(unsigned int i = 0; i < numBoards; i++)
          populateHwDevicesOfSingleBoard(deviceTree,devicesInfo,memMap, bUnified, bXPR);
      }
    }
  }

  static bool validateVersions(boost::property_tree::ptree const& versionTree)
  {
    using boost::property_tree::ptree;
    ptree::const_iterator end = versionTree.end();
    for (ptree::const_iterator it = versionTree.begin(); it != end; ++it)
    {
      if(it->first == "FileVersion")
      {
        std::string fileVersion = it->second.get_value<std::string>();
        if(fileVersion != "2.0")
        {
          std::cout<<"incompatible version of emconfig.json found.Please regenerate this file"<<std::endl;
          return false;
        }
      }
      else if(it->first == "ToolVersion")
      {
        //std::string toolVersion= it->second.get_value<std::string>();
      }
    }
    return true;
  }

  void getDevicesInfo(std::vector<std::tuple<xclDeviceInfo2,std::list<DDRBank> ,bool, bool , FeatureRomHeader> >& devicesInfo)
  {
    std::string emConfigFile =  getEmConfigFilePath();
    std::ifstream ifs;
    ifs.open(emConfigFile, std::ifstream::in);
    if(!ifs)
    {
      return;
    }

    if(ifs.is_open())
    {
      //  std::cout<<emConfigFile<<" is used for the platform configuration "<<std::endl;
    }

    std::map<std::string, uint64_t> memMap;
    initializeMemMap(memMap);
    boost::property_tree::ptree configTree;
    boost::property_tree::read_json(ifs, configTree);//read the config file and stores in configTree
    ifs.close();

    using boost::property_tree::ptree;
    ptree::const_iterator end = configTree.end();
    boost::property_tree::ptree versionTree;
    boost::property_tree::ptree platformTree;

    //iterate over configTree and  check whether file version is  1.0 or not. If not return 1.
    //get both platform and environment tree
    for (ptree::const_iterator it = configTree.begin(); it != end; ++it)
    {
      if(it->first == "Version")
      {
        versionTree = it->second;//get the version tree
      }
      else if(it->first == "Platform")
      {
        platformTree = it->second; //get the platform tree
      }
    }

    bool success = validateVersions(versionTree);
    if(!success)
      return;//validation of Versions failed.
    populateHwEmDevices(platformTree,devicesInfo,memMap);
  }

  bool copyLogsFromOneFileToAnother(const std::string &logFile, std::ofstream &ofs) {
    std::ifstream ifs(logFile.c_str());
    if (!ifs.is_open() || !ofs.is_open())
      return true;

    ofs << ifs.rdbuf() << std::endl;
    ifs.close();
    return false;
  }
}
