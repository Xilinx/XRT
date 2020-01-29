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


#ifndef _DEVICEMGT_H
#define _DEVICEMGT_H

#include "xcl2.h"
#include "xbtestcommon.h"
#include "logging.h"
#include "deviceinterface.h"
#include "testinterface.h"

#include <condition_variable>

class DeviceMgt : public TestInterface
{

public:

	DeviceMgt( Xbtest_Pfm_Def_t xbtest_pfm_def, DeviceInterface *device, Testcase_Parameters_t test_parameters, Global_Config_t global_config );
	~DeviceMgt();

	// implement virtual inherited functions
	bool PreSetup();
	void Run();
    void PostTeardown();
    void Abort();

    void WaitFor1sTick();
    DeviceInterface::Device_Info GetPowerTestMeas();

    template<typename T> bool CheckMeasurementChange(std::string name, T inst, T last, unsigned *num_nochange);
    bool CheckAllMeasurementsChange();

    bool CheckMeasurements();

private:

    Xbtest_Pfm_Def_t m_xbtest_pfm_def;

    std::atomic<bool> m_abort;
	std::future<int> m_check_thread_future;
	std::future<int> m_thread_future;
    std::future<void> m_thread_1s;
    DeviceInterface *m_device;

    std::string m_outputfile_name;
    bool m_use_outputfile       = false;
    std::ofstream m_outputfile;

	int RunCheckThread();
	int RunMeasureThread();
    int m_runthread = 0;
	int RunThread();
    void Run1sTick();
    void RunMeasFile();
    void WriteToMeasurementFile( DeviceInterface::Device_Info device_info );

    bool m_start_check_thread;
    bool m_overall_task_failure;
    bool m_measure_valid;

    // currently CMC reads (via the SC) sensor values every 100ms
    static const uint MIN_SAMPLING = 1;
    static const uint NOM_SAMPLING = 1;
    static const uint MAX_SAMPLING = 10;


    const float MIN_OPER_POWER = 0.0;

    static const uint NUM_SEC_CHANGE = 7;

    bool s_instance_flag = false;

    uint m_num_sample_nochange;

    typedef struct
    {
        unsigned current[MAX_POWER_SOURCES];
        unsigned voltage[MAX_POWER_SOURCES];
    } Meas_Num_NoChange;

    Meas_Num_NoChange m_meas_num_nochange;

    DeviceInterface::Device_Info m_instant_meas;
    DeviceInterface::Device_Info m_instant_meas_last;

    void SignalNewSecondTick(void);
    std::mutex m_Mutex;
    std::condition_variable m_CV;

};

#endif /* _DEVICEMGT_H */
