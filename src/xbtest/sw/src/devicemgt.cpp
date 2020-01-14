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

#include "devicemgt.h"
#include <thread>

DeviceMgt::DeviceMgt( Xbtest_Pfm_Def_t xbtest_pfm_def, DeviceInterface *device, Testcase_Parameters_t test_parameters, Global_Config_t global_config )
{
	m_state = TestState::TS_NOT_SET;
	m_result = TestResult::TR_PASSED;

	m_log = Logging::getInstance();
    m_log_msg_test_type = "DEVICE_MGT : ";
    m_abort = false;

    m_xbtest_pfm_def = xbtest_pfm_def;
    m_device = device;
    m_test_parameters = test_parameters;
    m_global_config = global_config;

    m_measure_valid = false;
    for (uint i=0; i<MAX_POWER_SOURCES; i++)
    {
        m_meas_num_nochange.current[i] = 0;
        m_meas_num_nochange.voltage[i] = 0;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DeviceMgt::~DeviceMgt() { s_instance_flag = false; }

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DeviceMgt::PreSetup()
{
    bool ret = true;
    LogMessage(LOG_STATUS, "PreSetup");
    m_state = TestState::TS_PRE_SETUP;
    return ret;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DeviceMgt::PostTeardown()
{
    LogMessage(LOG_STATUS, "PostTeardown");
    m_state = TestState::TS_POST_TEARDOWN;

    m_outputfile.flush();
    m_outputfile.close();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DeviceMgt::Abort()
{
    if (m_abort == false)
    {
        LogMessage(LOG_INFO, "Stop received");
        m_abort = true;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T> bool DeviceMgt::CheckMeasurementChange(std::string name, T inst, T last, unsigned *num_nochange)
{
    if (inst == last)
        (*num_nochange)++;
    else
        (*num_nochange) = 0;

    if ((*num_nochange) >= m_num_sample_nochange)
    {
        LogMessage(LOG_DEBUG, "CheckThread: Measurement of " + std::to_string((*num_nochange)) + " consecutive sample(s) without change: " + name + " = " + std::to_string(inst));
        // Do not generate any abort
        // return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DeviceMgt::CheckAllMeasurementsChange()
{
    bool ret_failure = false;

    for (uint i=0; i<m_xbtest_pfm_def.physical.power.num_power_sources; i++)
    {
        if (m_xbtest_pfm_def.physical.power.power_sources[i].def_by_curr_volt == true)
        {
            ret_failure |= CheckMeasurementChange<unsigned>(
                StrVectToStr(m_xbtest_pfm_def.physical.power.power_sources[i].name_current, "."),
                m_instant_meas.current[i],
                m_instant_meas_last.current[i],
                &(m_meas_num_nochange.current[i]));
            ret_failure |= CheckMeasurementChange<unsigned>(
                StrVectToStr(m_xbtest_pfm_def.physical.power.power_sources[i].name_voltage, "."),
                m_instant_meas.voltage[i],
                m_instant_meas_last.voltage[i],
                &(m_meas_num_nochange.voltage[i]));
        }
    }
    m_instant_meas_last = m_instant_meas;
    return ret_failure;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DeviceMgt::CheckMeasurements()
{

    for (uint i=0; i<m_xbtest_pfm_def.physical.thermal.num_temp_sources; i++)
    {
        if (m_xbtest_pfm_def.physical.thermal.temp_sources[i].limit > -1)
        {
            if (m_instant_meas.temperature[i] > (uint)m_xbtest_pfm_def.physical.thermal.temp_sources[i].limit)
            {
                LogMessage(LOG_ERROR, "CheckThread: Temperature source " + std::to_string(i) + ": operational temperature greater than defined limit: " + std::to_string(m_instant_meas.temperature[i]) + " deg C > " + std::to_string(m_xbtest_pfm_def.physical.thermal.temp_sources[i].limit) + " deg C");
                return true;
            }
        }
    }

    for (uint i=0; i<m_xbtest_pfm_def.physical.power.num_power_sources; i++)
    {
        if (m_xbtest_pfm_def.physical.power.power_sources[i].limit > -1)
        {
            if (m_instant_meas.power[i] > (double)m_xbtest_pfm_def.physical.power.power_sources[i].limit)
            {
                LogMessage(LOG_ERROR, "CheckThread: Power source " + std::to_string(i) + ": operational power greater than defined limit: " + std::to_string(m_instant_meas.power[i]) + " W > " + std::to_string(m_xbtest_pfm_def.physical.power.power_sources[i].limit) + " W");
                return true;
            }
        }
        if (m_instant_meas.power[i] < MIN_OPER_POWER)
        {
            LogMessage(LOG_ERROR, "CheckThread: Power source " + std::to_string(i) + ": operational power lower than defined limit: " + std::to_string(m_instant_meas.power[i]) + " W < " + std::to_string(MIN_OPER_POWER) + " W");
            return true;
        }
    }

    if (CheckAllMeasurementsChange())
        return true;

    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int DeviceMgt::RunThread()
{
	int ret = 0;
    int ticks_1s_error_cnt = 0;

    LogMessage(LOG_INFO, "Start Measurement thread");

    while ((m_abort == false) && (m_overall_task_failure == false))
    {
        WaitFor1sTick();
        auto begin_1s = std::chrono::steady_clock::now();

        if ((m_abort == false) && (m_overall_task_failure == false)) m_overall_task_failure |= m_device->GetDeviceInfo(&m_instant_meas);
        if ((m_abort == false) && (m_overall_task_failure == false)) m_overall_task_failure |= CheckMeasurements();
        if ((m_abort == false) && (m_overall_task_failure == false)) m_measure_valid = true;

        if ((m_abort == false) && (m_overall_task_failure == false))
        {
            auto end_1s    = std::chrono::steady_clock::now();
            auto ticks_1s  = std::chrono::duration_cast<std::chrono::microseconds>(end_1s-begin_1s);
            if (ticks_1s.count() > 1000000)
            {
                LogMessage(LOG_DEBUG, "Measurements took more time than 1 second: " + std::to_string(ticks_1s.count()) + "us > 1000000 us");
                ticks_1s_error_cnt ++;
            }
            else
            {
                ticks_1s_error_cnt = 0;
            }

            if (ticks_1s_error_cnt >= 5)
                LogMessage(LOG_CRIT_WARN, "Measurements took more time than 1 second " + std::to_string(ticks_1s_error_cnt) + " times consecutively");
        }

    }

    // As this test is run as background task, the abort signal is not considered as a failure
    // An internal task failure is considered as an abort, so that all other tests will be aborted as a consequence
    if ((m_overall_task_failure == false) && (m_abort == true))
    {
        ret = 0;
    }
    else if (m_overall_task_failure == true)
    {
        ret = -1;
    }
    else
    {
        ret = 1;
    }

    return ret;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DeviceMgt::Run1sTick()
{
    LogMessage(LOG_INFO, "Start 1s tick function");
    while ((m_abort == false) && (m_overall_task_failure == false))
    {
        SignalNewSecondTick();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DeviceMgt::RunMeasFile()
{
    LogMessage(LOG_INFO, "Start measurement logging");
    while ((m_abort == false) && (m_overall_task_failure == false))
    {
        WaitFor1sTick();
        if (m_measure_valid == true)
        {
            WriteToMeasurementFile(GetPowerTestMeas());
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DeviceInterface::Device_Info DeviceMgt::GetPowerTestMeas()
{
    return m_instant_meas;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DeviceMgt::Run()
{
    Json_Parameters_t::iterator it;

	m_state     = TestState::TS_RUNNING; // update progress
    m_result    = TestResult::TR_FAILED;

	LogMessage(LOG_STATUS, "Run");
    // parse testcase parameters
    bool global_settings_failure = false;

    ///////////////////////////////////////////////////////////////////////
    // Get verbosity if defined in JSON, else use top level one
    int verbosity = (int)m_global_config.verbosity;
    bool verbos_ret = GetVerbosity(&(m_test_parameters.param), &verbosity);
    if (verbos_ret == true)
        LogMessage(LOG_FAILURE, VERBOSITY_FAILURE);
    global_settings_failure |= verbos_ret;
    m_global_config.verbosity = static_cast<LogLevel>(verbosity);

    // measurement output file
    it = FindJsonParam(&(m_test_parameters.param), OUTPUT_FILE_MEMBER);
    if (it != m_test_parameters.param.end())
    {
        m_outputfile_name = TestcaseParamCast<std::string>(it->second);
        m_use_outputfile  = true;

        global_settings_failure |= OpenOutputFile(m_outputfile_name + ".csv", &m_outputfile );
        m_outputfile << "Fan RPM" << ",";
        for (uint i=0; i<m_xbtest_pfm_def.physical.thermal.num_temp_sources; i++)
        {
            m_outputfile << m_xbtest_pfm_def.physical.thermal.temp_sources[i].source_name << ",";
        }
        for (uint i=0; i<m_xbtest_pfm_def.physical.power.num_power_sources; i++)
        {
            if (m_xbtest_pfm_def.physical.power.power_sources[i].def_by_curr_volt == true)
            {
                m_outputfile << m_xbtest_pfm_def.physical.power.power_sources[i].source_name_current << ",";
                m_outputfile << m_xbtest_pfm_def.physical.power.power_sources[i].source_name_voltage << ",";
            }
            m_outputfile << m_xbtest_pfm_def.physical.power.power_sources[i].source_name << ",";
        }
        m_outputfile << "Total power" << ",";
        m_outputfile << "\n";
        m_outputfile.flush();
    }

    m_num_sample_nochange = NUM_SEC_CHANGE;

    // spawn a thread and wait for up to a period of time for completion
    int thread_state = 1;
    if ((global_settings_failure == false) && (m_abort == false))
    {
        m_start_check_thread = false;
        m_overall_task_failure = false;

        //std::async(std::launch::async, &DeviceMgt::Run1sTick, this);
        std::thread thread1sTick( &DeviceMgt::Run1sTick, this);
        std::thread threadMeasFile;
        if (m_use_outputfile == true)
        {
            threadMeasFile = std::thread(&DeviceMgt::RunMeasFile, this);
        }

        // run thread async, block & wait for completion
        m_thread_future  = std::async(std::launch::async, &DeviceMgt::RunThread, this);
        m_thread_future.wait();
        // check on completion if it has been aborted
        thread_state = m_thread_future.get();
        thread1sTick.join();
        if (m_use_outputfile == true)
        {
            threadMeasFile.join();
        }
    }

    if ( (thread_state < 0) || (global_settings_failure == true) )
    {
        LogMessage(LOG_ERROR, "Some measurement tests failed");
        LogMessage(LOG_FAILURE, "Aborted");
        m_result = TestResult::TR_ABORTED;
    }
    else if (thread_state > 0)
    {
        LogMessage(LOG_ERROR, "Unexpected end of measurement tests");
        m_result = TestResult::TR_FAILED;
    }
    else
    {
        LogMessage(LOG_PASS, "All measurement tests passed");
        m_result = TestResult::TR_PASSED;
    }

    return;
}

void DeviceMgt::WaitFor1sTick()
{
    std::unique_lock<std::mutex> lk(m_Mutex);
    if (m_CV.wait_for(lk,std::chrono::milliseconds(1100)) == std::cv_status::timeout)
    {
        if (m_abort == false)
        {
            LogMessage(LOG_CRIT_WARN, "Measurements alignment timed out (1.1 seconds)");
        }
    }
}

void DeviceMgt::SignalNewSecondTick()
{
    m_CV.notify_all();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DeviceMgt::WriteToMeasurementFile( DeviceInterface::Device_Info device_info )
{
    if (m_use_outputfile == true)
    {
        m_outputfile << std::to_string(device_info.mFanRpm) << ",";

        for (uint i=0; i<m_xbtest_pfm_def.physical.thermal.num_temp_sources; i++)
        {
            m_outputfile << std::to_string(device_info.temperature[i]) << ",";
        }
        for (uint i=0; i<m_xbtest_pfm_def.physical.power.num_power_sources; i++)
        {
            if (m_xbtest_pfm_def.physical.power.power_sources[i].def_by_curr_volt == true)
            {
                m_outputfile << std::to_string(device_info.current[i]) << ",";
                m_outputfile << std::to_string(device_info.voltage[i]) << ",";
            }
            m_outputfile << std::to_string(device_info.power[i]) << ",";
        }
        m_outputfile
            << std::to_string(device_info.Power_mW/1000.0) << ","
            << "\n";
        m_outputfile.flush();
    }
}
