
/**********
Copyright (c) 2018, Xilinx, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********/

#include "deviceinterface.h"

// protect device access
static std::mutex m_device_mtx;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DeviceInterface::LogMessage ( LogLevel Level, std::string msg )
{
    m_log->LogMessage(Level, m_log_msg_test_type + msg, m_global_config.verbosity);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DeviceInterface::DeviceInterface( Xbtest_Pfm_Def_t xbtest_pfm_def, Global_Config_t global_config, std::atomic<bool> *gAbort )
{
    m_xbtest_pfm_def = xbtest_pfm_def;
    m_abort = gAbort;

    // get log instance
    m_log = Logging::getInstance();
    m_global_config = global_config;
}

bool DeviceInterface::SetupDevice( Json_Parameters_t *DeviceParameters )
{
    Json_Parameters_t::iterator it;
    cl_int cl_err = CL_SUCCESS;
    ChkClErr_t chk_cl_err = CHK_CL_ERR_SUCCESS;

    //////////////////////////////////////////////////////////////////////
    // xclbin
    //////////////////////////////////////////////////////////////////////
    it = FindJsonParam(DeviceParameters, XCLBIN_MEMBER);
    if (it != DeviceParameters->end())
    {
        m_xclbin_filename = TestcaseParamCast<std::string>(it->second);
        LogMessage(LOG_INFO, "Using \"" + XCLBIN_MEMBER.name + "\": " + m_xclbin_filename);
    }
    else
    {
        LogMessage(LOG_FAILURE, "\"" + XCLBIN_MEMBER.name + "\" must be defined in test json file or in command line");
        return true;
    }
    // check if the xclbin exists
    std::ifstream infile(m_xclbin_filename);
    if(!infile.good())
    {
        LogMessage(LOG_FAILURE, "xclbin \"" + std::string(m_xclbin_filename) + "\" not found");
        return true;
    }

    //////////////////////////////////////////////////////////////////////
    // Device index
    //////////////////////////////////////////////////////////////////////
    bool device_idx_provided = false;
    it = FindJsonParam(DeviceParameters, DEVICE_IDX_MEMBER);
    if (it != DeviceParameters->end())
    {
        m_device_index = TestcaseParamCast<uint>(it->second);
        LogMessage(LOG_INFO, "Using provided \"" + DEVICE_IDX_MEMBER.name + "\": " + std::to_string(m_device_index));
        device_idx_provided = true;
    }
    else
    {
        LogMessage(LOG_INFO, "No device index provided");
    }

    m_device_name = m_xbtest_pfm_def.info.name; // extract the device name from the parameters
    LogMessage(LOG_INFO, "Using \"" + DEVICE_MEMBER.name + "\": " + m_device_name);

    //////////////////////////////////////////////////////////////////////
    // Define platform
    //////////////////////////////////////////////////////////////////////
    cl_err = cl::Platform::get(&cl_platforms);
    CheckClPlatformGet(cl_err, &chk_cl_err); CHK_CL_ERR_RETURN(chk_cl_err);
    LogMessage(LOG_DEBUG, "Number of platforms found: " + std::to_string(cl_platforms.size()));

    bool found_platform = false;
    cl::Platform cl_platform;
    std::string cl_platformName;

    for(uint i = 0; (i < cl_platforms.size() ) ;i++)
    {
        cl_platform = cl_platforms[i];
        cl_platformName = cl_platform.getInfo<CL_PLATFORM_NAME>(&cl_err);
        CheckClPlatformGetInfo(cl_err, "CL_PLATFORM_NAME", &chk_cl_err); CHK_CL_ERR_RETURN(chk_cl_err);
        if (cl_platformName == XILINX_PLATFORM_STR)
        {
            found_platform = true;
            break;
        }
    }
    if (found_platform == false)
    {
        LogMessage(LOG_FAILURE, "No Xilinx platform available");
        return true;
    }

    std::string cl_platformVendor       = cl_platform.getInfo<CL_PLATFORM_VENDOR>(&cl_err);        CheckClPlatformGetInfo(cl_err, "CL_PLATFORM_VENDOR",       &chk_cl_err); CHK_CL_ERR_RETURN(chk_cl_err);
    std::string cl_platformVersion      = cl_platform.getInfo<CL_PLATFORM_VERSION>(&cl_err);       CheckClPlatformGetInfo(cl_err, "CL_PLATFORM_VERSION",      &chk_cl_err); CHK_CL_ERR_RETURN(chk_cl_err);
    std::string cl_platformProfile      = cl_platform.getInfo<CL_PLATFORM_PROFILE>(&cl_err);       CheckClPlatformGetInfo(cl_err, "CL_PLATFORM_PROFILE",      &chk_cl_err); CHK_CL_ERR_RETURN(chk_cl_err);
    std::string cl_platformExtensions   = cl_platform.getInfo<CL_PLATFORM_EXTENSIONS>(&cl_err);    CheckClPlatformGetInfo(cl_err, "CL_PLATFORM_EXTENSIONS",   &chk_cl_err); CHK_CL_ERR_RETURN(chk_cl_err);

    LogMessage(LOG_DEBUG, "Platform Information:");
    LogMessage(LOG_DEBUG, "\t - Name:       " + cl_platformName);
    LogMessage(LOG_DEBUG, "\t - Vendor:     " + cl_platformVendor);
    LogMessage(LOG_DEBUG, "\t - Version:    " + cl_platformVersion);
    LogMessage(LOG_DEBUG, "\t - Profile:    " + cl_platformProfile);
    LogMessage(LOG_DEBUG, "\t - Extensions: " + cl_platformExtensions);

    //////////////////////////////////////////////////////////////////////
    // Find device
    //////////////////////////////////////////////////////////////////////
    // Find all the devices on the "Xilinx" platform
    bool found_device = false;
    cl_devices.clear();
    cl_err = cl_platform.getDevices(CL_DEVICE_TYPE_ACCELERATOR, &cl_devices);
    CheckClPlatformGetDevices(cl_err, "CL_DEVICE_TYPE_ACCELERATOR", &chk_cl_err); CHK_CL_ERR_RETURN(chk_cl_err);

    // if device_idx is provided, use it. Otherwise use the first compatible card available
    if (device_idx_provided == true)
    {
        if(m_device_index >= (uint)cl_devices.size())
        {
            LogMessage(LOG_FAILURE, "No device found at index: " + std::to_string(m_device_index) + ". Try 'xbutil list'");
            return true;
        }
        cl_device = cl_devices[m_device_index];
        std::string cl_deviceName = cl_device.getInfo<CL_DEVICE_NAME>(&cl_err);
        CheckClDeviceGetInfo(cl_err, "CL_DEVICE_NAME", &chk_cl_err); CHK_CL_ERR_RETURN(chk_cl_err);

        if (StrMatchNoCase(m_device_name, cl_deviceName))
        {
            found_device = true;
        }
    }
    else
    {
        for (uint j = 0 ; j < cl_devices.size() ; j++)
        {
            cl_device = cl_devices[j];
            std::string cl_deviceName = cl_device.getInfo<CL_DEVICE_NAME>(&cl_err);
            CheckClDeviceGetInfo(cl_err, "CL_DEVICE_NAME", &chk_cl_err); CHK_CL_ERR_RETURN(chk_cl_err);

            if (StrMatchNoCase(m_device_name, cl_deviceName))
            {
                found_device = true;
            }

            if (found_device == true)
            {
                m_device_index = j;
                InsertJsonParam<uint>(DeviceParameters, DEVICE_IDX_MEMBER, m_device_index);
                LogMessage(LOG_INFO, "Using device found at index: " + std::to_string(m_device_index));
                break;
            }
        }
    }

    if (found_device == false)
    {
        LogMessage(LOG_FAILURE, "No matching device found for: " + m_device_name);
        return true;
    }

    //////////////////////////////////////////////////////////////////////
    // Create parser
    //////////////////////////////////////////////////////////////////////
    m_xbutil_dump_parser = new XbutilDumpParser(std::to_string(m_device_index), m_global_config, m_abort);

    //////////////////////////////////////////////////////////////////////
    // OpenCL
    //////////////////////////////////////////////////////////////////////
    cl_context = cl::Context(cl_device, NULL, NULL, NULL, &cl_err); // Create a context
    CheckClContextConstructor(cl_err, &chk_cl_err); CHK_CL_ERR_RETURN(chk_cl_err);

    cl_cmd_queue = cl::CommandQueue(cl_context, cl_device, CL_QUEUE_PROFILING_ENABLE, &cl_err); // Create a command queue
    CheckClCommandQueueConstructor(cl_err, "CL_QUEUE_PROFILING_ENABLE", &chk_cl_err); CHK_CL_ERR_RETURN(chk_cl_err);

    LogMessage(LOG_INFO, "Loading binary: " + m_xclbin_filename);
    std::ifstream bin_file(m_xclbin_filename, std::ifstream::binary); // Read the xclbin
    bin_file.seekg (0, bin_file.end);
    unsigned nb = bin_file.tellg();
    bin_file.seekg (0, bin_file.beg);
    char *buf = new char [nb];
    bin_file.read(buf, nb);

    // creating Program from Binary File
    cl::Program::Binaries bins;
    bins.push_back({buf,nb});

    auto begin = std::chrono::steady_clock::now();
    cl_program = cl::Program(cl_context, {cl_device}, bins, NULL, &cl_err); // Download the xclbin on the device
    auto end = std::chrono::steady_clock::now();

    CheckClProgramConstructor(cl_err, &chk_cl_err); CHK_CL_ERR_RETURN(chk_cl_err);

    auto ticks = std::chrono::duration_cast<std::chrono::microseconds>(end-begin);
    m_xclbin_download_time = ticks.count();
    LogMessage(LOG_DEBUG, "Binary download time: " + std::to_string(m_xclbin_download_time/1000) + " ms");

    //////////////////////////////////////////////////////////////////////
    // Get Memory Topology
    //////////////////////////////////////////////////////////////////////

    bool ret_failure = false;
    ret_failure |= GetXclbinDumpInfo();
    if (ret_failure == true)
    {
        LogMessage(LOG_FAILURE, "Failed to get xbutil dump info");
        return true;
    }
    LogMessage(LOG_INFO, "Binary UUID: " + m_xclbin_uuid);
    PrintUsedMemTopology(m_mem_topology);

    if (m_xbtest_pfm_def.memory.ddr_exists == true)
    {
        uint mem_count      = GetNumMemTopologyType(m_mem_topology, "DDR");
        uint xbtest_pfm_qty = m_xbtest_pfm_def.memory.ddr.quantity;
        if (mem_count != xbtest_pfm_qty)
            LogMessage(LOG_DEBUG, "Memory Topology DDR count: " + std::to_string(mem_count) + " does not match DDR quantity in " + PLATDEF_JSON_NAME + ": " + std::to_string(xbtest_pfm_qty));
    }
    if (m_xbtest_pfm_def.memory.hbm_exists == true)
    {
        uint mem_count      = GetNumMemTopologyType(m_mem_topology, "HBM");
        uint xbtest_pfm_qty = m_xbtest_pfm_def.memory.hbm.quantity;
        if (mem_count != xbtest_pfm_qty)
            LogMessage(LOG_DEBUG, "Memory Topology HBM count: " + std::to_string(mem_count) + " does not match HBM quantity in " + PLATDEF_JSON_NAME + ": " + std::to_string(xbtest_pfm_qty));
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DeviceInterface::CheckXCLBINDownloadTime()
{
    bool ret = false;
    int expected_time = m_xbtest_pfm_def.runtime.download_time;

    if (expected_time > -1)
    {
        if (m_xclbin_download_time > (uint)expected_time)
        {
            ret = true;
            LogMessage(LOG_ERROR, "Binary download time greater than expected: "   + std::to_string(m_xclbin_download_time/1000) + " ms > " + std::to_string((int)expected_time/1000) + " ms");
        }
        else
        {
            LogMessage(LOG_PASS, "Binary download time within tolerance: "         + std::to_string(m_xclbin_download_time/1000) + " ms < " + std::to_string((int)expected_time/1000) + " ms");
        }
    }
    else
    {
        LogMessage(LOG_PASS, "Binary download time not checked");
    }

    return ret;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint DeviceInterface::CheckClocks()
{
    bool ret_failure = false;

    LogMessage(LOG_INFO, "Checking xclbin clock frequencies");

    m_xbutil_dump_parser->ClearParser();
    ret_failure |= m_xbutil_dump_parser->Parse();
    if (ret_failure == true) return 1;

    for (uint i=0; i<m_xbtest_pfm_def.info.num_clocks; i++)
    {
        std::vector<std::string> node_title = m_xbtest_pfm_def.info.clocks[i].name;
        uint expected_freq                  = m_xbtest_pfm_def.info.clocks[i].frequency;

        std::string tmp_str;
        ret_failure |= m_xbutil_dump_parser->ExtractNodeValueStr(node_title, &tmp_str);
        if (ret_failure == true) return 1;

        uint actual_freq;
        ret_failure |= ConvString2Num<unsigned>(tmp_str, &actual_freq);
        if (ret_failure == true)
        {
            LogMessage(LOG_FAILURE, "CheckClocks: Failed to convert xbutil dump value: " + StrVectToStr(node_title, "."));
            return 1;
        }

        if (expected_freq != actual_freq)
        {
            LogMessage(LOG_ERROR, "Clock \"" + StrVectToStr(node_title, ".") + "\" frequency: xclbin value " + std::to_string(actual_freq) + " MHz, expected " + std::to_string(expected_freq) + " MHz");
            return 2;
        }
        else
        {
            LogMessage(LOG_PASS, "Clock \"" + StrVectToStr(node_title, ".") + "\" frequency " + std::to_string(expected_freq) + " MHz");
        }
    }
    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::string DeviceInterface::MemTypeIndexToMemTag ( std::string mem_type, uint mem_index )
{
    std::string tag;
    if (StrMatchNoCase(mem_type, TEST_MEM_TYPE_BANK))
        tag = mem_type + std::to_string(mem_index);
    else if ((StrMatchNoCase(mem_type, TEST_MEM_TYPE_DDR)) || (StrMatchNoCase(mem_type, TEST_MEM_TYPE_HBM)))
        tag = mem_type + "[" + std::to_string(mem_index) + "]";
    else
        tag = "";
    return tag;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DeviceInterface::IsTagOfType ( std::string mem_tag, std::string mem_type )
{
    bool is_tag_of_type = false;

    if ((StrMatchNoCase(mem_type, TEST_MEM_TYPE_DDR)) || (StrMatchNoCase(mem_type, TEST_MEM_TYPE_BANK)))
    {
        if (   (StrMatchNoCase(mem_tag, 0, TEST_MEM_TYPE_DDR.size(),  TEST_MEM_TYPE_DDR))
            || (StrMatchNoCase(mem_tag, 0, TEST_MEM_TYPE_BANK.size(), TEST_MEM_TYPE_BANK)))
            is_tag_of_type = true;
    }
    else
    {
        if (StrMatchNoCase(mem_tag, 0, mem_type.size(), mem_type))
            is_tag_of_type = true;
    }

    return is_tag_of_type;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DeviceInterface::ForceNotUseBankTag ( std::string *mem_tag )
{
    if ((StrMatchNoCase(*mem_tag, 0, TEST_MEM_TYPE_BANK.size(), TEST_MEM_TYPE_BANK)) && (mem_tag->size() == TEST_MEM_TYPE_BANK.size() + 1))
    {
        std::string mem_index = mem_tag->substr(mem_tag->size()-1,mem_tag->size());  // One extra character in tag which is memory index
        *mem_tag = TEST_MEM_TYPE_DDR + "[" + mem_index + "]";
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DeviceInterface::GetInMemTopology ( MemTopology_t mem_topology, std::string mem_type, std::string mem_tag, uint mem_index, MemData_t *mem_data, uint *mem_topol_idx )
{
    bool test_failure = true;
    std::string tag_bank = "";

    if (StrMatchNoCase(mem_type, TEST_MEM_TYPE_DDR)) // For "DDR" tag, also look for "bank" tag
        tag_bank = MemTypeIndexToMemTag(TEST_MEM_TYPE_BANK, mem_index);

    for (uint i = 0; i < mem_topology.mem_count; i++ )
    {
        std::string mem_topology_tag = mem_topology.mem_data[i].tag;
        if (((StrMatchNoCase(mem_type, TEST_MEM_TYPE_DDR)) && ((StrMatchNoCase(mem_topology_tag, mem_tag)) || (StrMatchNoCase(mem_topology_tag, tag_bank))))
         || ((StrMatchNoCase(mem_type, TEST_MEM_TYPE_HBM)) && ( StrMatchNoCase(mem_topology_tag, mem_tag) )))
        {
            if (mem_topology.mem_data[i].enabled == false)
            {
                LogMessage(LOG_ERROR, mem_tag + " **UNUSED** in Memory Topology, try xbutil query");
            }
            else
            {
                *mem_data = mem_topology.mem_data[i];
                *mem_topol_idx = i;
                test_failure = false;
                break;
            }
        }
    }
    if (test_failure == false)
    {
        LogMessage(LOG_DEBUG, "Found " + mem_tag + " in Memory Topology at index: " + std::to_string(*mem_topol_idx));
        PrintMemData(*mem_topol_idx, *mem_data);
    }
    else
    {
        LogMessage(LOG_ERROR, mem_tag + " not found in Memory Topology");
    }
    return test_failure;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DeviceInterface::FindNextUsedInMemTopology ( MemTopology_t mem_topology, std::string mem_type, uint *mem_topol_idx, std::string *mem_tag, MemData_t *mem_data )
{
    bool mem_used_found = false;

    for (uint i = (*mem_topol_idx); i < mem_topology.mem_count; i++ )
    {
        if (mem_topology.mem_data[i].enabled == true)
        {
            std::string mem_topology_tag = mem_topology.mem_data[i].tag;
            if (IsTagOfType(mem_topology_tag,mem_type))
            {
                *mem_data       = mem_topology.mem_data[i];
                *mem_topol_idx  = i;
                *mem_tag        = mem_topology_tag;
                mem_used_found  = true;
                LogMessage(LOG_DEBUG,"Found " + *mem_tag + " in Memory Topology at index: " + std::to_string(*mem_topol_idx));
                PrintMemData(*mem_topol_idx, *mem_data);
                break;
            }
        }
    }
    return mem_used_found;

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DeviceInterface::SetupKernels()
{
    cl_int cl_err = CL_SUCCESS;
    ChkClErr_t chk_cl_err = CHK_CL_ERR_SUCCESS;

    LogMessage(LOG_INFO, "Setup compute units");

    // Create kernel in program:
    // All kernels found in program are created in the vector of OpenCL kernels "kernels"
    cl_err = cl_program.createKernels(&kernels);
    CheckClProgramCreateKernels(cl_err, &chk_cl_err); CHK_CL_ERR_RETURN(chk_cl_err);

    if (kernels.size() == 0)
    {
        LogMessage(LOG_FAILURE, "No compute unit found in program");
        return true;
    }

    for (int kernel_type = 0; kernel_type < NUM_KERNEL_TYPE; kernel_type++)
        m_num_krnls[kernel_type] = 0;

    for (uint i = 0; i < kernels.size(); i++)
    {
        // Get the kernel name. The kernel name must match the following formats:
        // - krnl_powertest_slr<c_krnl_slr>
        // - krnl_memtest_<"hbm"/"ddr">_<%02d,c_num_used_m_axi>_<%02d,c_num_used_mem>_<%02d,c_mem_krnl_inst>
        // - krnl_gt_test<c_gt_index>
        // - krnl_gt_mac_test<c_gt_index>
        std::string kernel_info_name = kernels[i].getInfo<CL_KERNEL_FUNCTION_NAME>(&cl_err);
        CheckClKernelGetInfo(cl_err, "kernels[" + std::to_string(i) + "]", "CL_KERNEL_FUNCTION_NAME", &chk_cl_err); CHK_CL_ERR_RETURN(chk_cl_err);

        // Determine kernel type
        int kernel_type = KRNL_TYPE_UNKNOWN;

        // Compares KRNL_PWR_NAME.size() characters from 0 index of kernel_info_name with string KRNL_PWR_NAME
        if (kernel_info_name.compare(0, KRNL_PWR_NAME.size(), KRNL_PWR_NAME) == 0)
            kernel_type = KRNL_TYPE_PWR;
        else if (kernel_info_name.compare(0, KRNL_MEM_DDR_NAME.size(), KRNL_MEM_DDR_NAME) == 0)
            kernel_type = KRNL_TYPE_MEM_DDR;
        else if (kernel_info_name.compare(0, KRNL_MEM_HBM_NAME.size(), KRNL_MEM_HBM_NAME) == 0)
            kernel_type = KRNL_TYPE_MEM_HBM;
        else if (kernel_info_name.compare(0, KRNL_GT_NAME.size(), KRNL_GT_NAME) == 0)
            kernel_type = KRNL_TYPE_GT;
        else if (kernel_info_name.compare(0, KRNL_GT_MAC_NAME.size(), KRNL_GT_MAC_NAME) == 0)
            kernel_type = KRNL_TYPE_GT_MAC;

        if (kernel_type == KRNL_TYPE_PWR)
        {
            LogMessage(LOG_DEBUG, "Get Power compute unit info for compute unit: " + kernel_info_name);
            // Get the power kernel slr in power kernel name (last character of the kernel name, integer between 0 and 9)
            // This will be used to check the build info
            bool get_info_failure = false;
            if (kernel_info_name.size() != KRNL_PWR_NAME.size() + 1)
            {
                get_info_failure = true;
            }
            else
            {
                int power_kernel_slr;
                std::string power_kernel_slr_str = kernel_info_name.substr(KRNL_PWR_NAME.size(), 1); // Get last character
                get_info_failure = ConvString2Num<int>(power_kernel_slr_str, &power_kernel_slr);
                if (get_info_failure == true)
                {
                    LogMessage(LOG_WARN, "Failed to get compute unit info power_kernel_slr in compute unit name: " + kernel_info_name);
                    LogMessage(LOG_WARN, "\t Info power_kernel_slr = " + power_kernel_slr_str);
                }
                if (get_info_failure == false)
                    m_power_kernel_slr[m_num_krnls[KRNL_TYPE_PWR]] = power_kernel_slr; // Successfully got kernel slr
            }
            if (get_info_failure == true) // if no conversion could be performed
            {
                LogMessage(LOG_WARN, "Compute unit type power is detected but cannot get compute unit SLR in compute unit name: " + kernel_info_name);
                kernel_type = KRNL_TYPE_UNKNOWN;
            }
        }
        else if ((kernel_type == KRNL_TYPE_MEM_DDR) || (kernel_type == KRNL_TYPE_MEM_HBM))
        {
            // Get in memory kernel name: c_num_used_m_axi, c_num_used_mem, c_m00_axi_data_width
            // This will be used to check the build info
            // format is: krnl_memtest_<"hbm"/"ddr">_<%02d,c_num_used_m_axi>_<%02d,c_num_used_mem>_<%02d,c_mem_krnl_inst>
            // (note <%02d,param> for 0 2-digit param number with 0 padding)
            bool get_info_failure = false;
            std::string kernel_base_name;
            if (kernel_type == KRNL_TYPE_MEM_DDR)
            {
                LogMessage(LOG_DEBUG, "Get DDR memory compute unit info for compute unit: " + kernel_info_name);
                kernel_base_name = KRNL_MEM_DDR_NAME;
            }
            else if (kernel_type == KRNL_TYPE_MEM_HBM)
            {
                LogMessage(LOG_DEBUG, "Get HBM memory compute unit info for compute unit: " + kernel_info_name);
                kernel_base_name = KRNL_MEM_HBM_NAME;
            }

            if ((kernel_info_name.size() != kernel_base_name.size() + 9))
            {
                get_info_failure = true;
            }
            else
            {
                int mem_kernel_num_core;
                int mem_kernel_num_mem;
                int mem_kernel_inst;

                std::string mem_kernel_num_core_str = kernel_info_name.substr(kernel_base_name.size() + 1, 2);
                std::string mem_kernel_num_mem_str  = kernel_info_name.substr(kernel_base_name.size() + 4, 2);
                std::string mem_kernel_inst_str     = kernel_info_name.substr(kernel_base_name.size() + 7, 2);

                if (get_info_failure == false)
                {
                    get_info_failure = ConvString2Num<int>(mem_kernel_num_core_str, &mem_kernel_num_core);
                    if (get_info_failure == true)
                    {
                        LogMessage(LOG_WARN, "Failed to get kernel info mem_kernel_num_core in kernel name: " + kernel_info_name);
                        LogMessage(LOG_WARN, "\t Info mem_kernel_num_core = " + mem_kernel_num_core_str);
                    }
                }
                if (get_info_failure == false)
                {
                    get_info_failure = ConvString2Num<int>(mem_kernel_num_mem_str, &mem_kernel_num_mem);
                    if (get_info_failure == true)
                    {
                        LogMessage(LOG_WARN, "Failed to get kernel info mem_kernel_num_mem in kernel name: " + kernel_info_name);
                        LogMessage(LOG_WARN, "\t Info mem_kernel_num_mem = " + mem_kernel_num_mem_str);
                    }
                }
                if (get_info_failure == false)
                {
                    get_info_failure = ConvString2Num<int>(mem_kernel_inst_str, &mem_kernel_inst);
                    if (get_info_failure == true)
                    {
                        LogMessage(LOG_WARN, "Failed to get kernel info mem_kernel_inst in kernel name: " + kernel_info_name);
                        LogMessage(LOG_WARN, "\t Info mem_kernel_inst = " + mem_kernel_inst_str);
                    }
                }
                if (get_info_failure == false)  // Successfully got kernel infos
                {
                    m_mem_kernel_num_core[kernel_type][m_num_krnls[kernel_type]]    = mem_kernel_num_core;
                    m_mem_kernel_num_mem[kernel_type][m_num_krnls[kernel_type]]     = mem_kernel_num_mem;
                    m_mem_kernel_inst[kernel_type][m_num_krnls[kernel_type]]        = mem_kernel_inst;
                }
            }
            if (get_info_failure == true) // if no conversion could be performed
            {
                if (kernel_type == KRNL_TYPE_MEM_DDR)
                    LogMessage(LOG_WARN, "Compute unit type DDR memory is detected but cannot get compute unit infos in compute unit name: " + kernel_info_name);
                else if (kernel_type == KRNL_TYPE_MEM_HBM)
                    LogMessage(LOG_WARN, "Compute unit type HBM memory is detected but cannot get compute unit infos in compute unit name: " + kernel_info_name);

                kernel_type = KRNL_TYPE_UNKNOWN;
            }
        }
        else if (kernel_type == KRNL_TYPE_GT)
        {
            LogMessage(LOG_DEBUG, "No info for compute unit: " + kernel_info_name);
        }
        else if (kernel_type == KRNL_TYPE_GT_MAC)
        {
            LogMessage(LOG_DEBUG, "No info for compute unit: " + kernel_info_name);
        }

        // Add kernel index and OpencL kernel to the used kernels depending on the kernel type
        cl_kernel_names[kernel_type][m_num_krnls[kernel_type]] = kernel_info_name;
        cl_kernels[kernel_type][m_num_krnls[kernel_type]] = kernels[i];
        m_num_krnls[kernel_type]++;
    }

    int num_known_krnls = 0;
	num_known_krnls += m_num_krnls[KRNL_TYPE_PWR];
	num_known_krnls += m_num_krnls[KRNL_TYPE_MEM_DDR];
	num_known_krnls += m_num_krnls[KRNL_TYPE_MEM_HBM];

    if (kernels.size() != 0)
        LogMessage(LOG_INFO, "Total number of Compute Unit(s) found in program: " + std::to_string(kernels.size()));
    else
        LogMessage(LOG_WARN, "No Compute Unit(s) found in program");

    if (m_num_krnls[KRNL_TYPE_PWR] != 0)
        LogMessage(LOG_INFO, "Found " + std::to_string(m_num_krnls[KRNL_TYPE_PWR])     + " Power Compute Unit(s)");
    if (m_num_krnls[KRNL_TYPE_MEM_DDR] != 0)
        LogMessage(LOG_INFO, "Found " + std::to_string(m_num_krnls[KRNL_TYPE_MEM_DDR]) + " DDR Memory Compute Unit(s)");
    if (m_num_krnls[KRNL_TYPE_MEM_HBM] != 0)
        LogMessage(LOG_INFO, "Found " + std::to_string(m_num_krnls[KRNL_TYPE_MEM_HBM]) + " HBM Memory Compute Unit(s)");
    if (m_num_krnls[KRNL_TYPE_GT] != 0)
        LogMessage(LOG_INFO, "Found " + std::to_string(m_num_krnls[KRNL_TYPE_GT])      + " GT Compute Unit(s)");
    if (m_num_krnls[KRNL_TYPE_GT_MAC] != 0)
        LogMessage(LOG_INFO, "Found " + std::to_string(m_num_krnls[KRNL_TYPE_GT_MAC])  + " GT MAC Compute Unit(s)");

    if (m_num_krnls[KRNL_TYPE_UNKNOWN] > 0)
    {
        LogMessage(LOG_WARN, "Found " + std::to_string(m_num_krnls[KRNL_TYPE_UNKNOWN]) + " Unknown Compute Unit(s)");
        for (int i = 0; i <  m_num_krnls[KRNL_TYPE_UNKNOWN]; i++)
            LogMessage(LOG_WARN, "\t - Unknown Compute Unit " + std::to_string(i) + " : " + GetClKernelNames(KRNL_TYPE_UNKNOWN,i));
    }


    for (int kernel_type=0; kernel_type < NUM_KERNEL_TYPE-1; kernel_type++)
    {
        for (int kernel_idx=0; kernel_idx < m_num_krnls[kernel_type]; kernel_idx++)
        {
            std::string kernel_info_name = GetClKernelNames(kernel_type,kernel_idx);
            cl_err = cl_kernels[kernel_type][kernel_idx].setArg(0, 0);    CheckClKernelSetArg(cl_err, kernel_info_name, "0", &chk_cl_err); CHK_CL_ERR_RETURN(chk_cl_err);
            cl_err = cl_kernels[kernel_type][kernel_idx].setArg(1, 0);    CheckClKernelSetArg(cl_err, kernel_info_name, "1", &chk_cl_err); CHK_CL_ERR_RETURN(chk_cl_err);
            cl_err = cl_kernels[kernel_type][kernel_idx].setArg(2, 0);    CheckClKernelSetArg(cl_err, kernel_info_name, "2", &chk_cl_err); CHK_CL_ERR_RETURN(chk_cl_err);
            cl_err = cl_kernels[kernel_type][kernel_idx].setArg(3, 0);    CheckClKernelSetArg(cl_err, kernel_info_name, "3", &chk_cl_err); CHK_CL_ERR_RETURN(chk_cl_err);
        }
    }

    m_xclbinutil_parser = new XclbinUtilParser(m_device_name, m_device_index, m_xclbin_filename, m_xclbin_uuid, m_global_config, m_abort);
    if (m_xclbinutil_parser->ParseConnectivity() == true)
    {
        LogMessage(LOG_FAILURE, "Failed to get xclbin connectivity");
        return true;
    }
    m_connectivity = m_xclbinutil_parser->GetConnectivity();

    PrintCuIndexNameMap();
    cl_m00_axi_buffer.clear();
    cl_m00_axi_mem_topology_index.clear();
    cl_m00_axi_buffer_origin.clear();

    // create a number of buffer based on the device type except KRNL_TYPE_UNKNOWN
    for (int kernel_type=0; kernel_type < NUM_KERNEL_TYPE-1; kernel_type++)
    {
        // create buffers
        for (int kernel_idx=0; kernel_idx < m_num_krnls[kernel_type]; kernel_idx++)
        {
            std::string kernel_info_name    = GetClKernelNames(kernel_type,kernel_idx);
            LogMessage(LOG_DEBUG, "Creating m00_axi sub-buffer for compute unit: " + kernel_info_name);

            uint64_t buffer_size = M00_AXI_SUB_SIZE_BYTES; // default
            if (kernel_type == KRNL_TYPE_GT_MAC)
                buffer_size = GT_MAC_BUF_SIZE * sizeof(uint32_t);

            // Get memory compute unit index
            bool cu_found = false;
            uint cu_index;
            for(uint i = 0; i < m_cu_index_name_map.name.size() ;i++)
            {
                // Expected format example:
                //      - m_cu_index_name_map.name[i] ==> "krnl_powertest_slr2:krnl_powertest_slr2_1",
                //      - kernel_info_name            ==> "krnl_powertest_slr2"
                // Check first characters of m_cu_index_name_map.name[i] match kernel_info_name
                if (m_cu_index_name_map.name[i].compare(0, kernel_info_name.size(), kernel_info_name) == 0)
                {
                    cu_found = true;
                    cu_index = i;
                    break;
                }
            }
            if (cu_found == false)
            {
                LogMessage(LOG_FAILURE, "Index not found for compute unit: " + kernel_info_name);
                return true;
            }

            // Get memory topology index for M00_AXI
            uint connection_found = false;
            uint mem_topology_index;
            for (auto connection : m_connectivity.m_connection)
            {
                if ((connection.m_ip_layout_index == cu_index) && (connection.arg_index == M00_ARG_INDEX))
                {
                    connection_found = true;
                    mem_topology_index = connection.mem_data_index;
                    break;
                }
            }
            if (connection_found == false)
            {
                LogMessage(LOG_FAILURE, "Connection of m00_axi not found for compute unit: " + kernel_info_name);
                return true;
            }
            LogMessage(LOG_DEBUG, "Found connection of m00_axi for compute unit: " + kernel_info_name + " at memory topology index: " + std::to_string(mem_topology_index));

            bool buffer_exists = false;
            uint buffer_index;
            // If buffer already created, then get buffer index
            for (uint i = 0; i < cl_m00_axi_mem_topology_index.size(); i++)
            {
                if (cl_m00_axi_mem_topology_index[i] == mem_topology_index)
                {
                    buffer_exists = true;
                    buffer_index = i;
                    break;
                }
            }
            // If buffer not found, then create it
            if (buffer_exists == false)
            {
                LogMessage(LOG_DEBUG, "Creating buffer for memory topology index: " + std::to_string(mem_topology_index));

                cl_mem_ext_ptr_t cl_mem_ext_ptr = {0};
                cl_mem_ext_ptr.param    = 0;
                cl_mem_ext_ptr.obj      = NULL;
                cl_mem_ext_ptr.flags    = mem_topology_index | XCL_MEM_TOPOLOGY;

                cl_m00_axi_buffer.push_back(cl::Buffer(cl_context, CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX, M00_AXI_BUFF_SIZE_BYTES, &cl_mem_ext_ptr, &cl_err));
                CheckClBufferConstructor(cl_err, "cl_m00_axi_buffer (mem topology index = " + std::to_string(mem_topology_index) + ")", "CL_MEM_READ_WRITE", &chk_cl_err); CHK_CL_ERR_RETURN(chk_cl_err);

                buffer_index = cl_m00_axi_buffer.size() - 1; // set buffer index

                cl_m00_axi_buffer_origin.push_back(0); // Init ptr origin
                cl_m00_axi_mem_topology_index.push_back(mem_topology_index); // Save memory topology index to then check if a buffer has already been created for this memeory
            }
            else
            {
                LogMessage(LOG_DEBUG, "Existing buffer for memory topology index: " + std::to_string(mem_topology_index));
            }

            cl_buffer_region cl_m00_axi_buffer_region;
            cl_m00_axi_buffer_region.size   = buffer_size;
            cl_m00_axi_buffer_region.origin = cl_m00_axi_buffer_origin[buffer_index];
            cl_m00_axi_buffer_origin[buffer_index] += buffer_size;

            cl_m00_axi_ptr[kernel_type][kernel_idx] = cl_m00_axi_buffer[buffer_index].createSubBuffer(CL_MEM_READ_WRITE, CL_BUFFER_CREATE_TYPE_REGION, &cl_m00_axi_buffer_region, &cl_err);

            std::string sub_buff_name = "cl_m_axi_ptr[" + std::to_string(kernel_type) + "][" + std::to_string(kernel_idx) + "]";
            CheckClCreateSubBuffer(cl_err, sub_buff_name, "CL_MEM_READ_WRITE", &chk_cl_err); CHK_CL_ERR_RETURN(chk_cl_err);

            if (kernel_type == KRNL_TYPE_MEM_DDR)
            {
                LogMessage(LOG_DEBUG, "Creating buffer of tested memory for compute unit: " + kernel_info_name);
                // DDR index obtained from kernel instance number in kernel name.
                // Thus Buffer can be allocated now for DDR memory kernel
                int         kernel_core_idx = 0; // Only 1 port per kernel for DDR
                std::string mem_type        = "DDR";
                uint        mem_index       = m_mem_kernel_inst[kernel_type][kernel_idx];
                std::string mem_tag         = MemTypeIndexToMemTag(mem_type, mem_index);
                MemData_t   m_mem_data;
                uint        mem_topol_idx;

                bool get_in_mem_topology_failure = GetInMemTopology(m_mem_topology, mem_type, mem_tag, mem_index, &m_mem_data, &mem_topol_idx);
                if (get_in_mem_topology_failure == true)
                {
                    LogMessage(LOG_FAILURE, "Memory not found in Memory Topology while allocating DDR buffer, check with xbutil query if memory available: DDR[" + std::to_string(mem_index) + "]");
                    return true;
                }
                cl_mem_ext_ptr_t cl_mem_ext_ptr = {0};
                cl_mem_ext_ptr.param    = 0;
                cl_mem_ext_ptr.obj      = NULL;
                cl_mem_ext_ptr.flags    = ((unsigned)mem_topol_idx) | XCL_MEM_TOPOLOGY;

                std::string buff_name = "cl_m_axi_ptr[" + std::to_string(kernel_type) + "][" + std::to_string(kernel_idx) + "][" + std::to_string(kernel_core_idx) + "]";

                cl_m_axi_ptr[kernel_type][kernel_idx][kernel_core_idx] = cl::Buffer(cl_context, CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX, M_AXI_DDR_SIZE_BYTES, &cl_mem_ext_ptr, &cl_err);
                CheckClBufferConstructor(cl_err, buff_name, "CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX", &chk_cl_err); CHK_CL_ERR_RETURN(chk_cl_err);
            }
            else if (kernel_type == KRNL_TYPE_MEM_HBM)
            {
                LogMessage(LOG_DEBUG, "Creating buffer of each tested memory for compute unit: " + kernel_info_name);

                for (int kernel_core_idx = 0; kernel_core_idx < m_mem_kernel_num_core[kernel_type][kernel_idx]; kernel_core_idx++)
                {
                    std::string buff_name = "cl_m_axi_ptr[" + std::to_string(kernel_type) + "][" + std::to_string(kernel_idx) + "][" + std::to_string(kernel_core_idx) + "]";

                    // For HBM, used default allocation, memory address offset set in kernel is checked during memory test
                    cl_m_axi_ptr[kernel_type][kernel_idx][kernel_core_idx] = cl::Buffer(cl_context, CL_MEM_READ_WRITE, M_AXI_TMP_HBM_SIZE_BYTES, NULL, &cl_err);
                    CheckClBufferConstructor(cl_err, buff_name, "CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX", &chk_cl_err); CHK_CL_ERR_RETURN(chk_cl_err);
                }
            }
        }
    }

    // Set arguments
    for (int kernel_type=0; kernel_type < NUM_KERNEL_TYPE-1; kernel_type++)
    {
        for (int kernel_idx=0; kernel_idx < m_num_krnls[kernel_type]; kernel_idx++)
        {
            std::string kernel_info_name = GetClKernelNames(kernel_type, kernel_idx);
            int kernel_core_idx = 0; // Only 1 port per kernel for DDR

            cl_err = cl_kernels[kernel_type][kernel_idx].setArg(4, cl_m00_axi_ptr[kernel_type][kernel_idx]);
            CheckClKernelSetArg(cl_err, kernel_info_name, "4", &chk_cl_err); CHK_CL_ERR_RETURN(chk_cl_err);

            if (kernel_type == KRNL_TYPE_MEM_DDR)
            {
                cl_err = cl_kernels[kernel_type][kernel_idx].setArg(5+kernel_core_idx, cl_m_axi_ptr[kernel_type][kernel_idx][kernel_core_idx]);
                CheckClKernelSetArg(cl_err, kernel_info_name, std::to_string(5+kernel_core_idx), &chk_cl_err); CHK_CL_ERR_RETURN(chk_cl_err);
            }
            else if (kernel_type == KRNL_TYPE_MEM_HBM)
            {
                for (int kernel_core_idx = 0; kernel_core_idx < m_mem_kernel_num_core[kernel_type][kernel_idx]; kernel_core_idx++)
                {
                    cl_err = cl_kernels[kernel_type][kernel_idx].setArg(5+kernel_core_idx, cl_m_axi_ptr[kernel_type][kernel_idx][kernel_core_idx]);
                    CheckClKernelSetArg(cl_err, kernel_info_name, std::to_string(5+kernel_core_idx), &chk_cl_err); CHK_CL_ERR_RETURN(chk_cl_err);
                }
            }
        }
    }

    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DeviceInterface::~DeviceInterface() {}

std::string DeviceInterface::GetClKernelNames( int kernel_type, int kernel_idx ) { return cl_kernel_names[kernel_type][kernel_idx]; }

int DeviceInterface::GetNumKernels( int kernel_type )   { return m_num_krnls[kernel_type]; }
int DeviceInterface::GetNumPowerKernels()               { return m_num_krnls[KRNL_TYPE_PWR]; }
int DeviceInterface::GetNumMemDDRKernels()              { return m_num_krnls[KRNL_TYPE_MEM_DDR]; }
int DeviceInterface::GetNumMemHBMKernels()              { return m_num_krnls[KRNL_TYPE_MEM_HBM]; }
int DeviceInterface::GetNumGTKernels()                  { return m_num_krnls[KRNL_TYPE_GT]; }
int DeviceInterface::GetNumGTMACKernels()               { return m_num_krnls[KRNL_TYPE_GT_MAC]; }

int DeviceInterface::GetPowerKernelSLR( int kernel_idx )    { return m_power_kernel_slr[kernel_idx]; }

int DeviceInterface::GetMemKernelDDRNumCore( int kernel_idx )   { return m_mem_kernel_num_core[KRNL_TYPE_MEM_DDR][kernel_idx]; }
int DeviceInterface::GetMemKernelHBMNumCore( int kernel_idx )   { return m_mem_kernel_num_core[KRNL_TYPE_MEM_HBM][kernel_idx]; }

int DeviceInterface::GetMemKernelDDRNumMem( int kernel_idx )    { return m_mem_kernel_num_mem[KRNL_TYPE_MEM_DDR][kernel_idx]; }
int DeviceInterface::GetMemKernelHBMNumMem( int kernel_idx )    { return m_mem_kernel_num_mem[KRNL_TYPE_MEM_HBM][kernel_idx]; }

std::string DeviceInterface::GetMemKernelDDRTag( int kernel_idx, int kernel_core_idx )  { return m_mem_kernel_tag[KRNL_TYPE_MEM_DDR][kernel_idx][kernel_core_idx]; }
std::string DeviceInterface::GetMemKernelHBMTag( int kernel_idx, int kernel_core_idx )  { return m_mem_kernel_tag[KRNL_TYPE_MEM_HBM][kernel_idx][kernel_core_idx]; }

int DeviceInterface::GetMemKernelDDRDstType( int kernel_idx, int kernel_core_idx )  { return m_mem_kernel_dst_type[KRNL_TYPE_MEM_DDR][kernel_idx][kernel_core_idx]; }
int DeviceInterface::GetMemKernelHBMDstType( int kernel_idx, int kernel_core_idx )  { return m_mem_kernel_dst_type[KRNL_TYPE_MEM_HBM][kernel_idx][kernel_core_idx]; }

int DeviceInterface::GetMemKernelDDRDstIdx( int kernel_idx, int kernel_core_idx )   { return m_mem_kernel_dst_idx[KRNL_TYPE_MEM_DDR][kernel_idx][kernel_core_idx]; }
int DeviceInterface::GetMemKernelHBMDstIdx( int kernel_idx, int kernel_core_idx )   { return m_mem_kernel_dst_idx[KRNL_TYPE_MEM_HBM][kernel_idx][kernel_core_idx]; }

int DeviceInterface::GetMemKernelDDRInst( int kernel_idx )  { return m_mem_kernel_inst[KRNL_TYPE_MEM_DDR][kernel_idx]; }
int DeviceInterface::GetMemKernelHBMInst( int kernel_idx )  { return m_mem_kernel_inst[KRNL_TYPE_MEM_HBM][kernel_idx]; }

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint DeviceInterface::ReadKernel( int kernel_type, int kernel_idx, uint address )
{
    std::vector<cl::Event> waitEnqueueEvent;
    cl::Event enqueueEvent;
    cl_int cl_err = CL_SUCCESS;
    ChkClErr_t chk_cl_err = CHK_CL_ERR_SUCCESS;
    uint value;

    std::string kernel_info_name = GetClKernelNames(kernel_type, kernel_idx);

    // Set argument 0 with register address and read flag
    uint arg_data = (address << 4) & 0xfffffff0; // set bits for address
    arg_data |= 0x1; // set bit for read command
    LogMessage(LOG_DESIGNER, "ReadKernel - " + kernel_info_name + ": Set argument 0");
    cl_err = cl_kernels[kernel_type][kernel_idx].setArg(0, arg_data);
    CheckClKernelSetArg(cl_err, kernel_info_name + " (ReadKernel)", "0", &chk_cl_err); CHK_CL_ERR_ABORT_RETURN_0(chk_cl_err, m_abort);

    // Enqueue a command to execute the kernel
    LogMessage(LOG_DESIGNER, "ReadKernel - " + kernel_info_name + ": Execute kernel");
    cl_err = cl_cmd_queue.enqueueTask(cl_kernels[kernel_type][kernel_idx], NULL, &enqueueEvent);
    CheckClCommandQueueEnqueueTask(cl_err, kernel_info_name + " (ReadKernel)", &chk_cl_err); CHK_CL_ERR_ABORT_RETURN_0(chk_cl_err, m_abort);

    // Wait for the kernel to finish the execution
    LogMessage(LOG_DESIGNER, "ReadKernel - " + kernel_info_name + ": Wait for kernel to complete");
    waitEnqueueEvent.push_back(enqueueEvent);
    cl_err = cl_cmd_queue.finish();
    CheckClCommandQueueFinish(cl_err, kernel_info_name + "(ReadKernel)", &chk_cl_err); CHK_CL_ERR_ABORT_RETURN_0(chk_cl_err, m_abort);

    // Once the kernel finishes the execution, the 32-bit read data is available at the offset 0x0 of the OpenCL buffer (m00_axi)
    LogMessage(LOG_DESIGNER, "ReadKernel - " + kernel_info_name + ": Read data in memory");
    cl_err = cl_cmd_queue.enqueueReadBuffer(cl_m00_axi_ptr[kernel_type][kernel_idx], CL_TRUE, 0, sizeof(value), &value, &waitEnqueueEvent, NULL);
    CheckClCommandQueueEnqueueReadBuffer(cl_err, "cl_m00_axi_ptr[" + std::to_string(kernel_type) + "][" + std::to_string(kernel_idx) + "] (ReadKernel)", &chk_cl_err); CHK_CL_ERR_ABORT_RETURN_0(chk_cl_err, m_abort);

    //LogMessage(LOG_DESIGNER, "ReadKernel - " + kernel_info_name + " @ addr: 0x" + NumToStrHex(address) + ", value 0x" + NumToStrHex(value) );
    return (value);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint DeviceInterface::ReadPwrKernel( int kernel_idx, uint address )     { return ReadKernel(KRNL_TYPE_PWR, kernel_idx, address); }

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint DeviceInterface::ReadMemDDRKernel( int kernel_idx, int kernel_core_idx, uint address )
{
    uint krnl_core_offset = GetKrnlMemKrnlCoreOffset(kernel_core_idx); // In DDR mode, only one memory kernel core
    return ReadKernel(KRNL_TYPE_MEM_DDR, kernel_idx, krnl_core_offset | address);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint DeviceInterface::ReadMemHBMKernel( int kernel_idx, int kernel_core_idx, uint address )
{
    uint krnl_core_offset = GetKrnlMemKrnlCoreOffset(kernel_core_idx);
    return ReadKernel(KRNL_TYPE_MEM_HBM, kernel_idx, krnl_core_offset | address);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint DeviceInterface::ReadGTKernel( int kernel_idx, uint address )      { return ReadKernel(KRNL_TYPE_GT, kernel_idx, address); }
uint DeviceInterface::ReadGTMACKernel( int kernel_idx, uint address )   { return ReadKernel(KRNL_TYPE_GT_MAC, kernel_idx, address); }

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DeviceInterface::WriteKernel( int kernel_type, int kernel_idx, uint address, uint value )
{
    std::vector<cl::Event> waitEnqueueEvent, waitWriteEvent;
    cl::Event enqueueEvent, writeEvent;
    cl_int cl_err = CL_SUCCESS;
    ChkClErr_t chk_cl_err = CHK_CL_ERR_SUCCESS;

    std::string kernel_info_name = GetClKernelNames(kernel_type, kernel_idx);
    //LogMessage(LOG_DESIGNER, "WriteKernel - " + kernel_info_name + " @ addr: 0x" + NumToStrHex(address) + ", value 0x" + NumToStrHex(value));

    // Set argument 0 with register address and write flag
    uint arg_data = (address << 4) & 0xfffffff0; // set bits for address
    LogMessage(LOG_DESIGNER, "WriteKernel - " + kernel_info_name + ": Set argument 0");
    cl_err = cl_kernels[kernel_type][kernel_idx].setArg(0, arg_data);
    CheckClKernelSetArg(cl_err, kernel_info_name + " (WriteKernel)", "0", &chk_cl_err); CHK_CL_ERR_ABORT_RETURN(chk_cl_err, m_abort);

    // Set argument 1 with register value
    arg_data = value & 0xffffffff; // set bits for data
    LogMessage(LOG_DESIGNER, "WriteKernel - " + kernel_info_name + ": Set argument 1");
    cl_err = cl_kernels[kernel_type][kernel_idx].setArg(1, arg_data);
    CheckClKernelSetArg(cl_err, kernel_info_name + " (WriteKernel)", "1", &chk_cl_err); CHK_CL_ERR_ABORT_RETURN(chk_cl_err, m_abort);

    // Enqueue a command to execute the kernel
    LogMessage(LOG_DESIGNER, "WriteKernel - " + kernel_info_name + ": Execute kernel");
    cl_err = cl_cmd_queue.enqueueTask(cl_kernels[kernel_type][kernel_idx], NULL, &enqueueEvent);
    CheckClCommandQueueEnqueueTask(cl_err, kernel_info_name + " (WriteKernel)", &chk_cl_err); CHK_CL_ERR_ABORT_RETURN(chk_cl_err, m_abort);

    // Wait for the kernel to finish the execution
    LogMessage(LOG_DESIGNER, "WriteKernel - " + kernel_info_name + ": Wait for kernel to complete");
    waitEnqueueEvent.push_back(enqueueEvent);
    cl_err = cl_cmd_queue.finish();
    CheckClCommandQueueFinish(cl_err, kernel_info_name + "(WriteKernel)", &chk_cl_err); CHK_CL_ERR_ABORT_RETURN(chk_cl_err, m_abort);

    LogMessage(LOG_DESIGNER, "WriteKernel - " + kernel_info_name + ": Successfully wrote data");
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DeviceInterface::WritePwrKernel( int kernel_idx, uint address, uint value )    { WriteKernel(KRNL_TYPE_PWR, kernel_idx, address, value); }

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DeviceInterface::WriteMemDDRKernel( int kernel_idx, int kernel_core_idx, uint address, uint value )
{
    uint krnl_core_offset = GetKrnlMemKrnlCoreOffset(kernel_core_idx);
    WriteKernel(KRNL_TYPE_MEM_DDR, kernel_idx, krnl_core_offset | address, value);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DeviceInterface::WriteMemHBMKernel( int kernel_idx, int kernel_core_idx, uint address, uint value )
{
    uint krnl_core_offset = GetKrnlMemKrnlCoreOffset(kernel_core_idx);
    WriteKernel(KRNL_TYPE_MEM_HBM, kernel_idx, krnl_core_offset | address, value);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DeviceInterface::WriteGTKernel( int kernel_idx, uint address, uint value )     { WriteKernel(KRNL_TYPE_GT, kernel_idx, address, value); }

void DeviceInterface::WriteGTMACKernel( int kernel_idx, uint address, uint value )    { WriteKernel(KRNL_TYPE_GT_MAC, kernel_idx, address, value); }

void DeviceInterface::WriteGTMACKernelCmd( int kernel_idx, uint value )
{
    std::vector<cl::Event> waitEnqueueEvent, waitWriteEvent;
    cl::Event enqueueEvent, writeEvent;
    cl_int cl_err = CL_SUCCESS;
    ChkClErr_t chk_cl_err = CHK_CL_ERR_SUCCESS;
    std::string krnl_msg;

    std::string kernel_info_name = GetClKernelNames(KRNL_TYPE_GT_MAC, kernel_idx);

    // Arg 0
    uint arg_data = value & 0xffffffff; // set bits for data
    LogMessage(LOG_DESIGNER, "WriteGTMACKernelCmd - " + kernel_info_name + ": Set argument 0");
    cl_err = cl_kernels[KRNL_TYPE_GT_MAC][kernel_idx].setArg(0, arg_data); // Set argument 0 with write data
    CheckClKernelSetArg(cl_err, kernel_info_name + " (WriteGTMACKernelCmd)", "0", &chk_cl_err); CHK_CL_ERR_ABORT_RETURN(chk_cl_err, m_abort);

    // Enqueue a command to execute the kernel
    LogMessage(LOG_DESIGNER, "WriteGTMACKernelCmd - " + kernel_info_name + ": Execute kernel");
    cl_err = cl_cmd_queue.enqueueTask(cl_kernels[KRNL_TYPE_GT_MAC][kernel_idx], NULL, &enqueueEvent);
    CheckClCommandQueueEnqueueTask(cl_err, kernel_info_name + " (WriteGTMACKernelCmd)", &chk_cl_err); CHK_CL_ERR_ABORT_RETURN(chk_cl_err, m_abort);

    // Wait for the kernel to finish the execution
    LogMessage(LOG_DESIGNER, "WriteGTMACKernelCmd - " + kernel_info_name + ": Wait for kernel to complete");
    waitEnqueueEvent.push_back(enqueueEvent);
    cl_err = cl_cmd_queue.finish(); // Wait for the kernel to finish the execution
    CheckClCommandQueueFinish(cl_err, kernel_info_name + "(WriteGTMACKernelCmd)", &chk_cl_err); CHK_CL_ERR_ABORT_RETURN(chk_cl_err, m_abort);

    LogMessage(LOG_DESIGNER, "WriteGTMACKernelCmd - " + kernel_info_name + ": Successfully wrote data");
}

void DeviceInterface::WriteGTMACTrafficCfg(int kernel_idx, uint32_t *traffic_cfg )
{
    const uint WRITEBUFFER = GT_MAC_BUF_SIZE;
    std::vector<cl::Event> waitEnqueueEvent;
    cl_int cl_err = CL_SUCCESS;
    ChkClErr_t chk_cl_err = CHK_CL_ERR_SUCCESS;

    std::string kernel_info_name = GetClKernelNames(KRNL_TYPE_GT_MAC, kernel_idx);

    LogMessage(LOG_DESIGNER, "WriteGTMACTrafficCfg - " + kernel_info_name + ": Write data in memory");
    cl_err = cl_cmd_queue.enqueueWriteBuffer(cl_m00_axi_ptr[KRNL_TYPE_GT_MAC][kernel_idx], CL_TRUE, 0, WRITEBUFFER * sizeof(uint32_t), traffic_cfg, &waitEnqueueEvent, NULL);
    CheckClCommandQueueEnqueueWriteBuffer(cl_err, kernel_info_name + "(WriteGTMACTrafficCfg)", &chk_cl_err); CHK_CL_ERR_ABORT_RETURN(chk_cl_err, m_abort);

    LogMessage(LOG_DESIGNER, "WriteGTMACTrafficCfg - " + kernel_info_name + ": Successfully wrote memory");
}

void DeviceInterface::ReadGTMACTrafficCfg(int kernel_idx, uint32_t *read_buffer)
{
    std::vector<cl::Event> waitEnqueueEvent;
    cl_int cl_err = CL_SUCCESS;
    ChkClErr_t chk_cl_err = CHK_CL_ERR_SUCCESS;

    std::string kernel_info_name = GetClKernelNames(KRNL_TYPE_GT_MAC, kernel_idx);

    LogMessage(LOG_DESIGNER, "ReadGTMACTrafficCfg - " + kernel_info_name + ": Read data in memory");
    cl_err = cl_cmd_queue.enqueueReadBuffer(cl_m00_axi_ptr[KRNL_TYPE_GT_MAC][kernel_idx], CL_TRUE, 0, GT_MAC_STATUS_SIZE * sizeof(uint32_t), read_buffer, &waitEnqueueEvent, NULL);
    CheckClCommandQueueEnqueueReadBuffer(cl_err, kernel_info_name + "(ReadGTMACTrafficCfg)", &chk_cl_err); CHK_CL_ERR_ABORT_RETURN(chk_cl_err, m_abort);

    LogMessage(LOG_DESIGNER, "ReadGTMACTrafficCfg - " + kernel_info_name + ": Successfully read memory");
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::string DeviceInterface::GetPwrKernelName   ( int kernel_idx ) { return GetClKernelNames(KRNL_TYPE_PWR,     kernel_idx); }
std::string DeviceInterface::GetMemDDRKernelName( int kernel_idx ) { return GetClKernelNames(KRNL_TYPE_MEM_DDR, kernel_idx); }
std::string DeviceInterface::GetMemHBMKernelName( int kernel_idx ) { return GetClKernelNames(KRNL_TYPE_MEM_HBM, kernel_idx); }
std::string DeviceInterface::GetGTKernelName    ( int kernel_idx ) { return GetClKernelNames(KRNL_TYPE_GT,      kernel_idx); }
std::string DeviceInterface::GetGTMACKernelName ( int kernel_idx ) { return GetClKernelNames(KRNL_TYPE_GT_MAC,  kernel_idx); }

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint DeviceInterface::GetKrnlMemKrnlCoreOffset( int kernel_core_idx )
{
    uint krnl_core_offset = 0x0800;                              //b11 0 => Wrapper build info (000) / 1 => MEMTEST_TOP build info (800)
    krnl_core_offset |= (((uint)kernel_core_idx & 0x1F) << 6);    // MEMTEST_TOP instance b10:b6 [00,1F] ([000,7C0])
    return krnl_core_offset;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DeviceInterface::Build_Info DeviceInterface::GetKrnlBI( int kernel_type, int kernel_idx, int kernel_core_idx )
{
    int read_data;
    Build_Info krnl_bi;
    uint krnl_core_offset = 0x0000;

    if (kernel_core_idx != KERNEL_CORE_IDX_UNUSED) // Do not use kernel core offset
        krnl_core_offset = GetKrnlMemKrnlCoreOffset(kernel_core_idx);

    krnl_bi.kernel_name = GetClKernelNames(kernel_type, kernel_idx);

    read_data = ReadKernel(kernel_type, kernel_idx, krnl_core_offset | BI_MAJOR_MINOR_VERSION_ADDR);
    krnl_bi.major_version       = (read_data >> 16) & 0x0000FFFF;
    krnl_bi.minor_version       = (read_data >> 0)  & 0x0000FFFF;

    read_data = ReadKernel(kernel_type, kernel_idx, krnl_core_offset | BI_PERFORCE_VERSION_ADDR);
    krnl_bi.perforce_version    = (read_data >> 0)  & 0xFFFFFFFF;

    read_data = ReadKernel(kernel_type, kernel_idx, krnl_core_offset | BI_COMPONENT_ID_ADDR);
    krnl_bi.component_id        = (read_data >> 0)  & 0xFFFFFFFF;

    read_data = ReadKernel(kernel_type, kernel_idx, krnl_core_offset | CMN_SCRATCH_PAD_ADDR);
    krnl_bi.scratch_pad         = (read_data >> 0)  & 0xFFFFFFFF;

    read_data = ReadKernel(kernel_type, kernel_idx, krnl_core_offset | BI_INFO_1_2_ADDR);
     // (read_data >> 0)  & 0x0000FFFF;   // Info 1 reserved for future use
    if (kernel_type == KRNL_TYPE_PWR)
        krnl_bi.slr             = (read_data >> 20) & 0x0000000F;   // Info 2

    if ((kernel_type == KRNL_TYPE_MEM_DDR) || (kernel_type == KRNL_TYPE_MEM_HBM))
        krnl_bi.mem_kernel_inst = (read_data >> 24)  & 0x000000FF;  // Info 2
    else if (kernel_type == KRNL_TYPE_GT)
        krnl_bi.gt_index        = (read_data >> 24)  & 0x000000FF;  // Info 2
    else if (kernel_type == KRNL_TYPE_GT_MAC)
        krnl_bi.gt_index        = (read_data >> 24)  & 0x000000FF;  // Info 2

    read_data = ReadKernel(kernel_type, kernel_idx, krnl_core_offset | BI_INFO_3_4_ADDR);
    if (kernel_type == KRNL_TYPE_PWR)
    {
        krnl_bi.num_reg     = (read_data >> 0)  & 0x0000FFFF;   // Info 3
        krnl_bi.num_dsp48e2 = (read_data >> 16) & 0x0000FFFF;   // Info 4
    }
    else if ((kernel_type == KRNL_TYPE_MEM_DDR) || (kernel_type == KRNL_TYPE_MEM_HBM))
    {
        krnl_bi.mem_kernel_num_core     = (read_data >> 16) & 0x0000FFFF;   // Info 4
    }

    read_data = ReadKernel(kernel_type, kernel_idx, krnl_core_offset | BI_INFO_5_6_ADDR);
    if (kernel_type == KRNL_TYPE_PWR)
    {
        krnl_bi.num_ramb36  = (read_data >> 0)  & 0x0000FFFF;   // Info 5
        krnl_bi.num_uram288 = (read_data >> 16) & 0x0000FFFF;   // Info 6
    }
    else if ((kernel_type == KRNL_TYPE_MEM_DDR) || (kernel_type == KRNL_TYPE_MEM_HBM))
    {
        krnl_bi.mem_kernel_num_mem  = (read_data >> 0)  & 0x0000FFFF;   // Info 5
        if (kernel_core_idx != KERNEL_CORE_IDX_UNUSED)
        {
            krnl_bi.mem_kernel_core_idx = (read_data >> 16) & 0x000000FF;   // Info 6
            krnl_bi.mem_kernel_dst_idx  = (read_data >> 24) & 0x0000007F;   // Info 6
            krnl_bi.mem_kernel_dst_type = (read_data >> 31) & 0x00000001;   // Info 6

            m_mem_kernel_dst_idx[kernel_type][kernel_idx][kernel_core_idx]  = krnl_bi.mem_kernel_dst_idx;
            m_mem_kernel_dst_type[kernel_type][kernel_idx][kernel_core_idx] = krnl_bi.mem_kernel_dst_type;
            if (krnl_bi.mem_kernel_dst_type == 0) // DDR type
            {
                m_mem_kernel_tag[kernel_type][kernel_idx][kernel_core_idx] = "DDR[" + std::to_string(krnl_bi.mem_kernel_dst_idx) + "]";
            }
            else if (krnl_bi.mem_kernel_dst_type == 1) // HBM Type
            {
                if (krnl_bi.mem_kernel_num_mem == 1)
                    m_mem_kernel_tag[kernel_type][kernel_idx][kernel_core_idx] = "HBM[" + std::to_string(krnl_bi.mem_kernel_dst_idx) + "]";
                else if (krnl_bi.mem_kernel_num_mem > 1)
                    m_mem_kernel_tag[kernel_type][kernel_idx][kernel_core_idx] = "HBM[" + std::to_string(krnl_bi.mem_kernel_dst_idx) + ":" + std::to_string(krnl_bi.mem_kernel_dst_idx + krnl_bi.mem_kernel_num_mem - 1) + "]";
            }
        }
    }

    read_data = ReadKernel(kernel_type, kernel_idx, krnl_core_offset | CMN_RESET_DETECTION_ADDR);
    krnl_bi.rst_detection   = (read_data >> 0)  & 0x00000003;

    return krnl_bi;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DeviceInterface::PrintKrnlBI( DeviceInterface::Build_Info krnl_bi, int kernel_core_idx )
{
    if (kernel_core_idx != KERNEL_CORE_IDX_UNUSED)
        LogMessage(LOG_DESIGNER, "Compute Unit build info (Core " + std::to_string(kernel_core_idx) + "):");
    else
        LogMessage(LOG_DESIGNER, "Compute Unit build info:");
    LogMessage(LOG_DESIGNER, "\t\t\t - CU Name                 : "     + krnl_bi.kernel_name);
    LogMessage(LOG_DESIGNER, "\t\t\t - Major Version           : "     + std::to_string(krnl_bi.major_version));
    LogMessage(LOG_DESIGNER, "\t\t\t - Minor Version           : "     + std::to_string(krnl_bi.minor_version));
    LogMessage(LOG_DESIGNER, "\t\t\t - HW Build                : "     + std::to_string(krnl_bi.perforce_version));
    LogMessage(LOG_DESIGNER, "\t\t\t - Component ID            : "     + std::to_string(krnl_bi.component_id));
    LogMessage(LOG_DESIGNER, "\t\t\t - Scratch pad             : 0x"   + NumToStrHex<uint>(krnl_bi.scratch_pad));
    LogMessage(LOG_DESIGNER, "\t\t\t - Reset Detection         : "     + std::to_string(krnl_bi.rst_detection));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DeviceInterface::PrintPwrKrnlBI( DeviceInterface::Build_Info krnl_bi )
{
    PrintKrnlBI(krnl_bi, KERNEL_CORE_IDX_UNUSED);
    LogMessage(LOG_DESIGNER, "\t\t\t - SLR                     : " + std::to_string(krnl_bi.slr));
    LogMessage(LOG_DESIGNER, "\t\t\t - Slices number           : " + std::to_string(krnl_bi.num_reg));
    LogMessage(LOG_DESIGNER, "\t\t\t - DSP48E2 number          : " + std::to_string(krnl_bi.num_dsp48e2));
    LogMessage(LOG_DESIGNER, "\t\t\t - RAMB36 number           : " + std::to_string(krnl_bi.num_ramb36));
    LogMessage(LOG_DESIGNER, "\t\t\t - URAM288 number          : " + std::to_string(krnl_bi.num_uram288));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DeviceInterface::PrintMemDDRKrnlBI( DeviceInterface::Build_Info krnl_bi, int kernel_core_idx )
{
    PrintKrnlBI(krnl_bi, kernel_core_idx);
    LogMessage(LOG_DESIGNER, "\t\t\t - Number of DDR cores     : " + std::to_string(krnl_bi.mem_kernel_num_core));
    LogMessage(LOG_DESIGNER, "\t\t\t - Number of DDR channels  : " + std::to_string(krnl_bi.mem_kernel_num_mem));
    LogMessage(LOG_DESIGNER, "\t\t\t - DDR CU instance         : " + std::to_string(krnl_bi.mem_kernel_inst));
    if (kernel_core_idx != KERNEL_CORE_IDX_UNUSED)
    {
        LogMessage(LOG_DESIGNER, "\t\t\t - DDR CU channel index    : " + std::to_string(krnl_bi.mem_kernel_core_idx));
        LogMessage(LOG_DESIGNER, "\t\t\t - DDR channel dest type   : " + std::to_string(krnl_bi.mem_kernel_dst_type));
        LogMessage(LOG_DESIGNER, "\t\t\t - DDR channel dest index  : " + std::to_string(krnl_bi.mem_kernel_dst_idx));
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DeviceInterface::PrintMemHBMKrnlBI( DeviceInterface::Build_Info krnl_bi, int kernel_core_idx )
{
    PrintKrnlBI(krnl_bi, kernel_core_idx);
    LogMessage(LOG_DESIGNER, "\t\t\t - Number of HBM cores     : " + std::to_string(krnl_bi.mem_kernel_num_core));
    LogMessage(LOG_DESIGNER, "\t\t\t - Number of HBM channels  : " + std::to_string(krnl_bi.mem_kernel_num_mem));
    LogMessage(LOG_DESIGNER, "\t\t\t - HBM CU instance         : " + std::to_string(krnl_bi.mem_kernel_inst));
    if (kernel_core_idx != KERNEL_CORE_IDX_UNUSED)
    {
        LogMessage(LOG_DESIGNER, "\t\t\t - HBM CU channel index    : " + std::to_string(krnl_bi.mem_kernel_core_idx));
        LogMessage(LOG_DESIGNER, "\t\t\t - HBM channel dest type   : " + std::to_string(krnl_bi.mem_kernel_dst_type));
        LogMessage(LOG_DESIGNER, "\t\t\t - HBM channel dest index  : " + std::to_string(krnl_bi.mem_kernel_dst_idx));
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DeviceInterface::PrintGTKrnlBI( DeviceInterface::Build_Info krnl_bi )
{
    PrintKrnlBI(krnl_bi, KERNEL_CORE_IDX_UNUSED);
    LogMessage(LOG_DESIGNER, "\t\t\t - GT index                : " + std::to_string(krnl_bi.gt_index));
}

void DeviceInterface::PrintGTMACKrnlBI( DeviceInterface::Build_Info krnl_bi )
{
    PrintKrnlBI(krnl_bi, KERNEL_CORE_IDX_UNUSED);
    LogMessage(LOG_DESIGNER, "\t\t\t - GT MAC index            : " + std::to_string(krnl_bi.gt_index));
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DeviceInterface::PrintAllMemTopology( MemTopology_t mem_topology )
{
    LogMessage(LOG_DESIGNER, "Memory Topology count: " + std::to_string(mem_topology.mem_count));
    for (uint i = 0; i < mem_topology.mem_count; i++ )
    {
        PrintMemData(i, mem_topology.mem_data[i]);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DeviceInterface::PrintUsedMemTopology( MemTopology_t mem_topology )
{
    LogMessage(LOG_DESIGNER, "Memory Topology count: " + std::to_string(mem_topology.mem_count));
    for (uint i = 0; i < mem_topology.mem_count; i++ )
    {
        if (mem_topology.mem_data[i].enabled == true)
        {
            PrintMemData(i, mem_topology.mem_data[i]);
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DeviceInterface::PrintMemData( uint mem_topology_idx, MemData_t mem_data )
{
    LogMessage(LOG_DESIGNER, "Memory Topology index " + std::to_string(mem_topology_idx) + ":");
    LogMessage(LOG_DESIGNER, "\t - Type:          " + mem_data.type);
    LogMessage(LOG_DESIGNER, "\t - Temperature:   " + std::to_string(mem_data.temp));
    LogMessage(LOG_DESIGNER, "\t - Tag:           " + mem_data.tag);
    LogMessage(LOG_DESIGNER, "\t - Enabled:       " + BoolToStr(mem_data.enabled));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint DeviceInterface::GetNumMemTopologyType( MemTopology_t mem_topology, std::string mem_type )
{
    uint mem_count = 0;
    for (uint i = 0; i < mem_topology.mem_count; i++ )
    {
        if (IsTagOfType(mem_topology.mem_data[i].tag, mem_type) == true)
            mem_count++;
    }
    LogMessage(LOG_INFO, "Memory Topology " + mem_type + " count: " + std::to_string(mem_count));
    return mem_count;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MemTopology_t DeviceInterface::GetMemoryTopology() { return m_mem_topology; }

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DeviceInterface::GetXclbinDumpInfo()
{
    bool ret_failure = false;
    std::vector<std::string> node_title;

    m_xbutil_dump_parser->ClearParser();
    ret_failure |= m_xbutil_dump_parser->Parse();
    if (ret_failure == true)
    {
        LogMessage(LOG_FAILURE, "GetXclbinDumpInfo: Failed to execute xbutil dump!");
        return true;
    }

    node_title = {"board", "xclbin", "uuid"};
    ret_failure |= m_xbutil_dump_parser->ExtractNodeValueStr(node_title, &m_xclbin_uuid);
    if (ret_failure == true) return true;

    // Get memory topology
    // init
    m_mem_topology.mem_count = 0;
    m_mem_topology.mem_data.clear();

    // Check if first index exists
    node_title = {"board", "memory", "mem", std::to_string(m_mem_topology.mem_count)};
    if (m_xbutil_dump_parser->NodeExists(node_title) == false)
    {
        LogMessage(LOG_FAILURE, "GetXclbinDumpInfo: No memory found in Memory Topology!");
        return true;
    }

    // Get memory for each existing index
    bool mem_exists = true;
    while (mem_exists == true)
    {
        node_title = {"board", "memory", "mem", std::to_string(m_mem_topology.mem_count)};
        if (m_xbutil_dump_parser->NodeExists(node_title) == false)
        {
            mem_exists = false;
        }
        else
        {
            // Parse mem_data
            MemData_t mem_data;
            std::string tmp_str;

            // Get type
            node_title = {"board", "memory", "mem", std::to_string(m_mem_topology.mem_count), "type"};
            ret_failure |= m_xbutil_dump_parser->ExtractNodeValueStr(node_title, &(mem_data.type));
            if (ret_failure == true) return true;

            // Get temp
            node_title = {"board", "memory", "mem", std::to_string(m_mem_topology.mem_count), "temp"};
            ret_failure |= m_xbutil_dump_parser->ExtractNodeValueStr(node_title, &(tmp_str));
            if (ret_failure == true) return true;
            ret_failure |= ConvString2Num<unsigned>(tmp_str, &(mem_data.temp));
            if (ret_failure == true)
            {
                LogMessage(LOG_FAILURE, "GetXclbinDumpInfo: Failed to convert xbutil dump value: " + StrVectToStr(node_title, "."));
                return true;
            }

            // Get tag
            node_title = {"board", "memory", "mem", std::to_string(m_mem_topology.mem_count), "tag"};
            ret_failure |= m_xbutil_dump_parser->ExtractNodeValueStr(node_title, &(mem_data.tag));
            if (ret_failure == true) return true;
            ForceNotUseBankTag(&(mem_data.tag));

            // Get enabled
            node_title = {"board", "memory", "mem", std::to_string(m_mem_topology.mem_count), "enabled"};
            ret_failure |= m_xbutil_dump_parser->ExtractNodeValueStr(node_title, &(tmp_str));
            if (ret_failure == true) return true;
            mem_data.enabled = StrToBool(tmp_str);

            m_mem_topology.mem_data.push_back(mem_data);
            m_mem_topology.mem_count++;
        }
    }

    // Get compute unit index (ip layout index) / name map
    // init
    m_cu_index_name_map.count = 0;
    m_cu_index_name_map.name.clear();

    // Check if first index exists
    node_title = {"board", "compute_unit", std::to_string(m_cu_index_name_map.count)};
    if (m_xbutil_dump_parser->NodeExists(node_title) == false)
    {
        LogMessage(LOG_FAILURE, "GetXclbinDumpInfo: No compute unit found in xclbin");
        return true;
    }
    bool cu_exists = true;
    while (cu_exists == true)
    {
        node_title = {"board", "compute_unit", std::to_string(m_cu_index_name_map.count)};
        if (m_xbutil_dump_parser->NodeExists(node_title) == false)
        {
            cu_exists = false;
        }
        else
        {
            // Parse compute_unit
            std::string name;

            // Get name
            node_title = {"board", "compute_unit", std::to_string(m_cu_index_name_map.count), "name"};
            ret_failure |= m_xbutil_dump_parser->ExtractNodeValueStr(node_title, &(name));
            if (ret_failure == true) return true;
            m_cu_index_name_map.name.push_back(name);
            m_cu_index_name_map.count++;
        }
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DeviceInterface::GetDeviceInfo( Device_Info *pInfo )
{
    bool ret_failure = false;
    std::vector<std::string> node_title;
    Device_Info aInfo;

    // Reset sensor value before parsing
    aInfo.mFanRpm   = 0;
    for (uint i=0; i<MAX_TEMP_SOURCES; i++)
    {
        aInfo.temperature[i] = 0;
    }
    for (uint i=0; i<MAX_POWER_SOURCES; i++)
    {
        aInfo.current[i] = 0;
        aInfo.voltage[i] = 0;
        aInfo.power[i] = 0;
    }

    // auto begin = std::chrono::steady_clock::now();
    m_xbutil_dump_parser->ClearParser();
    ret_failure |= m_xbutil_dump_parser->Parse();
    // auto end = std::chrono::steady_clock::now();
    // auto ticks = std::chrono::duration_cast<std::chrono::microseconds>(end-begin);
    // LogMessage(LOG_DEBUG, "m_xbutil_dump_parser->Parse(): " + std::to_string(ticks.count()) + "us");

    if (ret_failure == true)
    {
        LogMessage(LOG_FAILURE, "GetDeviceInfo: Failed to execute xbutil dump!");
        return true;
    }

    std::string tmp_str;

    // Get fan_speed
    node_title = {"board","physical","thermal","fan_speed"};
    ret_failure |= m_xbutil_dump_parser->ExtractNodeValueStr(node_title, &tmp_str);
    if (ret_failure == true) return true;
    ret_failure |= ConvString2Num<unsigned>(tmp_str, &(aInfo.mFanRpm));
    if (ret_failure == true)
    {
        LogMessage(LOG_FAILURE, "GetDeviceInfo: Failed to convert xbutil dump value: " + StrVectToStr(node_title, "."));
        return true;
    }

    for (uint i=0; i<m_xbtest_pfm_def.physical.thermal.num_temp_sources; i++)
    {
        // Get temperature
        node_title = m_xbtest_pfm_def.physical.thermal.temp_sources[i].name;
        ret_failure |= m_xbutil_dump_parser->ExtractNodeValueStr(node_title, &tmp_str);
        if (ret_failure == true) return true;
        ret_failure |= ConvString2Num<unsigned>(tmp_str, &(aInfo.temperature[i]));
        if (ret_failure == true)
        {
            LogMessage(LOG_FAILURE, "GetDeviceInfo: Failed to convert xbutil dump value: " + StrVectToStr(node_title, "."));
            return true;
        }
    }

    aInfo.Power_uW = 0.0;
    aInfo.Power_Calib_mW = 0.0;
    for (uint i=0; i<m_xbtest_pfm_def.physical.power.num_power_sources; i++)
    {
        if (m_xbtest_pfm_def.physical.power.power_sources[i].def_by_curr_volt == true)
        {
            // Get current
            node_title = m_xbtest_pfm_def.physical.power.power_sources[i].name_current;
            ret_failure |= m_xbutil_dump_parser->ExtractNodeValueStr(node_title, &tmp_str);
            if (ret_failure == true) return true;
            ret_failure |= ConvString2Num<unsigned>(tmp_str, &(aInfo.current[i]));
            if (ret_failure == true)
            {
                LogMessage(LOG_FAILURE, "GetDeviceInfo: Failed to convert xbutil dump value: " + StrVectToStr(node_title, "."));
                return true;
            }

            // Get voltage
            node_title = m_xbtest_pfm_def.physical.power.power_sources[i].name_voltage;
            ret_failure |= m_xbutil_dump_parser->ExtractNodeValueStr(m_xbtest_pfm_def.physical.power.power_sources[i].name_voltage, &tmp_str);
            if (ret_failure == true) return true;
            ret_failure |= ConvString2Num<unsigned>(tmp_str, &(aInfo.voltage[i]));
            if (ret_failure == true)
            {
                LogMessage(LOG_FAILURE, "GetDeviceInfo: Failed to convert xbutil dump value: " + StrVectToStr(node_title, "."));
                return true;
            }

            // Compute power
            aInfo.power[i] = (double)(aInfo.voltage[i] * aInfo.current[i]) / 1000.0 / 1000.0;
            if (m_xbtest_pfm_def.physical.power.power_sources[i].powertest == true)
                aInfo.Power_uW += (double)(aInfo.voltage[i] * aInfo.current[i]);
            if (m_xbtest_pfm_def.physical.power.power_sources[i].calibration > 0)
                aInfo.Power_Calib_mW += (double)(aInfo.voltage[i] * aInfo.current[i])/1000.0;
        }
        else
        {
            // Get power
            node_title = m_xbtest_pfm_def.physical.power.power_sources[i].name;
            unsigned power_int = 0;
            ret_failure |= m_xbutil_dump_parser->ExtractNodeValueStr(m_xbtest_pfm_def.physical.power.power_sources[i].name, &tmp_str);
            if (ret_failure == true) return true;
            ret_failure |= ConvString2Num<unsigned>(tmp_str, &power_int);
            if (ret_failure == true)
            {
                LogMessage(LOG_FAILURE, "GetDeviceInfo: Failed to convert xbutil dump value: " + StrVectToStr(node_title, "."));
                return true;
            }

            aInfo.power[i] = (double)power_int;
            if (m_xbtest_pfm_def.physical.power.power_sources[i].powertest == true)
                aInfo.Power_uW += aInfo.power[i] * 1000.0 * 1000.0;
            if (m_xbtest_pfm_def.physical.power.power_sources[i].calibration == true)
                aInfo.Power_Calib_mW += aInfo.power[i] * 1000.0;
        }
    }
    aInfo.Power_mW = aInfo.Power_uW / 1000.0;
    aInfo.Power_W = (unsigned)(aInfo.Power_uW / 1000.0 / 1000.0);


    *pInfo = aInfo; // Copy results

    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

cl::CommandQueue * DeviceInterface::GetCmdQueueInstance()   { return &cl_cmd_queue; }
cl::Context * DeviceInterface::GetContextInstance()         { return &cl_context; }

void DeviceInterface::LockDevice()      { m_device_mtx.lock(); }
void DeviceInterface::UnlockDevice()    { m_device_mtx.unlock(); }


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DeviceInterface::PrintCuIndexNameMap()
{
    LogMessage(LOG_DEBUG, "Xclbin Compute unit names:");
    LogMessage(LOG_DEBUG, "\t - count: " + std::to_string(m_cu_index_name_map.count));
    for (uint j = 0; j < m_cu_index_name_map.count; j++) // For each element in connection array
    {
        LogMessage(LOG_DEBUG, "\t - name[" + std::to_string(j) + "]: " + m_cu_index_name_map.name[j]);
    }
}
