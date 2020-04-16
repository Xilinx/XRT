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

#ifndef _TESTCASE_H
#define _TESTCASE_H

#include "xbtestcommon.h"
#include "logging.h"
#include "deviceinterface.h"
#include "testinterface.h"

class TestCase
{

private:

    TestCaseType m_testcase_type;
    TestType m_type;
    TestInterface *m_base;
    bool m_complete;
    Global_Config_t m_global_config;
    Logging *m_log;

public:

    enum TestCaseThreadResult { TC_FAIL, TC_PASS, TC_ABORTED };
    std::future<TestCaseThreadResult> future_result;

    TestCase( TestCaseType testcase_type, TestType type, TestInterface *base, Global_Config_t global_config );
    ~TestCase();
    void SignalAbortTest();
    void SetTestComplete();
    bool GetTestComplete();
    bool CheckTestAborted();
    TestCaseType GetTestCaseType();
    TestType GetTestType();
    TestCaseThreadResult SpawnTest();

    void LogMessage ( LogLevel Level, std::string msg );

};

#endif /* _TESTCASE_H */
