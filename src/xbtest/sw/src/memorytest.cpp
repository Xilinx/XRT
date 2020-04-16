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

#include "memorytest.h"
#include <queue>
extern std::queue<int> IPC_queue;


uint MemoryTest::ReadMemKernel( int kernel_core_idx, uint address )
{
    uint read_data = 0;
    if (m_kernel_type == TEST_MEMORY_DDR)
        read_data = m_device->ReadMemDDRKernel(m_kernel_idx, kernel_core_idx, address);
    else if (m_kernel_type == TEST_MEMORY_HBM)
        read_data = m_device->ReadMemHBMKernel(m_kernel_idx, kernel_core_idx, address);
    return read_data;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MemoryTest::WriteMemKernel( int kernel_core_idx, uint address, uint value )
{
    if (m_kernel_type == TEST_MEMORY_DDR)
        m_device->WriteMemDDRKernel(m_kernel_idx, kernel_core_idx, address, value);
    else if (m_kernel_type == TEST_MEMORY_HBM)
        m_device->WriteMemHBMKernel(m_kernel_idx, kernel_core_idx, address, value);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::string MemoryTest::GetMemKernelName()
{
    std::string kernel_name;
    if (m_kernel_type == TEST_MEMORY_DDR)
        kernel_name = m_device->GetMemDDRKernelName(m_kernel_idx);
    else if (m_kernel_type == TEST_MEMORY_HBM)
        kernel_name = m_device->GetMemHBMKernelName(m_kernel_idx);
    return kernel_name;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int MemoryTest::GetMemKernelNumCore()
{
    int kernel_num_core = 0;
    if (m_kernel_type == TEST_MEMORY_DDR)
        kernel_num_core = m_device->GetMemKernelDDRNumCore(m_kernel_idx);
    else if (m_kernel_type == TEST_MEMORY_HBM)
        kernel_num_core = m_device->GetMemKernelHBMNumCore(m_kernel_idx);
    return kernel_num_core;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int MemoryTest::GetMemKernelNumMem()
{
    int kernel_num_mem = 0;
    if (m_kernel_type == TEST_MEMORY_DDR)
        kernel_num_mem = m_device->GetMemKernelDDRNumMem(m_kernel_idx);
    else if (m_kernel_type == TEST_MEMORY_HBM)
        kernel_num_mem = m_device->GetMemKernelHBMNumMem(m_kernel_idx);
    return kernel_num_mem;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::string MemoryTest::GetMemKernelTag( int kernel_core_idx )
{
    std::string tag;
    if (m_kernel_type == TEST_MEMORY_DDR)
        tag = m_device->GetMemKernelDDRTag(m_kernel_idx, kernel_core_idx);
    else if (m_kernel_type == TEST_MEMORY_HBM)
        tag = m_device->GetMemKernelHBMTag(m_kernel_idx, kernel_core_idx);
    return tag;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::string MemoryTest::GetMemKernelTag2( int kernel_core_idx )
{
    std::string tag;
    if (m_kernel_type == TEST_MEMORY_DDR)
    {
        tag = "ddr_";
        tag += std::to_string(m_device->GetMemKernelDDRDstIdx(m_kernel_idx, kernel_core_idx));
    }
    else if (m_kernel_type == TEST_MEMORY_HBM)
    {
        tag = "hbm_";
        tag += std::to_string(m_device->GetMemKernelHBMDstIdx(m_kernel_idx, kernel_core_idx));
        tag += "_";
        tag += std::to_string(m_device->GetMemKernelHBMDstIdx(m_kernel_idx, kernel_core_idx) + m_device->GetMemKernelHBMNumMem(m_kernel_idx) - 1);
    }
    return tag;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int MemoryTest::GetMemKernelInst()
{
    int kernel_inst = 0;
    if (m_kernel_type == TEST_MEMORY_DDR)
        kernel_inst = m_device->GetMemKernelDDRInst(m_kernel_idx);
    else if (m_kernel_type == TEST_MEMORY_HBM)
        kernel_inst = m_device->GetMemKernelHBMInst(m_kernel_idx);
    return kernel_inst;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::string MemoryTest::GetMemTypeStr()
{
    std::string mem_type;
    if (m_kernel_type == TEST_MEMORY_DDR)
        mem_type = "DDR";
    else if (m_kernel_type == TEST_MEMORY_HBM)
        mem_type = "HBM";
    return mem_type;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MemoryTest::PrintRegHex( int kernel_core_idx, uint reg_addr, std::string reg_name )
{
    uint read_data = ReadMemKernel(kernel_core_idx, reg_addr);
    LogMessage(LOG_DEBUG,  "\t " + reg_name + " = 0x" + NumToStrHex<uint>(read_data));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MemoryTest::PrintConfig( int kernel_core_idx )
{
    LogMessage(LOG_DEBUG,  "Configuration channel " + std::to_string(kernel_core_idx) + ":");
    PrintRegHex( kernel_core_idx,  MEM_CTRL_ADDR,                         "MEM_CTRL_ADDR"                       );
    PrintRegHex( kernel_core_idx,  MEM_CTRL_WR_CTRL_ADDR_0,               "MEM_CTRL_WR_CTRL_ADDR_0"             );
    PrintRegHex( kernel_core_idx,  MEM_CTRL_WR_CTRL_ADDR_1,               "MEM_CTRL_WR_CTRL_ADDR_1"             );
    PrintRegHex( kernel_core_idx,  MEM_CTRL_RD_CTRL_ADDR_0,               "MEM_CTRL_RD_CTRL_ADDR_0"             );
    PrintRegHex( kernel_core_idx,  MEM_CTRL_RD_CTRL_ADDR_1,               "MEM_CTRL_RD_CTRL_ADDR_1"             );
    PrintRegHex( kernel_core_idx,  MEM_CTRL_WR_CTRL_XFER_BYTES_ADDR,      "MEM_CTRL_WR_CTRL_XFER_BYTES_ADDR"    );
    PrintRegHex( kernel_core_idx,  MEM_CTRL_RD_CTRL_XFER_BYTES_ADDR,      "MEM_CTRL_RD_CTRL_XFER_BYTES_ADDR"    );
    PrintRegHex( kernel_core_idx,  MEM_CTRL_WR_CTRL_NUM_XFER_ADDR,        "MEM_CTRL_WR_CTRL_NUM_XFER_ADDR"      );
    PrintRegHex( kernel_core_idx,  MEM_CTRL_RD_CTRL_NUM_XFER_ADDR,        "MEM_CTRL_RD_CTRL_NUM_XFER_ADDR"      );
    PrintRegHex( kernel_core_idx,  MEM_STAT_WR_TRANSFER_CNT_ADDR,         "MEM_STAT_WR_TRANSFER_CNT_ADDR"       );
    PrintRegHex( kernel_core_idx,  MEM_STAT_RD_TRANSFER_CNT_ADDR,         "MEM_STAT_RD_TRANSFER_CNT_ADDR"       );
    PrintRegHex( kernel_core_idx,  MEM_STAT_TERM_ERROR_COUNT_ADDR,        "MEM_STAT_TERM_ERROR_COUNT_ADDR"      );
    PrintRegHex( kernel_core_idx,  MEM_STAT_AXI_ADDR_PTR_ADDR_0,          "MEM_STAT_AXI_ADDR_PTR_ADDR_0"        );
    PrintRegHex( kernel_core_idx,  MEM_STAT_AXI_ADDR_PTR_ADDR_1,          "MEM_STAT_AXI_ADDR_PTR_ADDR_1"        );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MemoryTest::PrintConfigCores()
{
    for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
        PrintConfig(kernel_core_idx);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MemoryTest::InsertError( int kernel_core_idx )
{
    // insert random quantity of error [1,10]
    m_err_qty[kernel_core_idx] = rand() % 10 + 1;

    uint read_data = ReadMemKernel(kernel_core_idx, MEM_CTRL_ADDR);
    read_data |= MEM_CTRL_INSERT_ERR;
    for(uint ii=0; ii<m_err_qty[kernel_core_idx]; ii++)
    {
        WriteMemKernel(kernel_core_idx, MEM_CTRL_ADDR, read_data);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MemoryTest::InsertErrorCores()
{
    for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
        InsertError(kernel_core_idx);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MemoryTest::ClearError( int kernel_core_idx )
{
    uint read_data = ReadMemKernel(kernel_core_idx, MEM_CTRL_ADDR);
    read_data |= MEM_CTRL_CLEAR_ERR;
    WriteMemKernel(kernel_core_idx, MEM_CTRL_ADDR, read_data);

//    std::string msg = "Clear error";
//    if (m_kernel_type == TEST_MEMORY_HBM)
//        msg += " for channelchannel: " + std::to_string(kernel_core_idx) + " (" + GetMemKernelTag(kernel_core_idx) + ")";
//    LogMessage(LOG_WARN, msg);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MemoryTest::ClearErrorCores()
{
    for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
        ClearError(kernel_core_idx);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint MemoryTest::GetErrCnt( int kernel_core_idx )
{
    uint read_data = ReadMemKernel(kernel_core_idx, MEM_STAT_TERM_ERROR_COUNT_ADDR);
    return read_data;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void MemoryTest::SetTestMode( uint value )
{
    uint read_data;
    for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
    {
        read_data = ReadMemKernel(kernel_core_idx, MEM_CTRL_ADDR);
        read_data = (((value <<  4) & MEM_TEST_MODE_MASK) | (read_data & ~MEM_TEST_MODE_MASK));
        WriteMemKernel(kernel_core_idx, MEM_CTRL_ADDR, read_data);
    }
}

void MemoryTest::ResetWatchdog()
{
    uint read_data;
    for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
    {
        // if a reset is requested, it also means that the watchdog is enabled
        //  don't read the current value of the CMN_WATCHDOG_ADDR to save access
        read_data = CMN_WATCHDOG_RST | CMN_WATCHDOG_EN;
        WriteMemKernel(kernel_core_idx, CMN_WATCHDOG_ADDR,read_data);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool MemoryTest::StartTestAndEnableWatchdog()
{
    uint read_data;
    bool krnl_already_started = false;

    // start the kernel and check if the "already started" is received back
    // if it received back, check if the watchdog trigered before, meaning that previous test was abruptly stopped (kill process or terminal closed)
    //      if watchdog is enabled and alarm is present, we can safely
    //          1) clear watchdog
    //          2) start the test
    //      if no alarm or not enable, the previous run of xbtest was left in an unknown state so ask for user to "xbutil validate" it (aka remove xbtest xclbin)
    //
    // the watchdog doesn't clear the start bit
    // the watchdog is always disabled at teh of the test

    for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx && (krnl_already_started == false); kernel_core_idx++)
    {
        WriteMemKernel(kernel_core_idx, CMN_CTRL_STATUS_ADDR, CMN_STATUS_START);
        read_data = ReadMemKernel(kernel_core_idx, CMN_CTRL_STATUS_ADDR);
        if ((read_data & CMN_STATUS_ALREADY_START) == CMN_STATUS_ALREADY_START)
        {
            read_data = ReadMemKernel(kernel_core_idx, CMN_WATCHDOG_ADDR);
            // check if watchdog is already enable and error is detected
            if ( ((read_data & CMN_WATCHDOG_EN) == CMN_WATCHDOG_EN) && ((read_data & CMN_WATCHDOG_ALARM) == CMN_WATCHDOG_ALARM) )
            {
                std::string msg = "Watchdog has been triggered during previous test (memory CU";
                if ((read_data & CMN_WATCHDOG_ALARM) == CMN_WATCHDOG_ALARM)
                {
                    if (m_kernel_type == TEST_MEMORY_HBM)
                        msg += " for channel: " + std::to_string(kernel_core_idx) + " (" + GetMemKernelTag(kernel_core_idx) + ")";
                    msg += ") but start this test";
                    LogMessage(LOG_CRIT_WARN, msg);
                }

                // it's safe to restart the kernel, but first clear the start bit and the watchdog
                // stop the kernel
                WriteMemKernel(kernel_core_idx, CMN_CTRL_STATUS_ADDR, 0x0); // this also clears the alreay_start bit
                // stop watchdog and clear error
                WriteMemKernel(kernel_core_idx, CMN_WATCHDOG_ADDR, CMN_WATCHDOG_ALARM);
                // start the test
                WriteMemKernel(kernel_core_idx, CMN_CTRL_STATUS_ADDR, CMN_STATUS_START);
            }
            else
            {
                std::string msg = "Test already running on memory CU";
                if (m_kernel_type == TEST_MEMORY_HBM)
                    msg += " for channel: " + std::to_string(kernel_core_idx) + " (" + GetMemKernelTag(kernel_core_idx) + ")";
                msg += ". By trying to start another test, this may cause error(s) in currently running test. If no tests are running, you card is maybe in unkwown state, first re-validate it, then try xbtest again";
                LogMessage(LOG_ERROR, msg);

                krnl_already_started = true;
            }
        }
    }

    for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
    {
        read_data = ReadMemKernel(kernel_core_idx, CMN_WATCHDOG_ADDR);
        std::string msg = "Watchdog has been triggered during previous test (memory CU";
        if ((read_data & CMN_WATCHDOG_ALARM) == CMN_WATCHDOG_ALARM)
        {
            if (m_kernel_type == TEST_MEMORY_HBM)
                msg += " for channel: " + std::to_string(kernel_core_idx) + " (" + GetMemKernelTag(kernel_core_idx) + ")";
            msg += ").";
            LogMessage(LOG_WARN, msg);
        }
    }

    // enable the watchdog if the kernel was't started
    if (krnl_already_started == false)
    {
        for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
        {
            // start and clear any previous alarm
            read_data = CMN_WATCHDOG_EN | CMN_WATCHDOG_ALARM;
            WriteMemKernel(kernel_core_idx, CMN_WATCHDOG_ADDR, read_data);
        }
    }

    return krnl_already_started;
}

bool MemoryTest::StopTestAndDisableWatchdog()
{
    uint read_data;
    bool error = false;
    // stop the kernel and check if the "already started" is present,
    // meanign that another test tried to start teh kernl too

    for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
    {
        read_data = ReadMemKernel(kernel_core_idx, CMN_CTRL_STATUS_ADDR);
        if ((read_data & CMN_STATUS_ALREADY_START) == CMN_STATUS_ALREADY_START)
        {
            std::string msg = "Another test tried to access the memory CU";
            if (m_kernel_type == TEST_MEMORY_HBM)
                msg += " for channel: " + std::to_string(kernel_core_idx) + " (" + GetMemKernelTag(kernel_core_idx) + ")";
            msg += ". This may have caused error to this test";
            LogMessage(LOG_ERROR,msg);

            error = true;
        }
        // stop the kernel
        WriteMemKernel(kernel_core_idx, CMN_CTRL_STATUS_ADDR, 0x0);
    }

    // disable the watchdog
    for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
    {
        read_data = ReadMemKernel(kernel_core_idx, CMN_WATCHDOG_ADDR);
        if ((read_data & CMN_WATCHDOG_ALARM) == CMN_WATCHDOG_ALARM)
        {
            std::string msg = "Watchdog alarm detected (memory CU";
            if (m_kernel_type == TEST_MEMORY_HBM)
                msg += " for channel: " + std::to_string(kernel_core_idx) + " (" + GetMemKernelTag(kernel_core_idx) + ")";
            msg += "). This may have caused error to this test";
            LogMessage(LOG_ERROR,msg);

            error = true;
        }
        // disable watchdog and clear any alarm detected
        WriteMemKernel(kernel_core_idx, CMN_WATCHDOG_ADDR, CMN_WATCHDOG_ALARM);
    }

    return error;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MemoryTest::StartKernel()
{
    uint read_data;
    for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
    {
        read_data = ReadMemKernel(kernel_core_idx, MEM_CTRL_ADDR);
        read_data |= MEM_CTRL_START;
        WriteMemKernel(kernel_core_idx, MEM_CTRL_ADDR, read_data);
    }

}

void MemoryTest::StopKernel()
{
    uint read_data;
    std::string msg_txt = "Watchdog triggered";
    for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
    {
        read_data = ReadMemKernel(kernel_core_idx, MEM_CTRL_ADDR);
        read_data &= ~MEM_CTRL_START;
        WriteMemKernel(kernel_core_idx, MEM_CTRL_ADDR, read_data);
    }

}

void MemoryTest::ActivateReset()
{
    uint read_data;
    for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
    {
        read_data = ReadMemKernel(kernel_core_idx, MEM_CTRL_ADDR);
        read_data |= MEM_CTRL_RESET;
        // also stop the kernel
        read_data &= ~MEM_CTRL_START;
        WriteMemKernel(kernel_core_idx, MEM_CTRL_ADDR, read_data);
    }
}


void MemoryTest::ClearReset()
{
    uint read_data;
    for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
    {
        read_data = ReadMemKernel(kernel_core_idx, MEM_CTRL_ADDR);
        read_data &= ~MEM_CTRL_RESET;
        WriteMemKernel(kernel_core_idx, MEM_CTRL_ADDR, read_data);
    }

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void MemoryTest::UpdateCfgKernel( int kernel_core_idx )
{
    uint read_data = ReadMemKernel(kernel_core_idx, MEM_CTRL_ADDR);
    read_data |= MEM_CTRL_UPDATE_CFG;
    WriteMemKernel(kernel_core_idx, MEM_CTRL_ADDR, read_data);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MemoryTest::UpdateCfgKernelCores()
{
    for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
        UpdateCfgKernel(kernel_core_idx);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint MemoryTest::GetStatCfgUpdatedLatch( int kernel_core_idx )
{
    uint read_data = ReadMemKernel(kernel_core_idx, MEM_CTRL_ADDR);
    return (read_data >> 2) & 0x00000001;
}


bool MemoryTest::WaitCfgUpdated( TestItConfig_t test_it )
{
    bool ret_failure = false;
    uint cfg_updated;
    const uint WATCHDOG_TICK = 250000; // 250ms
    //uint64_t watchdog_cnt_init = test_it.cfg_update_time_us/WATCHDOG_TICK; // Ensure configuration is updated.
    uint64_t watchdog_cnt_init = UPDATE_MAX_DURATION * (1000000/WATCHDOG_TICK); // 10 sec max
    LogMessage(LOG_DEBUG, "Check Memory Compute unit configuration updated");
    if (watchdog_cnt_init>0)
        LogMessage(LOG_DEBUG, "Check updated config upto " + std::to_string(watchdog_cnt_init+1) + " times; wait " + std::to_string(WATCHDOG_TICK/1000)+ " ms inbetween each check");

    for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx && (m_abort == false); kernel_core_idx++)
    {
        uint64_t watchdog_cnt = watchdog_cnt_init;
        cfg_updated = GetStatCfgUpdatedLatch(kernel_core_idx);
        while ((watchdog_cnt > 0) && (cfg_updated == 0) && (m_abort == false))
        {
            std::this_thread::sleep_for(std::chrono::microseconds(WATCHDOG_TICK));
            cfg_updated = GetStatCfgUpdatedLatch(kernel_core_idx);
            watchdog_cnt --;
        }
        if ((watchdog_cnt <= 0) && (cfg_updated == 0) && (m_abort == false))
        {
            std::string msg = "Memory Compute unit configuration not updated";
            if (m_kernel_type == TEST_MEMORY_HBM)
                msg += " for channel: " + std::to_string(kernel_core_idx) + " (" + GetMemKernelTag(kernel_core_idx) + ")";
            msg += ", after checking " + std::to_string(watchdog_cnt_init) + " times " + std::to_string(WATCHDOG_TICK/1000) + "ms";
            LogMessage(LOG_ERROR, msg);
            ret_failure = true;
        }
        LogMessage(LOG_DEBUG, "Memory Compute unit configuration updated after checking " + std::to_string(watchdog_cnt_init+1-watchdog_cnt) + " times " + std::to_string(WATCHDOG_TICK/1000) + "ms");
    }

    return ret_failure;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool MemoryTest::GetConfigurationUpdateTime ( MemoryTestcaseCfg_t TC_Cfg, TestItConfig_t *test_it )
{
    // Compute expected time between configuration update command sent and configuration updated flag received depending on num_xfer and BW
    double double_cfg_update_time_us = 0.0;
    test_it->cfg_update_time_us = 0;
    if (test_it->test_mode == CTRL_TEST_MODE_ALTERNATE_WR_RD_TEST_VAL)
    {
        if (test_it->thresh_wr_rd.read.low <= 0.0)
        {
            LogMessage(LOG_FAILURE, "Failed to compute configuration update time as Read BW Low threshold <= 0, Check BW thresholds");
            return true;
        }
        if (test_it->thresh_wr_rd.write.low <= 0.0)
        {
            LogMessage(LOG_FAILURE, "Failed to compute configuration update time as Write BW Low threshold <= 0, Check BW thresholds");
            return true;
        }
        double_cfg_update_time_us  += (double)(test_it->rd_num_xfer)*64.0/(test_it->thresh_wr_rd.read.low*1024.0*1024.0)*1000.0*1000.0;
        double_cfg_update_time_us  += (double)(test_it->wr_num_xfer)*64.0/(test_it->thresh_wr_rd.write.low*1024.0*1024.0)*1000.0*1000.0;
    }
    else if (test_it->test_mode == CTRL_TEST_MODE_ONLY_RD_TEST_VAL)
    {
        if (test_it->thresh_wr_rd.read.low <= 0.0)
        {
            LogMessage(LOG_FAILURE, "Failed to compute configuration update time as Read BW Low threshold <= 0, Check BW thresholds");
            return true;
        }
        double_cfg_update_time_us  += (double)(test_it->rd_num_xfer)*64.0/(test_it->thresh_wr_rd.read.low*1024.0*1024.0)*1000.0*1000.0;
    }
    else if (test_it->test_mode == CTRL_TEST_MODE_ONLY_WR_TEST_VAL)
    {
        if (test_it->thresh_wr_rd.write.low <= 0.0)
        {
            LogMessage(LOG_FAILURE, "Failed to compute configuration update time as Write BW Low threshold <= 0, Check BW thresholds");
            return true;
        }
        double_cfg_update_time_us  += (double)(test_it->wr_num_xfer)*64.0/(test_it->thresh_wr_rd.write.low*1024.0*1024.0)*1000.0*1000.0;
    }
    test_it->cfg_update_time_us = (uint64_t)double_cfg_update_time_us;

    if (test_it->cfg_update_time_us <= 500*1000)
    {
        LogMessage(LOG_DEBUG, "Configuration update time = " + Float_to_String<double>((double)(test_it->cfg_update_time_us)/1000.0/1000.0,3) + "s below minimum, saturate to minimum 500ms");
        test_it->cfg_update_time_us = 500*1000;     // Minimum 500ms
    }

    if (test_it->cfg_update_time_us >= UPDATE_MAX_DURATION*1000*1000)
    {
        LogMessage(LOG_CRIT_WARN, "Configuration update time = " + Float_to_String<double>((double)(test_it->cfg_update_time_us)/1000.0/1000.0,3) + "s above maximum, saturate to maximum of" + std::to_string(UPDATE_MAX_DURATION) + " sec");
        test_it->cfg_update_time_us = UPDATE_MAX_DURATION*1000*1000;
    }

    if (test_it->cfg_update_time_us >= UPDATE_THRESHOLD_DURATION*1000*1000)
        LogMessage(LOG_CRIT_WARN, "Configuration update time " + Float_to_String<double>((double)(test_it->cfg_update_time_us)/1000.0/1000.0,3) + "s bigger than "+ std::to_string (UPDATE_THRESHOLD_DURATION) + " second, Check BW threshold definition");

    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MemoryTest::SetWrCtrlAddr( int kernel_core_idx, uint64_t value )
{
    uint value_tmp;
    value_tmp = value & 0xFFFFFFFF;
    WriteMemKernel(kernel_core_idx, MEM_CTRL_RD_CTRL_ADDR_0, value_tmp);
    value_tmp = (value >> 32) & 0xFFFFFFFF;
    WriteMemKernel(kernel_core_idx, MEM_CTRL_RD_CTRL_ADDR_1, value_tmp);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MemoryTest::SetWrCtrlAddrCores( uint64_t value )
{
    for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
        SetWrCtrlAddr(kernel_core_idx, value);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MemoryTest::SetWrCtrlXferBytes( int kernel_core_idx, uint value )
{
    uint value_tmp = (value - 1) & 0xFFFFFFFF;
    WriteMemKernel(kernel_core_idx, MEM_CTRL_WR_CTRL_XFER_BYTES_ADDR, value_tmp);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MemoryTest::SetWrCtrlXferBytesCores( uint value )
{
    for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
        SetWrCtrlXferBytes(kernel_core_idx, value);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MemoryTest::SetWrCtrlNumXfer( int kernel_core_idx, uint value )
{
    uint value_tmp = (value -1) & 0xFFFFFFFF;
    WriteMemKernel(kernel_core_idx, MEM_CTRL_WR_CTRL_NUM_XFER_ADDR, value_tmp);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MemoryTest::SetWrCtrlNumXferCores( uint value )
{
    for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
        SetWrCtrlNumXfer(kernel_core_idx, value);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MemoryTest::SetRdCtrlAddr( int kernel_core_idx, uint64_t value )
{
    uint value_tmp;
    value_tmp = value & 0xFFFFFFFF;
    WriteMemKernel(kernel_core_idx, MEM_CTRL_WR_CTRL_ADDR_0, value_tmp);
    value_tmp = (value >> 32) & 0xFFFFFFFF;
    WriteMemKernel(kernel_core_idx, MEM_CTRL_WR_CTRL_ADDR_1, value_tmp);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MemoryTest::SetRdCtrlAddrCores( uint64_t value )
{
    for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
        SetRdCtrlAddr(kernel_core_idx, value);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MemoryTest::SetRdCtrlXferBytes( int kernel_core_idx, uint value )
{
    uint value_tmp = (value -1) & 0xFFFFFFFF;
    WriteMemKernel(kernel_core_idx, MEM_CTRL_RD_CTRL_XFER_BYTES_ADDR, value_tmp);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MemoryTest::SetRdCtrlXferBytesCores( uint value )
{
    for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
        SetRdCtrlXferBytes(kernel_core_idx, value);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MemoryTest::SetRdCtrlNumXfer( int kernel_core_idx, uint value )
{
    uint value_tmp = (value - 1) & 0xFFFFFFFF;
    WriteMemKernel(kernel_core_idx, MEM_CTRL_RD_CTRL_NUM_XFER_ADDR, value_tmp);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MemoryTest::SetRdCtrlNumXferCores( uint value )
{
    for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
        SetRdCtrlNumXfer(kernel_core_idx, value);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint MemoryTest::GetStatWrTransferCnt( int kernel_core_idx )        { return ReadMemKernel(kernel_core_idx, MEM_STAT_WR_TRANSFER_CNT_ADDR);         }
uint MemoryTest::GetStatRdTransferCnt( int kernel_core_idx )        { return ReadMemKernel(kernel_core_idx, MEM_STAT_RD_TRANSFER_CNT_ADDR);         }

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint MemoryTest::GetHW1SecToggle( int kernel_core_idx )
{
    uint read_data = ReadMemKernel(kernel_core_idx, MEM_CTRL_ADDR);
    return (read_data >> 20) & 0x00000001;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool MemoryTest::CheckStatErrorEnLatch( int kernel_core_idx )
{
    uint read_data = ReadMemKernel(kernel_core_idx, MEM_CTRL_ADDR);
    if ((read_data & MEM_STAT_ERR) == MEM_STAT_ERR)
    {
        //LogMessage(LOG_DEBUG, "MEM_CTRL_ADDR 0x" + NumToStrHex<uint>(read_data));
        return true;
    }
    else
    {
        return false;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint64_t MemoryTest::GetAxiAddrPtr( int kernel_core_idx )
{
    uint64_t ret_data = 0x0000000000000000;
    uint read_data;

    read_data = ReadMemKernel(kernel_core_idx, MEM_STAT_AXI_ADDR_PTR_ADDR_1);
    ret_data = (uint64_t)read_data;
    ret_data = (ret_data << 32);

    read_data = ReadMemKernel(kernel_core_idx, MEM_STAT_AXI_ADDR_PTR_ADDR_0);
    ret_data |= (uint64_t)read_data;

    return ret_data;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool MemoryTest::CheckXferModBurst( TestItConfig_t test_it )
{
    if ((test_it.test_mode == CTRL_TEST_MODE_ONLY_RD_TEST_VAL) || (test_it.test_mode == CTRL_TEST_MODE_ALTERNATE_WR_RD_TEST_VAL))
    {
        std::string err_msg_rd = "read burst size does not fit evenly into total read transfer size: " + std::to_string(test_it.rd_num_xfer) + " mod " +  std::to_string(test_it.rd_burst_size) + " != 0";
        if (test_it.rd_burst_size == 0)
        {
            LogMessage(LOG_FAILURE, err_msg_rd);
            return true;
        }
        else if (test_it.rd_num_xfer % test_it.rd_burst_size != 0)
        {
            LogMessage(LOG_FAILURE, err_msg_rd);
            return true;
        }
    }
    if ((test_it.test_mode == CTRL_TEST_MODE_ONLY_WR_TEST_VAL) || (test_it.test_mode == CTRL_TEST_MODE_ALTERNATE_WR_RD_TEST_VAL))
    {
        std::string err_msg_wr = "write burst size does not fit evenly into total write transfer size: " + std::to_string(test_it.wr_num_xfer) + " mod " +  std::to_string(test_it.wr_burst_size) + " != 0";
        if (test_it.wr_burst_size == 0)
        {
            LogMessage(LOG_FAILURE, err_msg_wr);
            return true;
        }
        else if (test_it.wr_num_xfer % test_it.wr_burst_size != 0)
        {
            LogMessage(LOG_FAILURE, err_msg_wr);
            return true;
        }
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MemoryTest::SetSequenceCores( TestItConfig_t test_it )
{
    SetTestMode(test_it.test_mode);

    if ((test_it.test_mode == CTRL_TEST_MODE_ONLY_WR_TEST_VAL) || (test_it.test_mode == CTRL_TEST_MODE_ALTERNATE_WR_RD_TEST_VAL))
    {
        SetWrCtrlAddrCores      (test_it.wr_start_addr);
        SetWrCtrlXferBytesCores (test_it.wr_burst_size);
        SetWrCtrlNumXferCores   (test_it.wr_num_xfer);
    }
    else
    {
        SetWrCtrlAddrCores      (0);
        SetWrCtrlXferBytesCores (0);
        SetWrCtrlNumXferCores   (0);
    }

    if ((test_it.test_mode == CTRL_TEST_MODE_ONLY_RD_TEST_VAL) || (test_it.test_mode == CTRL_TEST_MODE_ALTERNATE_WR_RD_TEST_VAL))
    {
        SetRdCtrlAddrCores      (test_it.rd_start_addr);
        SetRdCtrlXferBytesCores (test_it.rd_burst_size);
        SetRdCtrlNumXferCores   (test_it.rd_num_xfer);
    }
    else
    {
        SetRdCtrlAddrCores      (0);
        SetRdCtrlXferBytesCores (0);
        SetRdCtrlNumXferCores   (0);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MemoryTest::WriteToMeasurementFileDetail( std::ofstream *measurement_file, int test_idx , TestItConfig_t test_it, Meas Wr_bw, Meas Rd_bw)
{
    if (m_use_outputfile == true)
    {
        (*measurement_file)
            << std::to_string(  test_idx        )   << ",";

        if (test_it.test_mode == CTRL_TEST_MODE_ALTERNATE_WR_RD_TEST_VAL)
        {
            (*measurement_file)
                << std::to_string(  Wr_bw.average   )   << ","
                << std::to_string(  Wr_bw.live      )   << ","
                << std::to_string(  Rd_bw.average   )   << ","
                << std::to_string(  Rd_bw.live      )   << ","
                << "\n";
        }
        else if (test_it.test_mode == CTRL_TEST_MODE_ONLY_WR_TEST_VAL)
        {
            (*measurement_file)
                << ","
                << ","
                << ","
                << ","
                << std::to_string(  Wr_bw.average   )   << ","
                << std::to_string(  Wr_bw.live      )   << ","
                << "\n";
        }
        else if (test_it.test_mode == CTRL_TEST_MODE_ONLY_RD_TEST_VAL)
        {
            (*measurement_file)
                << ","
                << ","
                << ","
                << ","
                << ","
                << ","
                << std::to_string(  Rd_bw.average   )   << ","
                << std::to_string(  Rd_bw.live      )   << ","
                << "\n";
        }
        else
        {
            LogMessage(LOG_ERROR, "Saving measurement not supported for this type of test: " + std::to_string(test_it.test_mode));
        }
        measurement_file->flush();
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MemoryTest::WriteToMeasurementFileResult( std::ofstream *measurement_file, int test_idx, TestItConfig_t test_it, Meas Wr_bw, Meas Rd_bw)
{
    if (m_use_outputfile == true)
    {
        (*measurement_file)
            << std::to_string(test_idx)             << ","
            << TestModeEnumToString(test_it.test_mode)    << ",";

        if (test_it.test_mode == CTRL_TEST_MODE_ALTERNATE_WR_RD_TEST_VAL)
        {
            (*measurement_file)
                << std::to_string(  test_it.wr_start_addr   )   << ","
                << std::to_string(  test_it.wr_burst_size   )   << ","
                << std::to_string(  test_it.wr_num_xfer     )   << ","
                << std::to_string(  test_it.rd_start_addr   )   << ","
                << std::to_string(  test_it.rd_burst_size   )   << ","
                << std::to_string(  test_it.rd_num_xfer     )   << ","
                << std::to_string(  Wr_bw.average           )   << ","
                << std::to_string(  Rd_bw.average           )   << ","
                << "\n";
        }
        else if (test_it.test_mode == CTRL_TEST_MODE_ONLY_WR_TEST_VAL)
        {
            (*measurement_file)
                << std::to_string(  test_it.wr_start_addr   )   << ","
                << std::to_string(  test_it.wr_burst_size   )   << ","
                << std::to_string(  test_it.wr_num_xfer     )   << ","
                << ","
                << ","
                << ","
                << ","
                << ","
                << std::to_string(  Wr_bw.average   )   << ","
                << "\n";
        }
        else if (test_it.test_mode == CTRL_TEST_MODE_ONLY_RD_TEST_VAL)
        {
            (*measurement_file)
                << ","
                << ","
                << ","
                << std::to_string(  test_it.rd_start_addr   )   << ","
                << std::to_string(  test_it.rd_burst_size   )   << ","
                << std::to_string(  test_it.rd_num_xfer     )   << ","
                << ","
                << ","
                << ","
                << std::to_string(  Rd_bw.average   )   << ","
                << "\n";
        }
        else
        {
            LogMessage(LOG_ERROR, "Saving measurement not supported for this type of test: " + std::to_string(test_it.test_mode));
        }
        measurement_file->flush();
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MemoryTest::PrintTestItConfig( TestItConfig_t test_it )
{
    LogMessage(LOG_INFO,   "\t Duration:      "      + std::to_string(test_it.duration) + "s");
    LogMessage(LOG_INFO,       "\t Type:          "      + TestModeEnumToString(test_it.test_mode));
    if ((test_it.test_mode == CTRL_TEST_MODE_ALTERNATE_WR_RD_TEST_VAL) || (test_it.test_mode == CTRL_TEST_MODE_ONLY_WR_TEST_VAL))
    {
        LogMessage(LOG_INFO,   "\t wr_start_addr: 0x"    + NumToStrHex<uint64_t>(test_it.wr_start_addr));
        LogMessage(LOG_INFO,   "\t wr_burst_size: "      + std::to_string(test_it.wr_burst_size));
        LogMessage(LOG_INFO,   "\t wr_num_xfer:   "      + std::to_string(test_it.wr_num_xfer));
    }
    if ((test_it.test_mode == CTRL_TEST_MODE_ALTERNATE_WR_RD_TEST_VAL) || (test_it.test_mode == CTRL_TEST_MODE_ONLY_RD_TEST_VAL))
    {
        LogMessage(LOG_INFO,   "\t rd_start_addr: 0x"    + NumToStrHex<uint64_t>(test_it.rd_start_addr));
        LogMessage(LOG_INFO,   "\t rd_burst_size: "      + std::to_string(test_it.rd_burst_size));
        LogMessage(LOG_INFO,   "\t rd_num_xfer:   "      + std::to_string(test_it.rd_num_xfer));
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int MemoryTest::RunThread( MemoryTestcaseCfg_t TC_Cfg, std::list<TestItConfig_t> *Tests_list )
{
    int ret = 0;

    bool test_it_failure = false;
    bool test_failure = false;
    bool pre_test_failure = false;
    int test_it_cnt = 1;

    bool test_started = false;

    LogMessage(LOG_DEBUG, "Device AXI address pointers configuration");
    for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
    {
        uint64_t axi_addr_ptr = GetAxiAddrPtr(kernel_core_idx);
        LogMessage(LOG_DEBUG, "\t - Channel " + std::to_string(kernel_core_idx) + " (" + GetMemKernelTag(kernel_core_idx) + "): 0x" + NumToStrHex<uint64_t>(axi_addr_ptr));
    }

    WaitSecTick(1);

    if (TC_Cfg.error_insertion == true)
    {
        LogMessage(LOG_INFO, "Run error insertion test");
        TestItConfig_t test_it;
        pre_test_failure |= GetErrorInsertionConfig(TC_Cfg, &test_it);
        if ((m_abort == false) && (pre_test_failure == false))
        {
            LogMessage(LOG_INFO, "Error insertion test configuration:");
            PrintTestItConfig(test_it);

            LogMessage(LOG_INFO, "Reset Memory Compute unit");
            test_started = false;
            ActivateReset();
            WaitSecTick(1);
            ClearReset();
            WaitSecTick(1);

            LogMessage(LOG_INFO, "Setup Memory Compute unit");
            SetSequenceCores(test_it);
            UpdateCfgKernelCores();
            pre_test_failure |= WaitCfgUpdated(test_it);
        }

        if ((m_abort == false) && (pre_test_failure == false))
        {
            LogMessage(LOG_INFO, "Start Memory Compute unit");
            StartKernel();
            test_started = true;
            WaitSecTick(1);  // let run a while before clearing
            ClearErrorCores();
            WaitSecTick(1);  // let run a while after clearing
            for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
            {
                if (CheckStatErrorEnLatch(kernel_core_idx)) {
                    std::string msg = "Power up error not cleared before error insertion test";
                    if (m_kernel_type == TEST_MEMORY_HBM)
                        msg += " for channel: " + std::to_string(kernel_core_idx) + " (" + GetMemKernelTag(kernel_core_idx) + ")";
                    LogMessage(LOG_ERROR, msg);
                    PrintRegHex(kernel_core_idx, MEM_CTRL_ADDR,                     "MEM_CTRL_ADDR"                     );
                    PrintRegHex(kernel_core_idx, MEM_STAT_TERM_ERROR_COUNT_ADDR,    "MEM_STAT_TERM_ERROR_COUNT_ADDR"    );
                    pre_test_failure = true;
                }
            }
        }

        if ((m_abort == false) && (pre_test_failure == false))
        {
            //check that each kernel has started by:
            //  1) injecting errors
            //  2) checking that only the target DDR detect error
            LogMessage(LOG_INFO, "Inject errors, let run " + std::to_string(test_it.duration) + "sec, check errors detected and clear errors");
            InsertErrorCores();
            WaitSecTick(test_it.duration);
            for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
            {
                if (CheckStatErrorEnLatch(kernel_core_idx))
                {
                    uint err_det = GetErrCnt(kernel_core_idx);
                    if ((m_err_qty[kernel_core_idx] != err_det))
                    {
                        std::string msg = "Quantity of error detected doesn't match the quantity of error injected";
                        if (m_kernel_type == TEST_MEMORY_HBM)
                            msg += " for channel: " + std::to_string(kernel_core_idx) + " (" + GetMemKernelTag(kernel_core_idx) + ")";
                        LogMessage(LOG_ERROR, msg);
                        LogMessage(LOG_INFO,  "Quantity of error detected: " + std::to_string(err_det));
                        LogMessage(LOG_INFO,  "Quantity of error injected: " + std::to_string(m_err_qty[kernel_core_idx]));
                        pre_test_failure = true;
                    }
                    else
                    {
                        std::string msg = "Expected injected " + std::to_string(m_err_qty[kernel_core_idx]) + " errors detected back";
                        if (m_kernel_type == TEST_MEMORY_HBM)
                            msg += " for channel: " + std::to_string(kernel_core_idx) + " (" + GetMemKernelTag(kernel_core_idx) + ")";
                        LogMessage(LOG_DEBUG, msg);
                    }
                }
                else
                {
                    std::string msg = "Injected error not detected";
                    if (m_kernel_type == TEST_MEMORY_HBM)
                        msg += " for channel: " + std::to_string(kernel_core_idx) + " (" + GetMemKernelTag(kernel_core_idx) + ")";
                    LogMessage(LOG_ERROR, msg);
                    pre_test_failure = true;
                }
            }
        }
        if ((m_abort == false) && (pre_test_failure == false))
        {
            ClearErrorCores();
            WaitSecTick(1);
            for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
            {
                if (CheckStatErrorEnLatch(kernel_core_idx)) {
                    std::string msg = "Error not cleared";
                    if (m_kernel_type == TEST_MEMORY_HBM)
                        msg += " channel: " + std::to_string(kernel_core_idx) + " (" + GetMemKernelTag(kernel_core_idx) + ")";
                    LogMessage(LOG_ERROR, msg);
                    PrintRegHex(kernel_core_idx, MEM_CTRL_ADDR,                     "MEM_CTRL_ADDR"                     );
                    PrintRegHex(kernel_core_idx, MEM_STAT_TERM_ERROR_COUNT_ADDR,    "MEM_STAT_TERM_ERROR_COUNT_ADDR"    );
                    pre_test_failure = true;
                    ClearError(kernel_core_idx);
                }
            }
        }

        // end of test
        LogMessage(LOG_INFO, "Stop Memory Compute unit");
        StopKernel();
        WaitSecTick(2);

        std::string insertion_pass_msg = "Error insertion test pass";
        std::string insertion_fail_msg = "Error insertion test fail";
        if (m_kernel_type == TEST_MEMORY_HBM)
        {
            insertion_pass_msg += " for each of " + std::to_string(m_num_kernel_core) + " channel(s)";
            insertion_fail_msg += " for some of " + std::to_string(m_num_kernel_core) + " channel(s)";
        }
        if ((m_abort == true) || (pre_test_failure == true))
            LogMessage(LOG_ERROR, insertion_fail_msg);
        else
            LogMessage(LOG_PASS, insertion_pass_msg);

        test_failure |= pre_test_failure;
    }

    LogMessage(LOG_INFO, "Reset Memory Compute unit");
    test_started = false;
    ActivateReset();
    WaitSecTick(1);
    ClearReset();
    WaitSecTick(1);

    if (pre_test_failure == false)
        LogMessage(LOG_DEBUG, "Number of test iterations: " + std::to_string(Tests_list->size()));

    for (auto test_it: *Tests_list)
    {
        if ((m_abort == true) || (pre_test_failure == true))
            break;

        Meas total_meas_bw_wr, total_meas_bw_rd;
        Meas meas_bw_wr[m_num_kernel_core], meas_bw_rd[m_num_kernel_core];

        total_meas_bw_wr = RESET_MEAS;
        total_meas_bw_rd = RESET_MEAS;
        for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
        {
            meas_bw_wr[kernel_core_idx] = RESET_MEAS;
            meas_bw_rd[kernel_core_idx] = RESET_MEAS;
            total_meas_bw_wr.average += meas_bw_wr[kernel_core_idx].average;
            total_meas_bw_rd.average += meas_bw_rd[kernel_core_idx].average;
        }

        test_it_failure = false;
        bool test_it_setup_failure = false;

        LogMessage(LOG_INFO, "Start Test: " + std::to_string(test_it_cnt));
        PrintTestItConfig(test_it);

        uint duration_divider = 1;
        if ((test_it.duration >= 10) && (test_it.duration < 100))
            duration_divider =  5;
        else if (test_it.duration >= 100)
            duration_divider = 20;

        if (test_it.test_mode == CTRL_TEST_MODE_STOP_TEST_VAL)
        {
            // Stop kernel at the end of last test
            LogMessage(LOG_INFO, "Stop Memory Compute unit");
            StopKernel();
            test_started = false;
        }
        else
        {
            LogMessage(LOG_INFO, "Setup Memory Compute unit");
            SetSequenceCores(test_it);
            UpdateCfgKernelCores();

            test_it_setup_failure |= WaitCfgUpdated(test_it);
            if (test_started == false)
            {
                LogMessage(LOG_INFO, "Start Memory Compute unit");
                StartKernel();
                test_started = true;
            }
            WaitSecTick(1);  // let run a while before clearing
            ClearErrorCores();
            WaitSecTick(1);  // let run a while after clearing

            if (test_it_setup_failure == false)
            {
                for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
                {
                    if (CheckStatErrorEnLatch(kernel_core_idx))
                    {
                        std::string msg = "Power up error not cleared";
                        if (m_kernel_type == TEST_MEMORY_HBM)
                            msg += " for channel: " + std::to_string(kernel_core_idx) + " (" + GetMemKernelTag(kernel_core_idx) + ")";
                        LogMessage(LOG_ERROR, msg);
                        PrintRegHex(kernel_core_idx,  MEM_CTRL_ADDR,                     "MEM_CTRL_ADDR"                     );
                        PrintRegHex(kernel_core_idx,  MEM_STAT_TERM_ERROR_COUNT_ADDR,    "MEM_STAT_TERM_ERROR_COUNT_ADDR"    );
                        test_it_setup_failure = true;
                        PrintConfig(kernel_core_idx);
                        ClearError(kernel_core_idx);
                    }
                }
            }
        }

        uint64_t t_start_it, t_stop_it;
        uint64_t t_start_all, t_stop_all;

        if ((m_abort == false) && (test_it_setup_failure == false) && (test_it.test_mode != CTRL_TEST_MODE_STOP_TEST_VAL))
        {
            LogMessage(LOG_INFO, "Let run and start computing bandwidth");
            WaitSecTick(1);
        }
        GetTimestamp(&t_start_all);
        uint tog_1sec = GetHW1SecToggle(0); // initialize 1 sec toggle value
        uint tog_1sec_last = tog_1sec;
        int toggle_error_cnt = 0;

        test_it_failure |= test_it_setup_failure;

        for (uint xfer_cnt = 0; xfer_cnt < test_it.duration && (test_it_setup_failure == false) && (m_abort == false); xfer_cnt++)
        {
            // Detect toggle only for channel 0 as external toggle mode is used (C_EXT_TOGGLE_1_SEC = 1)
            uint toggle_watchdog = 5; // Ensure 1 sec is detected in 5 * 250 ms = 1.25 sec
            tog_1sec = GetHW1SecToggle(0);

            while ((tog_1sec == tog_1sec_last) && (m_abort == false))
            {
                if (toggle_watchdog == 0)
                {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::microseconds(250000));
                tog_1sec = GetHW1SecToggle(0);
                toggle_watchdog --;
            }
            tog_1sec_last = tog_1sec;

            GetTimestamp(&t_start_it);

            if (toggle_watchdog == 0)
            {
                LogMessage(LOG_DEBUG, "1 sec toggle not detected");
                toggle_error_cnt ++;
            }
            else
            {
                //LogMessage(Logging::LOG_DEBUG, "1 sec toggle detected after " + std::to_string(5-toggle_watchdog) + " wait of 250ms");
                toggle_error_cnt = 0;
            }

            if (toggle_error_cnt >= 5)
                LogMessage(LOG_CRIT_WARN, "1 sec toggle not detected " + std::to_string(toggle_error_cnt) + " times consecutively");

            if (((xfer_cnt % duration_divider == 0) || (xfer_cnt == 0)) && (m_abort == false))
                LogMessage(LOG_STATUS, "\t" + std::to_string(test_it.duration-xfer_cnt) + " Seconds Remaining of Memory Test");

            if (test_it.test_mode != CTRL_TEST_MODE_STOP_TEST_VAL)
            {
                // Get measurements and compute average BW
                for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
                {
                    meas_bw_wr[kernel_core_idx].live  = ((double)GetStatWrTransferCnt(kernel_core_idx) * 64.0) / 1024.0 / 1024.0;
                    meas_bw_rd[kernel_core_idx].live  = ((double)GetStatRdTransferCnt(kernel_core_idx) * 64.0) / 1024.0 / 1024.0;
                    meas_bw_wr[kernel_core_idx].acc += meas_bw_wr[kernel_core_idx].live;
                    meas_bw_rd[kernel_core_idx].acc += meas_bw_rd[kernel_core_idx].live;
                    meas_bw_wr[kernel_core_idx].average = meas_bw_wr[kernel_core_idx].acc / (double)(xfer_cnt+1);
                    meas_bw_rd[kernel_core_idx].average = meas_bw_rd[kernel_core_idx].acc / (double)(xfer_cnt+1);
                }
                // total BW is sum of all Channel BW
                if (m_kernel_type == TEST_MEMORY_HBM)
                {
                    total_meas_bw_wr = RESET_MEAS;
                    total_meas_bw_rd = RESET_MEAS;
                    for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
                    {
                        total_meas_bw_wr.live    += meas_bw_wr[kernel_core_idx].live;
                        total_meas_bw_wr.average += meas_bw_wr[kernel_core_idx].average;
                        total_meas_bw_rd.live    += meas_bw_rd[kernel_core_idx].live;
                        total_meas_bw_rd.average += meas_bw_rd[kernel_core_idx].average;
                    }
                }

                // Write Results
                for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
                    WriteToMeasurementFileDetail(&(m_outputfile_detail[kernel_core_idx]), test_it_cnt, test_it, meas_bw_wr[kernel_core_idx], meas_bw_rd[kernel_core_idx]);
                if (m_kernel_type == TEST_MEMORY_HBM)
                    WriteToMeasurementFileDetail(&m_outputfile_detail_total, test_it_cnt, test_it, total_meas_bw_wr, total_meas_bw_rd);

                // Regularly check for error
                if ((test_it.test_mode == CTRL_TEST_MODE_ALTERNATE_WR_RD_TEST_VAL) || (test_it.test_mode == CTRL_TEST_MODE_ONLY_RD_TEST_VAL))
                {
                    for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
                    {
                        if (CheckStatErrorEnLatch(kernel_core_idx))
                        {
                            std::string msg = "Unexpected Error detected";
                            if (m_kernel_type == TEST_MEMORY_HBM)
                                msg += " for channel: " + std::to_string(kernel_core_idx) + " (" + GetMemKernelTag(kernel_core_idx) + ")";
                            LogMessage(LOG_ERROR, msg);
                            PrintRegHex(kernel_core_idx, MEM_CTRL_ADDR,                     "MEM_CTRL_ADDR"                     );
                            PrintRegHex(kernel_core_idx, MEM_STAT_TERM_ERROR_COUNT_ADDR,    "MEM_STAT_TERM_ERROR_COUNT_ADDR"    );
                            test_it_failure = true;
                            ClearError(kernel_core_idx);
                        }
                    }
                }
            }
            // Check loop duration
            if (m_abort == false)
            {
                GetTimestamp(&t_stop_it);
                if ((t_stop_it - t_start_it) > 1000000)
                {
                    LogMessage(LOG_DEBUG, "Iteration execution took more than 1 second");
                    LogMessage(LOG_DEBUG, "Iteration elapsed time: " + Float_to_String<double>(((double)t_stop_it - (double)t_start_it)/1000000.0,3) + " sec. Expected duration was: 1 sec");
                }
            }
        }

        if ((m_abort == false) && (test_it_setup_failure == false))
        {
            GetTimestamp(&t_stop_all);
            LogMessage(LOG_DEBUG, "Total elapsed time: " + Float_to_String<double>(((double)t_stop_all - (double)t_start_all)/1000000.0,3) + " sec. Requested duration was: " + std::to_string(test_it.duration) + " sec");
        }

        IPC_queue.push(0);
        // Don't stop kernel at the end of the test iteration

        // check for error for data integrity
        if ((m_abort == false) && (test_it.test_mode != CTRL_TEST_MODE_STOP_TEST_VAL))
        {
            if (test_it.test_mode == CTRL_TEST_MODE_ONLY_WR_TEST_VAL)
            {
                LogMessage(LOG_INFO, "Data integrity not check as test is only write");
            }
            else
            {
                for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx && (m_abort == false); kernel_core_idx++)
                {
                    if (CheckStatErrorEnLatch(kernel_core_idx))
                    {
                        if (m_kernel_type == TEST_MEMORY_HBM)
                            LogMessage(LOG_ERROR, "Test did not maintain data integrity for channel: " + std::to_string(kernel_core_idx) + " (" + GetMemKernelTag(kernel_core_idx) + ")");
                        PrintRegHex(kernel_core_idx, MEM_CTRL_ADDR,                     "MEM_CTRL_ADDR"                     );
                        PrintRegHex(kernel_core_idx, MEM_STAT_TERM_ERROR_COUNT_ADDR,    "MEM_STAT_TERM_ERROR_COUNT_ADDR"    );
                        test_it_failure = true;
                        ClearError(kernel_core_idx);
                    }
                    else
                    {
                        if (m_kernel_type == TEST_MEMORY_HBM)
                            LogMessage(LOG_DEBUG, "Data integrity test pass for channel: " + std::to_string(kernel_core_idx) + " (" + GetMemKernelTag(kernel_core_idx) + ")");
                    }
                }
                std::string integrity_pass_msg, integrity_fail_msg;
                if (m_kernel_type == TEST_MEMORY_DDR)
                {
                    integrity_pass_msg = "Data integrity test pass";
                    integrity_fail_msg = "Data integrity test fail";
                }
                else if (m_kernel_type == TEST_MEMORY_HBM)
                {
                    integrity_pass_msg = "Data integrity test pass for each of " + std::to_string(m_num_kernel_core) + " channel(s)";
                    integrity_fail_msg = "Data integrity test fail for some of " + std::to_string(m_num_kernel_core) + " channel(s)";
                }
                if ((m_abort == false) && (test_it_failure == false))
                    LogMessage(LOG_PASS, integrity_pass_msg);
                else
                    LogMessage(LOG_ERROR, integrity_fail_msg);
            }
        }

        for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
            WriteToMeasurementFileResult(&(m_outputfile_result[kernel_core_idx]), test_it_cnt, test_it, meas_bw_wr[kernel_core_idx], meas_bw_rd[kernel_core_idx]);
        if (m_kernel_type == TEST_MEMORY_HBM)
            WriteToMeasurementFileResult(&m_outputfile_result_total, test_it_cnt, test_it, total_meas_bw_wr, total_meas_bw_rd);


        LogLevel log_level_pass = LOG_PASS; // Message severity for individual result check
        if      (m_kernel_type == TEST_MEMORY_DDR) log_level_pass = LOG_PASS;
        else if (m_kernel_type == TEST_MEMORY_HBM) log_level_pass = LOG_DEBUG;

        if (m_abort == false)
        {
            if (test_it.test_mode == CTRL_TEST_MODE_STOP_TEST_VAL)
            {
                if ((m_abort == false) && (test_it_failure == false))
                    LogMessage(LOG_PASS,  "Stop test passed");
                else
                    LogMessage(LOG_ERROR, "Stop test failed");
            }
            else
            {
                for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
                {
                    if ((test_it.test_mode == CTRL_TEST_MODE_ONLY_RD_TEST_VAL) || (test_it.test_mode == CTRL_TEST_MODE_ALTERNATE_WR_RD_TEST_VAL))
                        test_it_failure |= CheckResult(log_level_pass, LOG_ERROR, "Read", TC_Cfg, test_it, kernel_core_idx, meas_bw_rd[kernel_core_idx]);
                    if ((test_it.test_mode == CTRL_TEST_MODE_ONLY_WR_TEST_VAL) || (test_it.test_mode == CTRL_TEST_MODE_ALTERNATE_WR_RD_TEST_VAL))
                        test_it_failure |= CheckResult(log_level_pass, LOG_ERROR, "Write", TC_Cfg, test_it, kernel_core_idx, meas_bw_wr[kernel_core_idx]);
                }

                if (TC_Cfg.check_bw == true)
                {
                    std::string bw_pass_msg, bw_fail_msg;
                    if (m_kernel_type == TEST_MEMORY_DDR)
                    {
                        bw_pass_msg = "Bandwidth test pass";
                        bw_fail_msg = "Bandwidth test fail";
                    }
                    else if (m_kernel_type == TEST_MEMORY_HBM)
                    {
                        bw_pass_msg = "Bandwidth test pass for each of " + std::to_string(m_num_kernel_core) + " channel(s)";
                        bw_fail_msg = "Bandwidth test fail for some of " + std::to_string(m_num_kernel_core) + " channel(s)";
                    }

                    if ((m_abort == false) && (test_it_failure == false))
                        LogMessage(LOG_PASS,  bw_pass_msg);
                    else
                        LogMessage(LOG_ERROR, bw_fail_msg);

                    if (m_kernel_type == TEST_MEMORY_HBM)
                    {
                        if ((test_it.test_mode == CTRL_TEST_MODE_ONLY_RD_TEST_VAL) || (test_it.test_mode == CTRL_TEST_MODE_ALTERNATE_WR_RD_TEST_VAL))
                            test_it_failure |= CheckResult(LOG_PASS, LOG_ERROR, "Read", TC_Cfg, test_it, -1, total_meas_bw_rd);
                        if ((test_it.test_mode == CTRL_TEST_MODE_ONLY_WR_TEST_VAL) || (test_it.test_mode == CTRL_TEST_MODE_ALTERNATE_WR_RD_TEST_VAL))
                            test_it_failure |= CheckResult(LOG_PASS, LOG_ERROR, "Write", TC_Cfg, test_it, -1, total_meas_bw_wr);
                    }
                }
            }
        }

        LogMessage(LOG_INFO, "End Test: " + std::to_string(test_it_cnt));
        test_failure |= test_it_failure;
        test_it_cnt++;
    }

    LogMessage(LOG_INFO, "Stop Memory Compute unit");
    StopKernel();
    test_started = false;

    ////////////////////////////////////////////////////////////////////////////////////////////////////

    if (m_abort == true)
    {
        ret = -1;
    }
    else if (test_failure) // Do not print pass/error message if aborted
    {
        LogMessage(LOG_ERROR, "Test failed");
        ret = 1;
    }
    else
    {
        LogMessage(LOG_PASS, "Test passed");
        ret = 0;
    }

    return ret;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool MemoryTest::CheckBWInRange( LogLevel log_level_pass, LogLevel log_level_fail, double value, double min, double max, std::string msg )
{
    bool test_failure = false;
    if ( (value >= min) && (value <= max))
    {
        LogMessage(log_level_pass, msg + std::to_string(value) + " MBps inside the range [" + std::to_string(min) + ", " + std::to_string(max) +  "]");
    }
    else
    {
        LogMessage(log_level_fail, msg + std::to_string(value) + " MBps outside the range [" + std::to_string(min) + ", " + std::to_string(max) +  "]");
        if ((log_level_fail == LOG_ERROR) || (log_level_fail == LOG_FAILURE))
            test_failure = true;
    }
    return test_failure;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MemoryTest::GetBWRange ( MemoryTestcaseCfg_t TC_Cfg, TestItConfig_t *test_it )
{
    if (test_it->test_mode == CTRL_TEST_MODE_ALTERNATE_WR_RD_TEST_VAL)
    {
        test_it->thresh_wr_rd.read.low    = (double)TC_Cfg.cu_bw.alt_wr_rd.read.low;
        test_it->thresh_wr_rd.read.high   = (double)TC_Cfg.cu_bw.alt_wr_rd.read.high;
        test_it->thresh_wr_rd.write.low   = (double)TC_Cfg.cu_bw.alt_wr_rd.write.low;
        test_it->thresh_wr_rd.write.high  = (double)TC_Cfg.cu_bw.alt_wr_rd.write.high;
    }
    else if (test_it->test_mode == CTRL_TEST_MODE_ONLY_RD_TEST_VAL)
    {
        test_it->thresh_wr_rd.read.low    = (double)TC_Cfg.cu_bw.only_rd.read.low;
        test_it->thresh_wr_rd.read.high   = (double)TC_Cfg.cu_bw.only_rd.read.high;
    }
    else if (test_it->test_mode == CTRL_TEST_MODE_ONLY_WR_TEST_VAL)
    {
        test_it->thresh_wr_rd.write.low   = (double)TC_Cfg.cu_bw.only_wr.write.low;
        test_it->thresh_wr_rd.write.high  = (double)TC_Cfg.cu_bw.only_wr.write.high;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T>  T MemoryTest::SelectParamDDRorHBM( T sel_val_ddr, T sel_val_hbm )
{
    T sel_val = 0;
    if (m_kernel_type == TEST_MEMORY_DDR)
        sel_val = sel_val_ddr;
    else if (m_kernel_type == TEST_MEMORY_HBM)
        sel_val = sel_val_hbm;
    return sel_val;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MemoryTest::MemoryTest( Xbtest_Pfm_Def_t xbtest_pfm_def, DeviceInterface *device, DeviceMgt *device_mgt, Testcase_Parameters_t test_parameters, TestType kernel_type, int kernel_idx, Global_Config_t global_config )
{
    m_state     = TestState::TS_NOT_SET;
    m_result    = TestResult::TR_PASSED;

    m_log       = Logging::getInstance();
    m_abort     = false;

    m_xbtest_pfm_def = xbtest_pfm_def;
    m_device = device;
    m_devicemgt = device_mgt;
    m_test_parameters = test_parameters;
    if ((kernel_type == TEST_MEMORY_DDR) || (kernel_type == TEST_MEMORY_HBM))
        m_kernel_type       = kernel_type;
    else
        LogMessage(LOG_FAILURE, "Cannot run Memory Test for type " + TestTypeToString(kernel_type));
    m_kernel_idx    = kernel_idx;
    m_global_config = global_config;


    if  (m_kernel_type == TEST_MEMORY_DDR)
        m_log_msg_test_type = "MEMORY_TEST: " + GetMemKernelTag(0) + ": ";  // Only 1 port for DDR memory test kernel
    else if (m_kernel_type == TEST_MEMORY_HBM)
        m_log_msg_test_type = "MEMORY_TEST: HBM   : ";
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MemoryTest::~MemoryTest()
{
    for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
    {
        m_outputfile_detail[kernel_core_idx].flush();
        m_outputfile_result[kernel_core_idx].flush();

        m_outputfile_detail[kernel_core_idx].close();
        m_outputfile_result[kernel_core_idx].close();
    }
    m_outputfile_detail_total.flush();
    m_outputfile_result_total.flush();

    m_outputfile_detail_total.close();
    m_outputfile_result_total.close();
}

void MemoryTest::WaitSecTick(uint quantity)
{
    for (uint i=0; i<quantity && (m_abort == false); i++)
    {
        m_devicemgt->WaitFor1sTick();
        if ( (i % NUM_SEC_WATCHDOG == 0) && (quantity >= NUM_SEC_WATCHDOG) )  ResetWatchdog();
    }
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool MemoryTest::PreSetup()
{
    bool ret = true;
    LogMessage(LOG_INFO, "PreSetup");
    m_state = TestState::TS_PRE_SETUP;
    return ret;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool MemoryTest::ParseTestSequenceSettings( MemoryTestcaseCfg_t TC_Cfg, std::list<TestItConfig_t> *test_list )
{
    bool parse_failure = false;
    uint  parse_error_cnt = 0;
    int  test_cnt = 0;
    TestItConfig_t test_it_cfg;

    std::vector<Memory_Test_Sequence_Parameters_t> test_sequence;
    Json_Parameters_t::iterator it = FindJsonParam(&(m_test_parameters.param), TEST_SEQUENCE_MEMBER);
    if (it != m_test_parameters.param.end())
        test_sequence = TestcaseParamCast<std::vector<Memory_Test_Sequence_Parameters_t>>(it->second);

    for (auto test_seq_param : test_sequence)
    {
        if (m_abort == true) break;
        test_cnt ++;
        bool parse_it_failure = false;
        bool default_config = false;

        // test_mode value already checked in inputparser
        test_it_cfg.test_mode = TestModeStringToEnum(test_seq_param.test_mode);

        if (parse_it_failure == false)
        {
            parse_it_failure |= CheckParam<uint>(DURATION, test_seq_param.duration, MIN_DURATION, MAX_DURATION);
            test_it_cfg.duration = test_seq_param.duration;
        }
        if (test_it_cfg.test_mode == CTRL_TEST_MODE_STOP_TEST_VAL) continue; // No parameter to set in that case

        if (test_seq_param.num_param == NUM_TEST_SEQ_PARAM_MEMORY_DEF)
        {
            default_config = true;
            // Set all parameters to default values when only test_mode and duration provided
            test_it_cfg.wr_start_addr = m_min_ctrl_addr;
            test_it_cfg.wr_burst_size = m_max_burst_size;
            test_it_cfg.wr_num_xfer   = m_max_ctrl_num_xfer;
            test_it_cfg.rd_start_addr = m_min_ctrl_addr;
            test_it_cfg.rd_burst_size = m_max_burst_size;
            test_it_cfg.rd_num_xfer   = m_max_ctrl_num_xfer;
        }
        else
        {
            if (parse_it_failure == false)
            {
                if (test_it_cfg.test_mode != CTRL_TEST_MODE_ONLY_RD_TEST_VAL)
                {
                    parse_it_failure |= CheckParam<uint64_t>(WR_START_ADDR, test_seq_param.wr_start_addr, m_min_ctrl_addr, m_max_ctrl_addr);
                    test_it_cfg.wr_start_addr = test_seq_param.wr_start_addr;
                }
                else
                {
                    parse_it_failure |= CheckParam<uint64_t>(RD_START_ADDR, test_seq_param.rd_start_addr, m_min_ctrl_addr, m_max_ctrl_addr);
                    test_it_cfg.rd_start_addr = test_seq_param.rd_start_addr;
                }
            }
            if (parse_it_failure == false)
            {
                if (test_it_cfg.test_mode != CTRL_TEST_MODE_ONLY_RD_TEST_VAL)
                {
                    parse_it_failure |= CheckParam<uint>(WR_BURST_SIZE, test_seq_param.wr_burst_size, m_min_burst_size, m_max_burst_size);
                    test_it_cfg.wr_burst_size = test_seq_param.wr_burst_size;
                }
                else
                {
                    parse_it_failure |= CheckParam<uint>(RD_BURST_SIZE, test_seq_param.rd_burst_size, m_min_burst_size, m_max_burst_size);
                    test_it_cfg.rd_burst_size = test_seq_param.rd_burst_size;
                }
            }
            if (parse_it_failure == false)
            {
                if (test_it_cfg.test_mode != CTRL_TEST_MODE_ONLY_RD_TEST_VAL)
                {
                    parse_it_failure |= CheckParam<uint>(WR_NUM_XFER, test_seq_param.wr_num_xfer, test_it_cfg.wr_burst_size, m_max_ctrl_num_xfer - test_it_cfg.wr_start_addr/64);
                    test_it_cfg.wr_num_xfer = test_seq_param.wr_num_xfer;
                }
                else
                {
                    parse_it_failure |= CheckParam<uint>(RD_NUM_XFER, test_seq_param.rd_num_xfer, test_it_cfg.rd_burst_size, m_max_ctrl_num_xfer - test_it_cfg.rd_start_addr/64);
                    test_it_cfg.rd_num_xfer = test_seq_param.rd_num_xfer;
                }
            }
            if (parse_it_failure == false)
            {
                if (test_it_cfg.test_mode == CTRL_TEST_MODE_ALTERNATE_WR_RD_TEST_VAL)
                {
                    parse_it_failure |= CheckParam<uint64_t>(RD_START_ADDR, test_seq_param.rd_start_addr, m_min_ctrl_addr, m_max_ctrl_addr);
                    test_it_cfg.rd_start_addr = test_seq_param.rd_start_addr;
                }
            }
            if (parse_it_failure == false)
            {
                if (test_it_cfg.test_mode == CTRL_TEST_MODE_ALTERNATE_WR_RD_TEST_VAL)
                {
                    parse_it_failure |= CheckParam<uint>(RD_BURST_SIZE, test_seq_param.rd_burst_size, m_min_burst_size, m_max_burst_size);
                    test_it_cfg.rd_burst_size = test_seq_param.rd_burst_size;
                }
            }
            if (parse_it_failure == false)
            {
                if (test_it_cfg.test_mode == CTRL_TEST_MODE_ALTERNATE_WR_RD_TEST_VAL)
                {
                    parse_it_failure |= CheckParam<uint>(RD_NUM_XFER, test_seq_param.rd_num_xfer, test_it_cfg.rd_burst_size, m_max_ctrl_num_xfer - test_it_cfg.rd_start_addr/64);
                    test_it_cfg.rd_num_xfer = test_seq_param.rd_num_xfer;
                }
            }
        }

        if (parse_it_failure == false) parse_it_failure |= CheckXferModBurst(test_it_cfg);
        GetBWRange(TC_Cfg, &test_it_cfg);
        if (parse_it_failure == false) parse_it_failure |= GetConfigurationUpdateTime(TC_Cfg, &test_it_cfg);

        ////////////////////////////////////////////////////////////////////

        if (parse_it_failure == true)
        {
            LogMessage(LOG_FAILURE, "Test " + std::to_string(test_cnt) + ": invalid parameters" );
            parse_error_cnt ++;
            if (parse_error_cnt > MAX_NUM_PARSER_ERROR) break;
        }
        else
        {
            test_list->push_back(test_it_cfg);
            std::string params = "";
            params += "\"" + TestModeEnumToString(test_it_cfg.test_mode) + "\", ";
            params += std::to_string(test_it_cfg.duration);

            if (test_it_cfg.test_mode != CTRL_TEST_MODE_STOP_TEST_VAL)
                params += ", ";

            if (test_it_cfg.test_mode == CTRL_TEST_MODE_ALTERNATE_WR_RD_TEST_VAL)
            {
                params += std::to_string(test_it_cfg.wr_start_addr) + ", ";
                params += std::to_string(test_it_cfg.wr_burst_size) + ", ";
                params += std::to_string(test_it_cfg.wr_num_xfer)   + ", ";
                params += std::to_string(test_it_cfg.rd_start_addr) + ", ";
                params += std::to_string(test_it_cfg.rd_burst_size) + ", ";
                params += std::to_string(test_it_cfg.rd_num_xfer);
            }
            if (test_it_cfg.test_mode == CTRL_TEST_MODE_ONLY_WR_TEST_VAL)
            {
                params += std::to_string(test_it_cfg.wr_start_addr) + ", ";
                params += std::to_string(test_it_cfg.wr_burst_size) + ", ";
                params += std::to_string(test_it_cfg.wr_num_xfer);
            }
            if (test_it_cfg.test_mode == CTRL_TEST_MODE_ONLY_RD_TEST_VAL)
            {
                params += std::to_string(test_it_cfg.rd_start_addr) + ", ";
                params += std::to_string(test_it_cfg.rd_burst_size) + ", ";
                params += std::to_string(test_it_cfg.rd_num_xfer);
            }
            if (default_config == true)
                LogMessage(LOG_INFO, "Test " + std::to_string(test_cnt) + " parameters: " + params + ". (Set to default configuration)");
            else
                LogMessage(LOG_DEBUG, "Test " + std::to_string(test_cnt) + " parameters: " + params);
        }

        parse_failure |= parse_it_failure;

    }
    return parse_failure;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MemoryTest::Run()
{
    LogMessage(LOG_STATUS, "Run");

    // parse testcase parameters
    bool global_settings_failure = false;
    MemoryTestcaseCfg_t TC_Cfg;

    Json_Parameters_t::iterator it;

    m_state  = TestState::TS_RUNNING;
    m_result = TestResult::TR_FAILED;

    m_num_kernel_core   = GetMemKernelNumCore();
    m_kernel_num_mem    = GetMemKernelNumMem();
    m_kernel_inst       = GetMemKernelInst();

    // Select all cores to test
    m_min_kernel_core_idx = 0;
    m_max_kernel_core_idx = m_num_kernel_core-1;

    // DBG select specific cores
    // m_min_kernel_core_idx = 12;
    // m_max_kernel_core_idx = 12;

    m_num_kernel_core = m_max_kernel_core_idx-m_min_kernel_core_idx+1;

    ///////////////////////////////////////////////////////////////////////
    // Set the boundaries of the test parameters depending on the test type
    ///////////////////////////////////////////////////////////////////////

    uint memory_size_mb = SelectParamDDRorHBM<uint>(m_xbtest_pfm_def.memory.ddr.size, m_xbtest_pfm_def.memory.hbm.size);
    TC_Cfg.memory_size = (uint64_t)memory_size_mb*1024*1024;

    m_min_burst_size    = MIN_BURST_SIZE;
    m_min_ctrl_num_xfer = m_min_burst_size; // num_xfer must be multiple of burst_size, will be set to m_min_burst_size or test_it_cfg.wr_burst_size
    m_min_ctrl_addr     = MIN_CTRL_ADDR;

    m_max_burst_size    = MAX_BURST_SIZE;
    m_max_ctrl_num_xfer = (uint)(TC_Cfg.memory_size*(uint64_t)(m_kernel_num_mem)/64); // will then depend on start address
    m_max_ctrl_addr     = TC_Cfg.memory_size*(uint64_t)(m_kernel_num_mem) - (uint64_t)(m_min_ctrl_num_xfer)*64;

    LogMessage(LOG_DEBUG, "Parameter boundaries:");
    LogMessage(LOG_DEBUG, "\t - Address Min: " + std::to_string(m_min_ctrl_addr));
    LogMessage(LOG_DEBUG, "\t - Address Max: " + std::to_string(m_max_ctrl_addr));
    LogMessage(LOG_DEBUG, "\t - Burst size Min: " + std::to_string(m_min_burst_size));
    LogMessage(LOG_DEBUG, "\t - Burst size Max: " + std::to_string(m_max_burst_size));
    LogMessage(LOG_DEBUG, "\t - Number of transfers Min: will be set to test burst size");
    LogMessage(LOG_DEBUG, "\t - Number of transfers Max: " + std::to_string(m_max_ctrl_num_xfer));

    ///////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////
    // Get verbosity if defined in JSON, else use top level one
    int verbosity = (int)m_global_config.verbosity;
    bool verbos_ret = GetVerbosity(&(m_test_parameters.param), &verbosity);
    if (verbos_ret == true)
        LogMessage(LOG_FAILURE, VERBOSITY_FAILURE);
    global_settings_failure |= verbos_ret;
    m_global_config.verbosity = static_cast<LogLevel>(verbosity);

    ///////////////////////////////////////////////////////////////////////
    // Test parameters
    global_settings_failure |= GetJsonParamStr(TEST_SOURCE_MEMBER, SUPPORTED_TEST_SOURCE, &(TC_Cfg.test_source), TEST_SOURCE_MEMBER_JSON);
    global_settings_failure |= GetJsonParamBool(ERROR_INSERTION_MEMBER, &(TC_Cfg.error_insertion),  false);

    // Get thresholds
    if (m_kernel_type == TEST_MEMORY_DDR)
        TC_Cfg.cu_bw = m_xbtest_pfm_def.memory.ddr.cu_bw;
    else if (m_kernel_type == TEST_MEMORY_HBM)
        TC_Cfg.cu_bw = m_xbtest_pfm_def.memory.hbm.cu_bw;

    global_settings_failure |= GetJsonParamNum<uint>(LO_THRESH_ALT_WR_BW_MEMBER, MIN_LO_THRESH_ALT_WR_BW,  TC_Cfg.cu_bw.alt_wr_rd.write.low,   MAX_LO_THRESH_ALT_WR_BW,  &(TC_Cfg.cu_bw.alt_wr_rd.write.low));
    global_settings_failure |= GetJsonParamNum<uint>(HI_THRESH_ALT_WR_BW_MEMBER, MIN_HI_THRESH_ALT_WR_BW,  TC_Cfg.cu_bw.alt_wr_rd.write.high,  MAX_HI_THRESH_ALT_WR_BW,  &(TC_Cfg.cu_bw.alt_wr_rd.write.high));
    global_settings_failure |= CheckThresholdLoVsHi<uint>(LO_THRESH_ALT_WR_BW_MEMBER, TC_Cfg.cu_bw.alt_wr_rd.write.low, HI_THRESH_ALT_WR_BW_MEMBER, TC_Cfg.cu_bw.alt_wr_rd.write.high);

    global_settings_failure |= GetJsonParamNum<uint>(LO_THRESH_ALT_RD_BW_MEMBER, MIN_LO_THRESH_ALT_RD_BW,  TC_Cfg.cu_bw.alt_wr_rd.read.low,    MAX_LO_THRESH_ALT_RD_BW,  &(TC_Cfg.cu_bw.alt_wr_rd.read.low));
    global_settings_failure |= GetJsonParamNum<uint>(HI_THRESH_ALT_RD_BW_MEMBER, MIN_HI_THRESH_ALT_RD_BW,  TC_Cfg.cu_bw.alt_wr_rd.read.high,   MAX_HI_THRESH_ALT_RD_BW,  &(TC_Cfg.cu_bw.alt_wr_rd.read.high));
    global_settings_failure |= CheckThresholdLoVsHi<uint>(LO_THRESH_ALT_RD_BW_MEMBER, TC_Cfg.cu_bw.alt_wr_rd.read.low, HI_THRESH_ALT_RD_BW_MEMBER, TC_Cfg.cu_bw.alt_wr_rd.read.high);

    global_settings_failure |= GetJsonParamNum<uint>(LO_THRESH_ONLY_WR_BW_MEMBER, MIN_LO_THRESH_ONLY_WR_BW, TC_Cfg.cu_bw.only_wr.write.low,     MAX_LO_THRESH_ONLY_WR_BW, &(TC_Cfg.cu_bw.only_wr.write.low));
    global_settings_failure |= GetJsonParamNum<uint>(HI_THRESH_ONLY_WR_BW_MEMBER, MIN_HI_THRESH_ONLY_WR_BW, TC_Cfg.cu_bw.only_wr.write.high,    MAX_HI_THRESH_ONLY_WR_BW, &(TC_Cfg.cu_bw.only_wr.write.high));
    global_settings_failure |= CheckThresholdLoVsHi<uint>(LO_THRESH_ONLY_WR_BW_MEMBER, TC_Cfg.cu_bw.only_wr.write.low, HI_THRESH_ONLY_WR_BW_MEMBER, TC_Cfg.cu_bw.only_wr.write.high);

    global_settings_failure |= GetJsonParamNum<uint>(LO_THRESH_ONLY_RD_BW_MEMBER, MIN_LO_THRESH_ONLY_RD_BW, TC_Cfg.cu_bw.only_rd.read.low,      MAX_LO_THRESH_ONLY_RD_BW, &(TC_Cfg.cu_bw.only_rd.read.low));
    global_settings_failure |= GetJsonParamNum<uint>(HI_THRESH_ONLY_RD_BW_MEMBER, MIN_HI_THRESH_ONLY_RD_BW, TC_Cfg.cu_bw.only_rd.read.high,     MAX_HI_THRESH_ONLY_RD_BW, &(TC_Cfg.cu_bw.only_rd.read.high));
    global_settings_failure |= CheckThresholdLoVsHi<uint>(LO_THRESH_ONLY_RD_BW_MEMBER, TC_Cfg.cu_bw.only_rd.read.low, HI_THRESH_ONLY_RD_BW_MEMBER, TC_Cfg.cu_bw.only_rd.read.high);

    // Mode BW checked
    global_settings_failure |= GetJsonParamBool(CHECK_BW_MEMBER, &(TC_Cfg.check_bw), true);

    //////////////////////////////////////
    // measurement output file, Default value, don't write any output
    //////////////////////////////////////
    it = FindJsonParam(&(m_test_parameters.param), OUTPUT_FILE_MEMBER);
    if (it != m_test_parameters.param.end())
    {
        m_use_outputfile = true;

        std::string first_line_result = "";
        first_line_result += "Test,";
        first_line_result += "Mode,";
        first_line_result += "Duration,";
        first_line_result += "wr_start_addr,";
        first_line_result += "wr_burst_size,";
        first_line_result += "wr_num_xfer,";
        first_line_result += "rd_start_addr,";
        first_line_result += "rd_burst_size,";
        first_line_result += "rd_num_xfer,";
        first_line_result += "Alt Wr Bw,";
        first_line_result += "Alt Rd Bw,";
        first_line_result += "Only Wr Bw,";
        first_line_result += "Only Rd Bw,\n";

        std::string first_line_detail = "";
        first_line_detail += "Test,";
        first_line_detail += "Alt Wr Bw,";
        first_line_detail += "Alt Wr Bw live,";
        first_line_detail += "Alt Rd Bw,";
        first_line_detail += "Alt Rd Bw live,";
        first_line_detail += "Only Wr Bw,";
        first_line_detail += "Only Wr Bw live,";
        first_line_detail += "Only Rd Bw,";
        first_line_detail += "Only Rd Bw live,\n";

        for (int kernel_core_idx=m_min_kernel_core_idx; kernel_core_idx<=m_max_kernel_core_idx; kernel_core_idx++)
        {
            std::string filename = TestcaseParamCast<std::string>(it->second) + "_" + GetMemKernelTag2(kernel_core_idx) + "_";
            global_settings_failure |= OpenOutputFile(filename + "detail.csv", &(m_outputfile_detail[kernel_core_idx]));
            global_settings_failure |= OpenOutputFile(filename + "result.csv", &(m_outputfile_result[kernel_core_idx]));

            if (global_settings_failure == false)
            {
                // write the first line of the file containing the description of each column
                m_outputfile_detail[kernel_core_idx] << first_line_detail;
                m_outputfile_detail[kernel_core_idx].flush();
                m_outputfile_result[kernel_core_idx] << first_line_result;
                m_outputfile_result[kernel_core_idx].flush();
            }
        }
        if (m_kernel_type == TEST_MEMORY_HBM)
        {
            std::string filename = TestcaseParamCast<std::string>(it->second) + "_HBM_";
            global_settings_failure |= OpenOutputFile(filename + "detail_total.csv", &m_outputfile_detail_total);
            global_settings_failure |= OpenOutputFile(filename + "result_total.csv", &m_outputfile_result_total);
            if (global_settings_failure == false)
            {
                // write the first line of the file containing the description of each column
                m_outputfile_detail_total << first_line_detail;
                m_outputfile_detail_total.flush();
                m_outputfile_result_total << first_line_result;
                m_outputfile_detail_total.flush();
            }
        }
    }

    if (global_settings_failure == true)
        m_abort = true;

    int thread_state = 1;
    bool parse_failure = false;

    if (m_abort == false)
    {
        LogMessage(LOG_INFO, "Test parameters:");
        LogMessage(LOG_INFO, "\t- " + TEST_SOURCE_MEMBER.name           + ": " + TC_Cfg.test_source);
        LogMessage(LOG_INFO, "\t- " + ERROR_INSERTION_MEMBER.name       + ": " + BoolToStr(TC_Cfg.error_insertion));
        LogMessage(LOG_INFO, "\t- " + LO_THRESH_ALT_WR_BW_MEMBER.name   + ": " + std::to_string(TC_Cfg.cu_bw.alt_wr_rd.write.low));
        LogMessage(LOG_INFO, "\t- " + HI_THRESH_ALT_WR_BW_MEMBER.name   + ": " + std::to_string(TC_Cfg.cu_bw.alt_wr_rd.write.high));
        LogMessage(LOG_INFO, "\t- " + LO_THRESH_ALT_RD_BW_MEMBER.name   + ": " + std::to_string(TC_Cfg.cu_bw.alt_wr_rd.read.low));
        LogMessage(LOG_INFO, "\t- " + HI_THRESH_ALT_RD_BW_MEMBER.name   + ": " + std::to_string(TC_Cfg.cu_bw.alt_wr_rd.read.high));
        LogMessage(LOG_INFO, "\t- " + LO_THRESH_ONLY_WR_BW_MEMBER.name  + ": " + std::to_string(TC_Cfg.cu_bw.only_wr.write.low));
        LogMessage(LOG_INFO, "\t- " + HI_THRESH_ONLY_WR_BW_MEMBER.name  + ": " + std::to_string(TC_Cfg.cu_bw.only_wr.write.high));
        LogMessage(LOG_INFO, "\t- " + LO_THRESH_ONLY_RD_BW_MEMBER.name  + ": " + std::to_string(TC_Cfg.cu_bw.only_rd.read.low));
        LogMessage(LOG_INFO, "\t- " + HI_THRESH_ONLY_RD_BW_MEMBER.name  + ": " + std::to_string(TC_Cfg.cu_bw.only_rd.read.high));
        LogMessage(LOG_INFO, "\t- " + CHECK_BW_MEMBER.name              + ": " + BoolToStr(TC_Cfg.check_bw));

        LogMessage(LOG_INFO, "Start checking test sequence parameters" );
        std::list<TestItConfig_t> test_it_list;
        parse_failure = ParseTestSequenceSettings(TC_Cfg, &test_it_list);

        if (m_abort == false)
        {
            if (parse_failure == false)
            {
                LogMessage(LOG_PASS, "Checking test parameters finished");
            }
            else
            {
                LogMessage(LOG_FAILURE, "Some test parameters are not valid, check error messages above" );
                m_abort = true;
            }
        }

        if (m_abort == false)
        {
            m_abort = StartTestAndEnableWatchdog();
            if (m_abort == false)
            {
                // run thread async, block & wait for completion
                m_thread_future = std::async(std::launch::async, &MemoryTest::RunThread, this, TC_Cfg, &test_it_list);
                m_thread_future.wait();
                // check on completion if it has been aborted
                thread_state = m_thread_future.get();
                StopTestAndDisableWatchdog();
            }
        }
    }

    if ((thread_state < 0) || (m_abort == true))
    {
        LogMessage(LOG_FAILURE, "Aborted");
        m_result = TestResult::TR_ABORTED;
    }
    else if (thread_state > 0)
    {
        m_result = TestResult::TR_FAILED;
    }
    else
    {
        m_result = TestResult::TR_PASSED;
    }

    return;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool MemoryTest::GetErrorInsertionConfig( MemoryTestcaseCfg_t TC_Cfg, TestItConfig_t *test_it )
{
    test_it->test_mode           = CTRL_TEST_MODE_ALTERNATE_WR_RD_TEST_VAL;

    // Use minimal memory kernel configuration as read burst duration needs to be < 1ms (1 error is inserted every 1ms).
    test_it->wr_start_addr  = m_min_ctrl_addr;
    test_it->wr_burst_size  = m_min_burst_size;
    test_it->wr_num_xfer    = m_min_ctrl_num_xfer;

    test_it->rd_start_addr  = m_min_ctrl_addr;
    test_it->rd_burst_size  = m_min_burst_size;
    test_it->rd_num_xfer    = m_min_ctrl_num_xfer;


    GetBWRange(TC_Cfg, test_it);
    if (test_it->thresh_wr_rd.read.low <= 0.0)
    {
        LogMessage(LOG_FAILURE, "Failed to compute Error Insertion test duration as Read BW Low threshold <= 0, Check BW thresholds");
        return true;
    }
    if (test_it->thresh_wr_rd.write.low <= 0.0)
    {
        LogMessage(LOG_FAILURE, "Failed to compute Error Insertion test duration as Write BW Low threshold <= 0, Check BW thresholds");
        return true;
    }
    // Compute duration between error insertion and detection depending on num_xfer and BW
    double double_duration = 0;
    double_duration  += (double)(test_it->wr_num_xfer)*64.0/(test_it->thresh_wr_rd.write.low*1024.0*1024.0);
    double_duration  += (double)(test_it->rd_num_xfer)*64.0/(test_it->thresh_wr_rd.read.low*1024.0*1024.0);
    test_it->duration = (uint)double_duration;
    // Round up
    if (double_duration - (double)test_it->duration >= 0.5) test_it->duration++;
    // Minimum duration 1s
    if (test_it->duration <= 0) test_it->duration = 1;

    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MemoryTest::PostTeardown()
{
    LogMessage(LOG_STATUS, "PostTeardown");
    m_state = TestState::TS_POST_TEARDOWN;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MemoryTest::Abort()
{
    if (m_abort == false)
    {
        LogMessage(LOG_INFO, "Abort received");
        m_abort = true;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool MemoryTest::CheckResult( LogLevel log_level_pass, LogLevel log_level_fail, std::string direction, MemoryTestcaseCfg_t TC_Cfg, TestItConfig_t test_it, int kernel_core_idx, Meas meas_bw)
{
    bool test_it_failure = false;

    std::string msg_bw  = "";
    Xbtest_Pfm_Def_Mem_Thresh_WrRd_t thresh_wr_rd;

    if (kernel_core_idx == -1) // Set to -1 when the total is computed
    {
        std::string mem_type_str;
        if      (m_kernel_type == TEST_MEMORY_DDR) mem_type_str = "DDR";
        else if (m_kernel_type == TEST_MEMORY_HBM) mem_type_str = "HBM";

        msg_bw = mem_type_str + " (" + std::to_string(m_num_kernel_core) + " channel(s) sum) ";

        // BW total is the sum of BW for each channel
        thresh_wr_rd.write.low    = test_it.thresh_wr_rd.write.low*(m_num_kernel_core);
        thresh_wr_rd.write.high   = test_it.thresh_wr_rd.write.high*(m_num_kernel_core);
        thresh_wr_rd.read.low     = test_it.thresh_wr_rd.read.low*(m_num_kernel_core);
        thresh_wr_rd.read.high    = test_it.thresh_wr_rd.read.high*(m_num_kernel_core);
    }
    else
    {
        msg_bw  = GetMemKernelTag(kernel_core_idx);
        thresh_wr_rd  = test_it.thresh_wr_rd;
    }

    Xbtest_Pfm_Def_Mem_Thresh_HiLo_t thresh;
    uint burst_size;
    uint max_burst_size;
    uint num_xfer;
    uint max_num_xfer;

    std::string dir_0;

    if (StrMatchNoCase(direction, "Read"))
    {
        thresh          = thresh_wr_rd.read;
        burst_size      = test_it.rd_burst_size;
        max_burst_size  = m_max_burst_size;
        num_xfer        = test_it.rd_num_xfer;
        max_num_xfer    = m_max_ctrl_num_xfer;
        dir_0           += " <- ";
        direction       += " ";
    }
    else if (StrMatchNoCase(direction, "Write"))
    {
        thresh          = thresh_wr_rd.write;
        burst_size      = test_it.wr_burst_size;
        max_burst_size  = m_max_burst_size;
        num_xfer        = test_it.wr_num_xfer;
        max_num_xfer    = m_max_ctrl_num_xfer;
        dir_0           += " -> ";
    }

    msg_bw = "FPGA" + dir_0 + msg_bw  + " Average " + direction + " Bandwidth: ";

    // Check BW value only if the full range of memory is tested, and if the test lasted more than 20s
    if ((TC_Cfg.check_bw == true) && ((burst_size == max_burst_size) && (num_xfer == max_num_xfer) && (test_it.duration >= 20)))
        test_it_failure |= CheckBWInRange(log_level_pass, log_level_fail, meas_bw.average, thresh.low, thresh.high, msg_bw);
    else
        LogMessage(LOG_INFO, msg_bw + std::to_string(meas_bw.average) + " MBps");

    return test_it_failure;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::string MemoryTest::TestModeEnumToString( uint type )
{
    std::string type_str = "";
    switch(type)
    {
        case CTRL_TEST_MODE_ALTERNATE_WR_RD_TEST_VAL:    type_str = MEM_CTRL_TEST_MODE_ALTERNATE_WR_RD_TEST;  break;
        case CTRL_TEST_MODE_ONLY_WR_TEST_VAL:            type_str = MEM_CTRL_TEST_MODE_ONLY_WR_TEST;          break;
        case CTRL_TEST_MODE_ONLY_RD_TEST_VAL:            type_str = MEM_CTRL_TEST_MODE_ONLY_RD_TEST;          break;
        case CTRL_TEST_MODE_STOP_TEST_VAL:               type_str = MEM_CTRL_TEST_MODE_STOP_TEST;             break;
        default:                                         type_str = "UNKNOWN";                           break;
    }
    return  type_str;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint MemoryTest::TestModeStringToEnum( std::string type )
{
    uint type_val = CTRL_TEST_MODE_UNKNOWN_VAL;
    if      (StrMatchNoCase(type, MEM_CTRL_TEST_MODE_ALTERNATE_WR_RD_TEST) == true)    type_val = CTRL_TEST_MODE_ALTERNATE_WR_RD_TEST_VAL;
    else if (StrMatchNoCase(type, MEM_CTRL_TEST_MODE_ONLY_WR_TEST) == true)            type_val = CTRL_TEST_MODE_ONLY_WR_TEST_VAL;
    else if (StrMatchNoCase(type, MEM_CTRL_TEST_MODE_ONLY_RD_TEST) == true)            type_val = CTRL_TEST_MODE_ONLY_RD_TEST_VAL;
    else if (StrMatchNoCase(type, MEM_CTRL_TEST_MODE_STOP_TEST) == true)               type_val = CTRL_TEST_MODE_STOP_TEST_VAL;
    return  type_val;
}
