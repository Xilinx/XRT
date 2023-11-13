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
#include "core/common/xclbin_parser.h"
#include <errno.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>

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
    mLaunchWaveform = debug_mode::off;
    mDontRun = false;
    mNewMbscheduler = true;
    mXgqMode = false;
    mSimDir = "";
    mUserPreSimScript = "";
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
    mKeepRunDir = true;
    mLauncherArgs = "";
    mSystemDPA = true;
    mLegacyErt = ertmode::none;
    mCuBaseAddrForce = -1;
    mIsSharedFmodel = true;
    mIsM2MEnabled = false;
    mTimeOutScale = TIMEOUT_SCALE::NA;
    mIsPlatformDataAvailable = false;
    mIsDisabledHostBuffer = false;
    mIsFasterNocDDRAccessEnabled = true;
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

  static std::filesystem::path get_file_absolutepath(const std::string& filename) {
    
    std::filesystem::path exe_parent_path {getExecutablePath()};
    auto filepath = exe_parent_path / filename;

    if (std::filesystem::exists(filepath) == false) {
      auto current_path = std::filesystem::current_path();
      filepath = current_path / filename;
      if (std::filesystem::exists(filepath) == false)
        return std::filesystem::path{};
    }
    return filepath;

  }

  void config::populateEnvironmentSetup(std::map<std::string,std::string>& mEnvironmentNameValueMap)
  {
    setenv("HW_EM_DISABLE_LATENCY", "true", true);
    for (auto& [name, value] : mEnvironmentNameValueMap) {
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
      else if(name == "new_mbscheduler")
      {
        setNewMbscheduler(getBoolValue(value,false));
      }
      else if(name == "xgq_mode")
      {
        setXgqMode(getBoolValue(value,false));
      }
      else if (name == "user_pre_sim_script") {
        auto filepath = get_file_absolutepath(value);
        std::string absolutePath = getAbsolutePath(value, filepath.parent_path().string());
        setUserPreSimScript(absolutePath);
        setenv("USER_PRE_SIM_SCRIPT", absolutePath.c_str(), true);
      }
      else if (name == "user_post_sim_script") {
        auto filepath = get_file_absolutepath(value);
        std::string absolutePath = getAbsolutePath(value, filepath.parent_path().string());
        setUserPostSimScript(absolutePath);
        setenv("USER_POST_SIM_SCRIPT", absolutePath.c_str(), true);
      } 
      else if (name == "xtlm_aximm_log") {
        bool val = getBoolValue(value, true);
        if (val) {
          setenv("ENABLE_XTLM_AXIMM_LOG", "1", true);
        } else {
          setenv("ENABLE_XTLM_AXIMM_LOG", "0", true);
        }
      }
      else if (name == "xtlm_axis_log") {
        bool val = getBoolValue(value, true);
        if (val) {
          setenv("ENABLE_XTLM_AXIS_LOG", "1", true);
        } else {
          setenv("ENABLE_XTLM_AXIS_LOG", "0", true);
        }
      }
      else if (name == "ENABLE_GMEM_LATENCY" || name == "enable_gmem_latency") {
        //This is then new INI option that sets the ENV HW_EM_DISABLE_LATENCY to appropriate value before 
        //launching simulation
        bool val = getBoolValue(value, false);
        if (val) {
          setenv("HW_EM_DISABLE_LATENCY", "false", true);
        } else {
          setenv("HW_EM_DISABLE_LATENCY", "true", true);
        }
      } else if (name == "enable_memory_persistence" || name == "ENABLE_MEMORY_PERSISTENCE") {
        bool val = getBoolValue(value, false);
        if (val) {
          setenv("HWEMU_MEMORY_PERSISTENCE", "true", true);
        } else {
          setenv("HWEMU_MEMORY_PERSISTENCE", "false", true);
        }
      }
      else if (name == "wcfg_file_path") {
        auto filepath = get_file_absolutepath(value);
        std::string path = getAbsolutePath(value, filepath.parent_path().string());
        setWcfgFilePath(path);
      }
      else if(name == "enable_shared_memory")
      {
        mIsSharedFmodel=getBoolValue(value,true);
      }
      else if (name == "enable_m2m") {
        mIsM2MEnabled = getBoolValue(value, false);
      }
      else if (name == "host_buffer_sync") {
    	  mIsDisabledHostBuffer = getBoolValue(value, false);
      }
      else if(name == "keep_run_dir")
      {
        setKeepRunDir(getBoolValue(value,true));
      }
      else if (name == "enable_prep_target" || name == "enable_debug" || name == "aie_sim_options") {
        //Do nothing: Added to bypass the WARNING that is issued below stating "invalid xrt.ini option" 
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
      else if(name == "fast_nocddr_access")
      {
        mIsFasterNocDDRAccessEnabled = getBoolValue(value, true);
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
      else if(name == "launch_waveform" || name == "debug_mode" )
      {
        if (name == "launch_waveform")
          std::cout << "WARNING: [HW-EMU 09] INI option 'launch_waveform' is deprecated and replaced with the new switch 'debug_mode'." << std::endl;
        
        if (boost::iequals(value,"gui" ))
        {
          setLaunchWaveform(debug_mode::gui);
        }
        else if (boost::iequals(value,"batch" ))
        {
          setLaunchWaveform(debug_mode::batch);
        }
        else if (boost::iequals(value,"off" ))
        {
          setLaunchWaveform(debug_mode::off);
        } 
        else if (boost::iequals(value,"gdb")) 
        {
          setLaunchWaveform(debug_mode::gdb);
          std::cout << "WARNING: [HW-EMU 08] Invalid option '" << value << "' specified in xrt.ini/sdaccel.ini for debug_mode." << std::endl;
        }
        else
        {
          setLaunchWaveform(debug_mode::off);
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
      else if(name == "aliveness_message_interval")
      {
        //Nothing to do
      }
      else if(name == "system_dpa")
      {
        setSystemDPA(getBoolValue(value,true));
      }
      else if(name == "legacy_ert")
      {
        if (boost::iequals(value,"false" ))
          setLegacyErt(ertmode::updated);
        else if(boost::iequals(value,"true"))
          setLegacyErt(ertmode::legacy);
      } else if (name=="cu_base_addr_force") {
          mCuBaseAddrForce= strtoll(value.c_str(),NULL,0);
      } else if (name == "timeout_scale") {
      	  if (boost::iequals(value,"ms")) {
      		mTimeOutScale=TIMEOUT_SCALE::MS;
      	  } else if (boost::iequals(value,"sec")) {
    		  mTimeOutScale=TIMEOUT_SCALE::SEC;
    	  } else if (boost::iequals(value,"min")) {
    		  mTimeOutScale=TIMEOUT_SCALE::MIN;
    	  } else {
    		  mTimeOutScale=TIMEOUT_SCALE::NA;
    	  }
      }
      else if(name.find("Debug.") == std::string::npos)
      {
        std::cout<<"WARNING: [HW-EMU 08] Invalid option '"<<name<<"` specified in xrt.ini/sdaccel.ini"<<std::endl;
      }
    }
    //this code has to be removed once gui generates ini file by adding launch_waveform property
    const char* simMode = getenv("HW_EM_LAUNCH_WAVEFORM");
    if(simMode)
    {
      std::string simulationMode = simMode;
      if (boost::iequals(simulationMode,"gui" ))
      {
        setLaunchWaveform(debug_mode::gui);
      }
      else if (boost::iequals(simulationMode,"batch" ))
      {
        setLaunchWaveform(debug_mode::batch);
      }
      else if (boost::iequals(simulationMode,"off" ))
      {
        setLaunchWaveform(debug_mode::off);
      }
      else if (boost::iequals(simulationMode,"gdb" ))
      {
        setLaunchWaveform(debug_mode::gdb);
        std::cout<<"WARNING: [HW-EMU 08] Invalid option '"<<simulationMode<<"` specified in xrt.ini/sdaccel.ini"<<std::endl;
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

  std::string getAbsolutePath(const std::string& pathStr, const std::string& absBuildDirStr)
  {
    // If path value not set, user did not supply one. Must return empty string.
    if (pathStr.empty()) {
      return pathStr;
    }
    if (absBuildDirStr.empty()) {
      return pathStr;
    }

    return std::filesystem::absolute(pathStr.c_str()).string();
  }

  std::string getExecutablePath()
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
    auto filename{"emconfig.json"};
    auto filepath = get_file_absolutepath(filename);
    std::filesystem::path emconfig_env {valueOrEmpty(std::getenv("EMCONFIG_PATH"))};
    if (emconfig_env.empty() == false) {
      auto filepath_env_value = emconfig_env / "emconfig.json";
      if (std::filesystem::exists(filepath_env_value)) {
        filepath = filepath_env_value;
      }
    }
    return filepath.string();
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

  bool is_sw_emulation()
  {    
    static auto xem = std::getenv("XCL_EMULATION_MODE");
    if (xem)
    {
      if (std::strcmp(xem, "sw_emu") == 0)
      {
        return true;
      }
    }
    return false;
  }

  std::string getEmDebugLogFile()
  {
    auto self_path = std::filesystem::current_path();
    auto em_debug_file_path = self_path / "emulation_debug.log";
    return em_debug_file_path.string();
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

  // Converts any sort of memory size notation provided by the platform to proper memory value 
  // directly without any lookup. This is scalable approach and supports more such memory notations.
  uint64_t get_mem_value(std::string& memorySizeStr) {
    for (auto it = memorySizeStr.begin(); it != memorySizeStr.end(); it++) {
      if (isalpha(*it)) {
        uint64_t int_val = atoi(memorySizeStr.substr(0, *it).c_str()); 
        if (std::toupper(*it) == 'K')
          return int_val*1024; 
        else if (std::toupper(*it) == 'M')
          return int_val*1024*1024; 
        else if (std::toupper(*it) == 'G')
          return int_val*1024*1024*1024; 
        else if (std::toupper(*it) == 'T')
          return int_val*1024*1024*1024*1024; 
      }
    }
    return 0;
  }

  static void populateDDRBankInfo(boost::property_tree::ptree const& ddrBankTree, xclDeviceInfo2& info, 
    std::list<DDRBank>& DDRBankList)
  {
    info.mDDRSize = 0;
    info.mDDRBankCount = 0;
    DDRBankList.clear();
    using boost::property_tree::ptree;
    for (auto& prop : ddrBankTree) {
      for (auto& prop1 : prop.second) { //we have only one property as of now which is Size of each DDRBank
        std::string name = prop1.first;
        std::string value = prop1.second.get_value<std::string>();
        if (name == "Size") {
          uint64_t size = get_mem_value(value);
          if (size != 0) {
            info.mDDRSize = info.mDDRSize + size;
            DDRBank bank;
            bank.ddrSize = size;
            DDRBankList.push_back(bank);
          }
        }
      }
      info.mDDRBankCount = info.mDDRBankCount + 1;
    }
    //if no ddr exists, create a default DDR of 16GB
    if (DDRBankList.size() == 0) {
      DDRBank bank;
      bank.ddrSize = 0x400000000;
      DDRBankList.push_back(bank);
      info.mDDRBankCount = info.mDDRBankCount + 1;
    }
  }

  static void populatePlatformData(boost::property_tree::ptree const& platformDataTree, std::map<std::string, std::string>& platform_data) {

    for (auto& prop : platformDataTree) {
      platform_data[prop.first] = prop.second.get_value<std::string>();
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

  static void populateHwDevicesOfSingleBoard(boost::property_tree::ptree & deviceTree, std::vector<std::tuple<xclDeviceInfo2,std::list<DDRBank> ,bool, bool,
     FeatureRomHeader, boost::property_tree::ptree > >& devicesInfo, bool bUnified, bool bXPR)
  {

    for (auto& device : deviceTree)
    {
      xclDeviceInfo2 info;

      //fill info with default values
      info.mMagic = 0X586C0C6C;
      info.mHALMajorVersion = XCLHAL_MAJOR_VER;
      info.mHALMinorVersion= XCLHAL_MINOR_VER;
      info.mVendorId = 0x10ee;
      info.mSubsystemVendorId = 0x0000;
      info.mDeviceVersion = 0x0000;
      info.mDeviceId = 0x0000;
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
      boost::property_tree::ptree platformDataTree;

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
          populateDDRBankInfo(ddrBankTree, info, DDRBankList);
        }
        else if(prop.first == "FeatureRom")
        {
          boost::property_tree::ptree featureRomTree = prop.second;
          populateFeatureRom(featureRomTree,fRomHeader);
        }
        else if (prop.first == "PlatformData")
        {
          platformDataTree = prop.second;
          std::map<std::string, std::string> platDataMap;
          populatePlatformData(platformDataTree, platDataMap);

          if (platDataMap.size()) {
            config::getInstance()->setIsPlatformEnabled(true);
          }
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
        devicesInfo.push_back(make_tuple(info, DDRBankList, bUnified, bXPR, fRomHeader, platformDataTree));
      }
    }
    return;
  }

  //create all the devices If devices child is present in this tree otherwise call this function recursively for all the child trees
  //iterate over devices tree and create all the device objects.
  static void populateHwEmDevices(boost::property_tree::ptree const& platformTree,std::vector<std::tuple<xclDeviceInfo2,std::list<DDRBank> ,bool, bool, 
    FeatureRomHeader, boost::property_tree::ptree> >& devicesInfo)
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
          populateHwDevicesOfSingleBoard(deviceTree, devicesInfo, bUnified, bXPR);
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

  void getDevicesInfo(std::vector<std::tuple<xclDeviceInfo2, std::list<DDRBank>, bool, bool, FeatureRomHeader, boost::property_tree::ptree> >& devicesInfo)
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
    populateHwEmDevices(platformTree, devicesInfo);
  }

  bool copyLogsFromOneFileToAnother(const std::string &logFile, std::ofstream &ofs) {
    std::ifstream ifs(logFile.c_str());
    if (!ifs.is_open() || !ofs.is_open())
      return true;

    ofs << ifs.rdbuf() << std::endl;
    ifs.close();
    return false;
  }

  std::string getXclbinVersion(const axlf* top)
  {
    std::string sVersion = "";
    const axlf_section_header *xml_hdr = ::xclbin::get_axlf_section(top, BUILD_METADATA);

    if (!xml_hdr) {
      return sVersion;
    }
    auto begin = reinterpret_cast<const char*>(top) + xml_hdr->m_sectionOffset;
    const char *json_data = reinterpret_cast<const char*>(begin);
    uint64_t xml_size = xml_hdr->m_sectionSize;

    boost::property_tree::ptree json_project;
    std::stringstream json_stream;
    json_stream.write(json_data, xml_size);
    boost::property_tree::read_json(json_stream, json_project);
    auto json_meta = json_project.get_child_optional("build_metadata");
    if (!json_meta) {
      return sVersion;
    }
    auto buildMetaData = json_project.get_child("build_metadata");
    //std::string sTool = buildMetaData.get<std::string>("xclbin.generated_by.name", "");
    sVersion = buildMetaData.get<std::string>("xclbin.generated_by.version", "");
    //std::string sTimeStamp = buildMetaData.get<std::string>("xclbin.generated_by.time_stamp", "");    
    //std::cout << __func__ <<" Tool : " << sTool << " Version : " << sVersion << " TimeStamp : " << sTimeStamp << std::endl;
    return sVersion;
  }

  std::string getVivadoVersion()
  {
    char *xilinxVivadoEnvvar = getenv("XILINX_VIVADO");
    std::string sVivadoDir = "";
    if (xilinxVivadoEnvvar)
    {
      sVivadoDir = xilinxVivadoEnvvar;
    }
    std::string strVersionTmp = "";
    for (int i = VIVADO_MIN_VERSION; i < VIVADO_MAX_VERSION; i++) {
      float version = (float)i;
      for (int j = 0; j < 4; j++) {
        version = version + 0.1;
        std::ostringstream streamObj;
        // Set Fixed -Point Notation
        streamObj << std::fixed;
        // Set precision to 1 digit
        streamObj << std::setprecision(1);
        //Add double to stream
        streamObj << version;
        // Get string from output string stream
        std::string strVersion = streamObj.str();
        std::size_t found = sVivadoDir.find(strVersion);
        if (found != std::string::npos) {
          //std::cout << "Vivado Version : " << strVersion << std::endl;
          return strVersion;
        }
      }
    }
    return strVersionTmp;
  }

  void checkXclibinVersionWithTool(const xclBin *header)
  {   
    auto top = reinterpret_cast<const axlf*>(header);
    std::string xclbinVersion = xclemulation::getXclbinVersion(top);
    std::string vivadoVersion = xclemulation::getVivadoVersion();   
    if(!xclbinVersion.empty() && !vivadoVersion.empty()) {
      std::size_t found = xclbinVersion.find(vivadoVersion);
      if (found == std::string::npos) {        
        std::string warnMsg = "WARNING: XCLBIN used is generated with Vivado version " + xclbinVersion + " where as it is run with the Vivado version " + vivadoVersion + " which is not compatible. May result to weird behaviour.";
        std::cout << warnMsg << std::endl;
      }
    }
  }
  
  //Get CU index from IP_LAYOUT section for corresponding kernel name
  int getIPName2Index(const char *name, const char* buffer)
  {
    std::string errmsg;
    const uint64_t bad_addr = -1;

    if (!buffer)
    {
      errmsg = "ERROR: getIPName2Index - can't load ip_layout section";
      std::cerr << errmsg << std::endl;
      return -EINVAL;
    }

    auto map = reinterpret_cast<const ::ip_layout*>(buffer);
    if (map->m_count < 0) {
      errmsg = "ERROR: getIPName2Index - invalid ip_layout section content";
      std::cerr << errmsg << std::endl;
      return -EINVAL;
    }
    //Find out base address of the kernel in IP_LAYOUT section in XCLBIN
    uint64_t addr = bad_addr;
    for (int i = 0; i < map->m_count; i++) {
      if (strncmp((char *)map->m_ip_data[i].m_name, name,
        sizeof(map->m_ip_data[i].m_name)) == 0) {
        addr = map->m_ip_data[i].m_base_address;
        break;
      }
    }
    if (addr == bad_addr)
      return -EINVAL;
    //get all CU index vector for the correspodning ip_layout buffer.
    auto cus = xrt_core::xclbin::get_cus(map);
    auto itr = std::find(cus.begin(), cus.end(), addr);
    if (itr == cus.end())
      return -ENOENT;

    return std::distance(cus.begin(), itr);
  }

}
