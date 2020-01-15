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

#include "testcase.h"

TestCase::TestCase( TestCaseType testcase_type, TestType type, TestInterface *base, Global_Config_t global_config )
{
    m_testcase_type = testcase_type;
    m_type          = type;
    m_base          = base;
    m_complete      = false;
    m_global_config = global_config;

    m_log = Logging::getInstance();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TestCase::~TestCase() {}

void TestCase::SignalAbortTest()    { m_base->Abort(); } //signal the atomic abort
void TestCase::SetTestComplete()    { m_complete = true; }
bool TestCase::GetTestComplete()    { return m_complete; }

bool TestCase::CheckTestAborted()   { return (m_base->GetResult() == TestInterface::TR_ABORTED); }

TestCaseType TestCase::GetTestCaseType()    { return m_testcase_type; }
TestType TestCase::GetTestType()            { return m_type; }

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TestCase::TestCaseThreadResult TestCase::SpawnTest()
{
    TestCaseThreadResult ret = TC_FAIL;
    if (m_base != nullptr && m_log != nullptr)
    {
        LogMessage(LOG_INFO, TestTypeToString(m_type) + " Starting thread...");
        if (m_base->PreSetup() == true) // pre-test configuration
        {
            m_base->Run(); // run tests - block until complete or abort/fails
            m_base->PrintResult(); // get the state after tests run
            switch(m_base->GetResult())
            {
                default:                        ret = TC_FAIL;      break;
                case TestInterface::TR_PASSED:  ret = TC_PASS;      break;
                case TestInterface::TR_ABORTED: ret = TC_ABORTED;   break;
            }
            m_base->PostTeardown(); // tear down required even on failure
        }
        else
        {
            LogMessage(LOG_ERROR, TestTypeToString(m_type) + " PreSetup Failed!");
        }
        LogMessage(LOG_INFO, TestTypeToString(m_type) + " Exit thread...");
    }
    return ret;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TestCase::LogMessage ( LogLevel Level, std::string msg ) { m_log->LogMessage(Level, msg, m_global_config.verbosity); }
