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

#ifndef _VERIFYTEST_H
#define _VERIFYTEST_H

#include "xbtestcommon.h"
#include "logging.h"
#include "deviceinterface.h"
#include "testinterface.h"
#include "xbutildumpparser.h"

class VerifyTest: public TestInterface
{

private:
    Xbtest_Pfm_Def_t m_xbtest_pfm_def;

    std::atomic<bool> m_abort;
    std::future<int> m_thread_future;
    DeviceInterface *m_device;

    Logging *m_log = nullptr;

    XbutilDumpParser *m_xbutil_dump_parser;

public:

    VerifyTest( DeviceInterface *device, Global_Config_t global_config );
    ~VerifyTest();

    // implement virtual inherited functions
    bool PreSetup();
    void Run();
    void PostTeardown();
    void Abort();

    int RunThread();

    bool VerifyKernelBI( DeviceInterface::Build_Info krnl_bi, int kernel_type, int kernel_idx, int *verify_pass_cnt, int *verify_fail_cnt, int kernel_core_idx);

    template<typename T>  bool VerifyBIValue( DeviceInterface::Build_Info krnl_bi, std::string param_name, T param_read, T param_expected, int *verify_pass_cnt, int *verify_fail_cnt )
    {
        bool ret_failure = false;
        if (param_read != param_expected)
        {
            LogMessage(LOG_ERROR, "Build info " + krnl_bi.kernel_name + ". " + param_name + " read: " + std::to_string(param_read)+ ", expected: " + std::to_string(param_expected));
            ret_failure = true;
            (*verify_fail_cnt)++;
        }
        else (*verify_pass_cnt)++;
        return ret_failure;
    }

};

#endif /* _VERIFYTEST_H */
