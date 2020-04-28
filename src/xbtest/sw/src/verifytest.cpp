
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

#include "verifytest.h"

VerifyTest::VerifyTest( DeviceInterface *device, Global_Config_t global_config )
{
    m_state     = TestState::TS_NOT_SET;
    m_result    = TestResult::TR_PASSED;

    m_log = Logging::getInstance();
    m_log_msg_test_type = "VERIFY     : ";
    m_abort = false;

    m_device = device;
    m_global_config = global_config;
}

VerifyTest::~VerifyTest () {}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool VerifyTest::PreSetup()
{
    bool ret = true;
    LogMessage(LOG_INFO, "PreSetup");
    m_state = TestState::TS_PRE_SETUP;
    return ret;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VerifyTest::PostTeardown()
{
    LogMessage(LOG_STATUS, "PostTeardown");
    m_state = TestState::TS_POST_TEARDOWN;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VerifyTest::Abort()
{
    LogMessage(LOG_INFO, "Signal abort");
    m_abort = true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VerifyTest::Run()
{
    m_state  = TestState::TS_RUNNING;
    m_result = TestResult::TR_FAILED;
    LogMessage(LOG_STATUS, "Run");

    int thread_state = 1;

    if (m_abort == false)
    {
        // run thread async, block & wait for completion
        m_thread_future = std::async(std::launch::async, &VerifyTest::RunThread, this);
        m_thread_future.wait();
        // check on completion if it has been aborted
        thread_state = m_thread_future.get();
    }


    if ((thread_state < 0) || (m_abort == true))
    {
        LogMessage(LOG_FAILURE, "Aborted");
        m_result = TestResult::TR_ABORTED;
    }
    else if(thread_state > 0)
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

bool VerifyTest::VerifyKernelBI( DeviceInterface::Build_Info krnl_bi, int kernel_type, int kernel_idx, int *verify_pass_cnt, int *verify_fail_cnt, int kernel_core_idx)
{
    bool ret_failure = false;

    if (kernel_type == KRNL_TYPE_PWR)
        m_device->PrintPwrKrnlBI(krnl_bi);
    else if (kernel_type == KRNL_TYPE_MEM_DDR)
        m_device->PrintMemDDRKrnlBI(krnl_bi, kernel_core_idx);
    else if (kernel_type == KRNL_TYPE_MEM_HBM)
        m_device->PrintMemHBMKrnlBI(krnl_bi, kernel_core_idx);
    else if (kernel_type == KRNL_TYPE_GT)
        m_device->PrintGTKrnlBI(krnl_bi);
    else if (kernel_type == KRNL_TYPE_GT_MAC)
        m_device->PrintGTKrnlBI(krnl_bi);

    int major_version = 0;
    int minor_version = 0;
    int component_id = 0;

    if (kernel_type == KRNL_TYPE_PWR)
    {
        major_version   = BI_PWR_HW_VERSION_MAJOR;
        minor_version   = BI_PWR_HW_VERSION_MINOR;
        component_id    = BI_PWR_HW_COMPONENT_ID;
    }
    else if ((kernel_type == KRNL_TYPE_MEM_DDR) || (kernel_type == KRNL_TYPE_MEM_HBM))
    {
        major_version   = BI_MEM_HW_VERSION_MAJOR;
        minor_version   = BI_MEM_HW_VERSION_MINOR;
        component_id    = BI_MEM_HW_COMPONENT_ID;
    }
    else if (kernel_type == KRNL_TYPE_GT)
    {
        major_version   = BI_GT_HW_VERSION_MAJOR;
        minor_version   = BI_GT_HW_VERSION_MINOR;
        component_id    = -1;
    }
    else if (kernel_type == KRNL_TYPE_GT_MAC)
    {
        major_version   = BI_GT_MAC_HW_VERSION_MAJOR;
        minor_version   = BI_GT_MAC_HW_VERSION_MINOR;
        component_id    = BI_GT_MAC_HW_COMPONENT_ID;
    }

    ret_failure |= VerifyBIValue<int>(krnl_bi, "Major version",     krnl_bi.major_version,      major_version,                  verify_pass_cnt, verify_fail_cnt);
    ret_failure |= VerifyBIValue<int>(krnl_bi, "Minor version",     krnl_bi.minor_version,      minor_version,                  verify_pass_cnt, verify_fail_cnt);

    if (krnl_bi.perforce_version != SW_PERFORCE_VERSION)
        LogMessage(LOG_INFO, "Build info " + krnl_bi.kernel_name + ". HW Build: " + std::to_string(krnl_bi.perforce_version) + " (SW Build: " + std::to_string(SW_PERFORCE_VERSION) + ")");

    if (kernel_type == KRNL_TYPE_GT)
    {
        // there are 2 possible component ID for GT kernel
        if (krnl_bi.component_id != 3 && krnl_bi.component_id != 2)
        {
            LogMessage(LOG_ERROR, "Build info " + krnl_bi.kernel_name + ". Component ID read: " + std::to_string(krnl_bi.component_id )+ ", expected 2 or 3");
            ret_failure = true;
            (*verify_fail_cnt)++;
        }
        else
        {
            (*verify_pass_cnt)++;
        }

    }
    else
    {
        ret_failure |= VerifyBIValue<int>(krnl_bi, "Component ID",  krnl_bi.component_id,       component_id,                   verify_pass_cnt, verify_fail_cnt);
    }

    if (kernel_type == KRNL_TYPE_PWR)
    {
        int power_kernel_slr = m_device->GetPowerKernelSLR( kernel_idx );
        ret_failure |= VerifyBIValue<int>(krnl_bi, "SLR", krnl_bi.slr, power_kernel_slr, verify_pass_cnt, verify_fail_cnt);
    }
    else if ((kernel_type == KRNL_TYPE_MEM_DDR) || (kernel_type == KRNL_TYPE_MEM_HBM))
    {
        int mem_kernel_num_core;
        int mem_kernel_num_mem;
        int mem_kernel_inst;

        if (kernel_type == KRNL_TYPE_MEM_DDR)
        {
            mem_kernel_num_core = m_device->GetMemKernelDDRNumCore(kernel_idx);
            mem_kernel_num_mem  = m_device->GetMemKernelDDRNumMem(kernel_idx);
            mem_kernel_inst     = m_device->GetMemKernelDDRInst(kernel_idx);
        }
        else if (kernel_type == KRNL_TYPE_MEM_HBM)
        {
            mem_kernel_num_core = m_device->GetMemKernelHBMNumCore(kernel_idx);
            mem_kernel_num_mem  = m_device->GetMemKernelHBMNumMem(kernel_idx);
            mem_kernel_inst     = m_device->GetMemKernelHBMInst(kernel_idx);
        }

        ret_failure |= VerifyBIValue<int>(krnl_bi, "Number of cores",       krnl_bi.mem_kernel_num_core,    mem_kernel_num_core,    verify_pass_cnt, verify_fail_cnt);
        ret_failure |= VerifyBIValue<int>(krnl_bi, "Number of channel",     krnl_bi.mem_kernel_num_mem,     mem_kernel_num_mem,     verify_pass_cnt, verify_fail_cnt);
        ret_failure |= VerifyBIValue<int>(krnl_bi, "Compute unit instance", krnl_bi.mem_kernel_inst,        mem_kernel_inst,        verify_pass_cnt, verify_fail_cnt);

        if (kernel_core_idx != KERNEL_CORE_IDX_UNUSED) // Check this only for memory kernel core
        {
            int bi_kernel_core_idx = kernel_core_idx + 1; // corresponds to <idx> in axi interface name m<idx>_axi
            ret_failure |= VerifyBIValue<int>(krnl_bi, "Compute unit channel",   krnl_bi.mem_kernel_core_idx,    bi_kernel_core_idx, verify_pass_cnt, verify_fail_cnt);

            if (kernel_type == KRNL_TYPE_MEM_DDR)
            {
                ret_failure |= VerifyBIValue<int>(krnl_bi, "Channel destination index", krnl_bi.mem_kernel_dst_idx,     mem_kernel_inst,            verify_pass_cnt, verify_fail_cnt); // In DDR mode, m01_axi dest corresponds to instance idx
                ret_failure |= VerifyBIValue<int>(krnl_bi, "Channel destination type",  krnl_bi.mem_kernel_dst_type,    BI_MEM_KERNEL_DST_TYPE_DDR, verify_pass_cnt, verify_fail_cnt);
            }
            else if (kernel_type == KRNL_TYPE_MEM_HBM)
            {
                ret_failure |= VerifyBIValue<int>(krnl_bi, "Channel destination type",  krnl_bi.mem_kernel_dst_type,    BI_MEM_KERNEL_DST_TYPE_HBM, verify_pass_cnt, verify_fail_cnt);
            }
        }
    }

    // Scratch pad test
    // Check default value of scratch pad
    if ((krnl_bi.scratch_pad != (uint)(0xFFFF0000)) && (krnl_bi.scratch_pad != (uint)(0x0000FFFF)))
    {
        LogMessage(LOG_ERROR, "Build info " + krnl_bi.kernel_name + ". Scratch pad read test. Read: " + NumToStrHex<uint>(krnl_bi.scratch_pad) + ", expected: 0xFFFF0000 or 0x0000FFFF");
        ret_failure = true;
        (*verify_fail_cnt)++;
    }
    else (*verify_pass_cnt)++;

    uint scratch_pad_2 = (~krnl_bi.scratch_pad) & 0xFFFFFFFF;                                   // Compute next expected scratch pad
    m_device->WriteKernel(kernel_type, kernel_idx, CMN_SCRATCH_PAD_ADDR, ~(krnl_bi.scratch_pad & 0x1));    // Write to scratch pad
    krnl_bi.scratch_pad = m_device->ReadKernel(kernel_type, kernel_idx, CMN_SCRATCH_PAD_ADDR);             // Read new scratch pad

    // Check expected value
    ret_failure |= VerifyBIValue<uint>(krnl_bi, "Scratch pad write test.", krnl_bi.scratch_pad, scratch_pad_2, verify_pass_cnt, verify_fail_cnt);

    return ret_failure;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int VerifyTest::RunThread()
{
    bool test_failure = false;
    int  overall_verify_pass_cnt = 0;
    int  overall_verify_fail_cnt = 0;

    int num_known_krnls = 0;
    num_known_krnls += m_device->GetNumPowerKernels();
    num_known_krnls += m_device->GetNumMemDDRKernels();
    num_known_krnls += m_device->GetNumMemHBMKernels();
    num_known_krnls += m_device->GetNumGTKernels();
    num_known_krnls += m_device->GetNumGTMACKernels();

    if (num_known_krnls == 0)
    {
        LogMessage(LOG_WARN, "No Build info check performed as no valid kernel detected");
    }
    else
    {
        // create a number of kernels based on the device type
        for (int kernel_type = 0; kernel_type < NUM_KERNEL_TYPE-1 && (m_abort == false); kernel_type++)
        {
            // Check build info
            for (int kernel_idx = 0; kernel_idx < m_device->GetNumKernels(kernel_type) && (m_abort == false); kernel_idx++)
            {
                bool test_it_failure = false;
                int  verify_pass_cnt = 0;
                int  verify_fail_cnt = 0;

                DeviceInterface::Build_Info krnl_bi = m_device->GetKrnlBI(kernel_type, kernel_idx, KERNEL_CORE_IDX_UNUSED);

                test_it_failure |= VerifyKernelBI(krnl_bi, kernel_type, kernel_idx, &verify_pass_cnt, &verify_fail_cnt, KERNEL_CORE_IDX_UNUSED);

                if ((kernel_type == KRNL_TYPE_MEM_DDR) || (kernel_type == KRNL_TYPE_MEM_HBM))
                {
                    int mem_kernel_num_core;
                    if (kernel_type == KRNL_TYPE_MEM_DDR)
                        mem_kernel_num_core = m_device->GetMemKernelDDRNumCore(kernel_idx);
                    else if (kernel_type == KRNL_TYPE_MEM_HBM)
                        mem_kernel_num_core = m_device->GetMemKernelHBMNumCore(kernel_idx);
                    for (int kernel_core_idx = 0; kernel_core_idx < mem_kernel_num_core; kernel_core_idx++)
                    {
                        DeviceInterface::Build_Info kernel_core_bi = m_device->GetKrnlBI(kernel_type, kernel_idx, kernel_core_idx);
                        test_it_failure |= VerifyKernelBI(kernel_core_bi, kernel_type, kernel_idx, &verify_pass_cnt, &verify_fail_cnt, kernel_core_idx);
                    }
                }

                // Summary
                overall_verify_pass_cnt += verify_pass_cnt;
                overall_verify_fail_cnt += verify_fail_cnt;
                LogMessage(LOG_DEBUG,  "Number of pass tests for compute unit " + krnl_bi.kernel_name + ": " + std::to_string(verify_pass_cnt));
                LogMessage(LOG_DEBUG,  "Number of fail tests for compute unit " + krnl_bi.kernel_name + ": " + std::to_string(verify_fail_cnt));
                if (test_it_failure == false)
                    LogMessage(LOG_PASS,  "Test pass for compute unit " + krnl_bi.kernel_name);
                else
                    LogMessage(LOG_ERROR, "Test fail for compute unit " + krnl_bi.kernel_name);

                test_failure |= test_it_failure;
            }
        }

        LogMessage(LOG_DEBUG,  "Total number of pass tests: " + std::to_string(overall_verify_pass_cnt));
        LogMessage(LOG_DEBUG,  "Total number of fail tests: " + std::to_string(overall_verify_fail_cnt));

        if (((overall_verify_pass_cnt + overall_verify_fail_cnt) == 0) and (m_abort == false))
        {
            LogMessage(LOG_ERROR, "No test performed");
            test_failure = true;
        }
    }

    int ret = 0;
    if (m_abort == true)
    {
        ret = -1;
    }
    else if (test_failure == true)
    {
        ret = 1;
        LogMessage(LOG_ERROR,"Test failed");
    }
    else
    {
        LogMessage(LOG_PASS,"Test passed");
    }

    return ret;
}
