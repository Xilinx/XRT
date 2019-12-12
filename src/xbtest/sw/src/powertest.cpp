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

#include "powertest.h"
#include <queue>
extern std::queue<int> IPC_queue;

PowerTest::PowerTest( Xbtest_Pfm_Def_t xbtest_pfm_def, DeviceInterface *device, DeviceMgt *device_mgt, Testcase_Parameters_t test_parameters, Global_Config_t global_config )
{
    m_state     = TestState::TS_NOT_SET;
    m_result    = TestResult::TR_NOT_SET;

    m_log       = Logging::getInstance();
    m_log_msg_test_type = "POWER_TEST : ";
    m_abort     = false;

    m_xbtest_pfm_def = xbtest_pfm_def;
    m_device = device;
    m_devicemgt = device_mgt;
    m_test_parameters = test_parameters;
    m_global_config = global_config;

    m_num_krnls = m_device->GetNumPowerKernels();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

PowerTest::~PowerTest() {}

bool PowerTest::PreSetup()
{
    bool ret = true;
    LogMessage(LOG_STATUS, "PreSetup");
    m_state = TestState::TS_PRE_SETUP;
    return ret;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PowerTest::PostTeardown()
{
    LogMessage(LOG_STATUS, "PostTeardown");
    m_state = TestState::TS_POST_TEARDOWN;

    m_outputfile.flush();
    m_outputfile.close();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PowerTest::Abort()
{
    if (m_abort == false)
    {
        LogMessage(LOG_INFO, "Abort received");
        m_abort = true;
    }
}


void PowerTest::WaitSecTick(uint quantity)
{
    for (uint i=0; i<quantity && (m_abort == false); i++)
    {
        m_devicemgt->WaitFor1sTick();
        if ( (i % NUM_SEC_WATCHDOG == 0) && (quantity >= NUM_SEC_WATCHDOG) )  ResetWatchdog();
    }
}

void PowerTest::ResetWatchdog()
{
    uint read_data;
    for (int kernel_idx=0; kernel_idx<m_num_krnls; kernel_idx++)
    {
        // if a reset is requested, it also means that the watchdog is enabled
        //  don't read the current value of the CMN_WATCHDOG_ADDR to save access
        read_data = CMN_WATCHDOG_RST | CMN_WATCHDOG_EN;
        m_device->WritePwrKernel(kernel_idx, CMN_WATCHDOG_ADDR,read_data);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int PowerTest::Percent2Throttle( double percent )
{
    int temp = round(percent*((double)QTY_THROTTLE_STEP-1)/100.0);
    if (percent < 0.0)
    {
        LogMessage(LOG_ERROR,"negative toggle rate: " + Float_to_String(percent,1) + ", set it to 0%");
        temp = 0;
    }
    else if (percent > 100.0)
    {
        LogMessage(LOG_ERROR,"toggle rate bigger than 100%: " + Float_to_String(percent,1)+ ", set it to 100%");
        temp = QTY_THROTTLE_STEP-1;

    }
    return temp;
}

double PowerTest::Throttle2Percent( int throttle )
{
    double temp = (double)(100*throttle)/(QTY_THROTTLE_STEP-1);
    if (temp > 100.0) temp = 100.0;
    return temp;
}


bool PowerTest::SetClkThrottle( int throttle, bool force_write)
{
    uint read_data;
    std::string msg_txt = "Watchdog triggered";
    if ( (throttle != m_previous_throttle) || (force_write == true) )
    {
        m_previous_throttle = throttle;
        for (int kernel_idx = 0; kernel_idx < m_num_krnls; kernel_idx++)
        {
            read_data = m_device->ReadPwrKernel(kernel_idx, PWR_CTRL_REG_ADDR);
            //LogMessage(LOG_DEBUG, "kernel " + std::to_string(kernel_idx) + ": cfg register before 0x" + NumToStrHex<unsigned int>(read_data));
            // keep previous content and wipe out the throttle   then insert the new one
            read_data = (read_data & ~PWR_TOG_PAT_MASK) + (throttle & PWR_TOG_PAT_MASK);
            m_device->WritePwrKernel(kernel_idx, PWR_CTRL_REG_ADDR, read_data);
            //read_data = m_device->ReadPwrKernel(kernel_idx, PWR_CTRL_REG_ADDR);
            //LogMessage(LOG_DEBUG, "kernel " + std::to_string(kernel_idx) + ": cfg register after 0x" + NumToStrHex<unsigned int>(read_data));
        }
        return true;
    }
    else
    {
        return false;
    }
}

void PowerTest::StopPowerKernel()
{
    uint read_data;
    for (int kernel_idx=0; kernel_idx<m_num_krnls; kernel_idx++)
    {
        read_data = m_device->ReadPwrKernel(kernel_idx, PWR_CTRL_REG_ADDR);
        // set null throttle, disable everything
        read_data = read_data & ~(PWR_CTRL_ENABLE_REG | PWR_CTRL_ENABLE_DSP | PWR_CTRL_ENABLE_BRAM | PWR_CTRL_ENABLE_URAM | PWR_TOG_PAT_MASK);
        m_device->WritePwrKernel(kernel_idx, PWR_CTRL_REG_ADDR, read_data);
    }
}

void PowerTest::StartPowerKernel(PowerTestcaseCfg_t TC_Cfg)
{
    uint read_data;
    for (int kernel_idx=0; kernel_idx<m_num_krnls; kernel_idx++)
    {
        read_data = m_device->ReadPwrKernel(kernel_idx, PWR_CTRL_REG_ADDR);
        // set null throttle, disable everything
        read_data = read_data & ~(PWR_CTRL_ENABLE_REG | PWR_CTRL_ENABLE_DSP | PWR_CTRL_ENABLE_BRAM | PWR_CTRL_ENABLE_URAM | PWR_TOG_PAT_MASK);

        // enable each type of macro individually
        if (TC_Cfg.mode_enable_REG == true) read_data |= PWR_CTRL_ENABLE_REG;
        if (TC_Cfg.mode_enable_DSP == true) read_data |= PWR_CTRL_ENABLE_DSP;
        if (TC_Cfg.mode_enable_BRAM == true) read_data |= PWR_CTRL_ENABLE_BRAM;
        if (TC_Cfg.mode_enable_URAM == true) read_data |= PWR_CTRL_ENABLE_URAM;

        m_device->WritePwrKernel(kernel_idx, PWR_CTRL_REG_ADDR, read_data);
        //read_data = m_device->ReadPwrKernel(kernel_idx, PWR_CTRL_REG_ADDR);
        //LogMessage(LOG_DESIGNER, "kernel " + std::to_string(kernel_idx) + ": cfg register after 0x" + NumToStrHex<unsigned int>(read_data));
    }
}


bool PowerTest::CheckTime( uint time )
{
    bool ret = false;

    if (time < MIN_DURATION)
    {
        LogMessage(LOG_ERROR, "Duration below the minimum of " + std::to_string(MIN_DURATION) + "s: "  + std::to_string(time) + "s");
        ret = true;
    }
    else if (time > MAX_DURATION)
    {
        LogMessage(LOG_ERROR, "Duration above the maximum of " + std::to_string(MAX_DURATION) + "s: "  + std::to_string(time) + "s");
        ret = true;
    }

    return ret;
}

bool PowerTest::CheckTargetPower( int power )
{
    bool ret = false;

    if (power < m_min_power)
    {
        LogMessage(LOG_ERROR, "Target power below the minimum of " + std::to_string(m_min_power) + "W: " + std::to_string(power) + "W");
        ret = true;
    }
    else if (power > m_max_power)
    {
        LogMessage(LOG_ERROR, "Target power above the maximum of " + std::to_string(m_max_power) + "W: " + std::to_string(power) + "W");
        ret = true;
    }

    return ret;
}


bool PowerTest::CheckToggleRate(int toggle_rate)
{
    bool ret = false;

    if (toggle_rate < MIN_TOG)
    {
        LogMessage(LOG_ERROR, "Toggle rate below the minimum of " + std::to_string(MIN_TOG) + "%: " + std::to_string(toggle_rate) + "%");
        ret = true;
    }
    else if (toggle_rate > MAX_TOG)
    {
        LogMessage(LOG_ERROR, "Toggle rate above the maximum of " + std::to_string(MAX_TOG) + "%: " + std::to_string(toggle_rate) + "%");
        ret = true;
    }

    return ret;
}

double PowerTest::ComputePowerTolerance( int target_power )
{
    // xyz percent of the target power with a minimum of 2 W expressed in milli watts
    double tol = (double)target_power * POWER_TOLERANCE_CRITERIA * 10.0;
    double min_pwr = (double)m_min_power_toreance*1000.0;

    if (tol < min_pwr) return min_pwr;
    else return tol;
}


int PowerTest::CheckPowerRange( int in_range, int out_range )
{
    //check that that the quantity of "in" & "out" is matching with the quantity of sample expected

    int temp = 0;
    if ( (in_range + out_range) != 0)
        temp = ((in_range*100) / (in_range + out_range));

    return temp;
}


void PowerTest::WaitPowerStable(uint duration, DeviceInterface::Device_Info *device_info, int target_power, double percent)
{
    for (uint i=0; i<duration && (m_abort == false); i++)
    {
        m_devicemgt->WaitFor1sTick();
        *device_info = m_devicemgt->GetPowerTestMeas();
        LogMessage(LOG_STATUS, "Power: " + Float_to_String<double>(device_info->Power_mW/1000.0,1) + "W");
        WriteToMeasurementFile(*device_info, target_power, device_info->Power_mW,percent, 0.0,0.0);
    }
}

PowerTest::meas_temp_pwr PowerTest::MeasPwr(uint duration, DeviceInterface::Device_Info *device_info, double percent, bool calibration)
{
    double pwr = 0.0;
    double pwr_sum = 0.0;
    double temperature = 0.0;
    std::string temp_str = "total power";
    meas_temp_pwr result;

    if (calibration == true)
        temp_str = "calibration power";

    if (duration == 0)
    {
        *device_info = m_devicemgt->GetPowerTestMeas();
        if (calibration == true)
            pwr = device_info->Power_Calib_mW / 1000.0;
        else
            pwr = device_info->Power_mW / 1000.0;
        LogMessage(LOG_DEBUG, "MeasPwr: " + temp_str + ": " + Float_to_String<double>(pwr,1) + "W");
        WriteToMeasurementFile( *device_info, 0, pwr*1000.0, percent, 0.0,0.0 );
    }
    else
    {
        LogMessage(LOG_STATUS, "Measure Power during " + std::to_string(duration)  + " sec");
        for (uint i=0; i<duration && (m_abort == false); i++)
        {
            m_devicemgt->WaitFor1sTick();
            if ((i % NUM_SEC_WATCHDOG) == 0) ResetWatchdog();

            *device_info = m_devicemgt->GetPowerTestMeas();
            if (calibration == true)
                pwr = device_info->Power_Calib_mW / 1000.0;
            else
                pwr = device_info->Power_mW / 1000.0;
            pwr_sum += pwr;
            temperature += device_info->temperature[0];
            LogMessage(LOG_DEBUG, "MeasPwr: " + temp_str + ": " +  Float_to_String<double>(pwr,1) + "W @" + std::to_string(device_info->temperature[0])+ "C");
            WriteToMeasurementFile( *device_info, 0, pwr*1000.0, percent, 0.0,0.0 );
        }

        pwr = pwr_sum / ((double)duration);
        temperature = temperature / (double)duration;
        LogMessage(LOG_STATUS, "Measured an average "  + temp_str + " of " + Float_to_String<double>(pwr,1) + "W with toggle rate of " + Float_to_String<double>(percent,1) + "%, during the " + std::to_string(duration)  + " sec, at average temperature of " + Float_to_String<double>(temperature,0)+ "C" );
    }
    result.power = pwr;
    result.temp = round(temperature);

    return result;
}

int PowerTest::LeakCalibReachTemp(int limit_temp, bool heatup, int timeout, DeviceInterface::Device_Info *device_info, std::list<meas_temp_pwr> *meas_list,double percent)
{
    // if the tempereture doesn't change during "timeout" second, the procedure aborts as it looks like it can't reach the target temperature
    int current_temp = 0;
    int total_duration = 0;
    int current_temp_duration = 0;
    int previous_temp = 0;
    *device_info = m_devicemgt->GetPowerTestMeas();
    current_temp =  device_info->temperature[0];
    meas_temp_pwr meas;
    bool error = false;
    int max_duration = 20 * timeout;

    if (timeout > 0)
    {
        LogMessage(LOG_INFO, "Wait until FPGA temperature reaches " + std::to_string(limit_temp)  + "C (with a saturation timeout of " + std::to_string(timeout) +  "sec), current temperature is " + std::to_string(current_temp) + "C");
    }
    else
    {
        LogMessage(LOG_WARN, "Wait until FPGA temperature reaches " + std::to_string(limit_temp)  + "C WITHOUT any timeout, current temperature is " + std::to_string(current_temp) + "C");
    }

    previous_temp = device_info->temperature[0];
    while ( ((current_temp < limit_temp) && (heatup == true)) || ((current_temp > limit_temp) && (heatup == false)) )
    {
        m_devicemgt->WaitFor1sTick();
        total_duration++;
        if ((total_duration % NUM_SEC_WATCHDOG) == 0) ResetWatchdog();

        *device_info = m_devicemgt->GetPowerTestMeas();
        WriteToMeasurementFile( *device_info, 0, device_info->Power_Calib_mW, percent,0.0,0.0);

        current_temp = device_info->temperature[0];
        meas.temp  = device_info->temperature[0];
        meas.power = (device_info->Power_Calib_mW)/1000.0;
        meas_list->push_back(meas);
        if ( (heatup == false) && (percent == 0.0) )
            WriteToLeakCalibrationFile(0.0,meas.temp,meas.power,0,0.0,0.0,0.0,0.0,0.0);

        std::string temp_str =  std::to_string(total_duration)  + " sec, temperature: " +  std::to_string(current_temp) + "C, power " + Float_to_String<double>(meas.power,1) + "W";

        if (current_temp == previous_temp)
        {
            current_temp_duration++;
            if ((current_temp_duration % (timeout/4)) == 0)
            {
                LogMessage(LOG_INFO,temp_str);
            }
            else
            {
                LogMessage(LOG_DEBUG,temp_str);
            }
        }
        else
        {
            previous_temp = current_temp;
            current_temp_duration = 0;
            LogMessage(LOG_INFO, temp_str);
        }

        if ( ((current_temp_duration >= timeout) && (timeout > 0)) || (m_abort == true))
        {
            error = true;
            LogMessage(LOG_WARN, "Saturation reached after " + std::to_string(total_duration) + "s; the temperature was constant (@ " +  std::to_string(current_temp) + "C) during the last " + std::to_string(timeout)  + " seconds, stop now!");
            break;
        }

        if ( ((total_duration >= max_duration) && (timeout > 0)) || (m_abort == true))
        {
            error = true;
            LogMessage(LOG_WARN, "Timeout reached (" + std::to_string(max_duration) + "s), stop now!");
            break;
        }

        if ( (meas.power+1.0) > (double)m_xbtest_pfm_def.physical.power.max_calibration)
        {
            current_temp = -1;
            error = true;
            LogMessage(LOG_WARN, "Current power is too close to the calibration max power: " + Float_to_String(meas.power,1) + "W, stop now!");
            break;
        }
    }

    if (error == true)
    {
        LogMessage(LOG_WARN, "Couldn't reach the target temperature of " + std::to_string(limit_temp) + "C");
    }
    else
    {
        LogMessage(LOG_PASS, "Reach the target temperature of " + std::to_string(limit_temp) + "C, within " + std::to_string(total_duration)  + " seconds" );
    }

    return current_temp;
}


void PowerTest::ClearPwrMeasList()
{
    // empty the list and insert initial point
    //meas pwr_meas;
    m_pwr_meas_list.clear();
    //pwr_meas.percent = 0.0;
    //pwr_meas.power = 0.0;
    //m_pwr_meas_list.push_back(pwr_meas);
}

void PowerTest::WriteToMeasurementFile( DeviceInterface::Device_Info device_info, int power_target, double power_filter, double toggle_rate, double pwr_err, double pwr_err_filt )
{
    std::string msg;

    msg = std::to_string(device_info.mFanRpm) + ",";

    for (uint i=0; i<m_xbtest_pfm_def.physical.thermal.num_temp_sources; i++)
    {
        msg = msg + std::to_string(device_info.temperature[i]) + ",";
    }
    for (uint i=0; i<m_xbtest_pfm_def.physical.power.num_power_sources; i++)
    {
        if (m_xbtest_pfm_def.physical.power.power_sources[i].def_by_curr_volt == true)
        {
            msg = msg + std::to_string(device_info.current[i]) + ",";
            msg = msg + std::to_string(device_info.voltage[i]) + ",";
        }
        msg = msg + std::to_string(device_info.power[i]) + ",";
    }
    msg = msg + std::to_string(   device_info.Power_mW/1000.0    ) + ",";
    msg = msg +  std::to_string(   power_filter/1000.0            ) + ",";
    msg = msg +  std::to_string(   power_target                   ) + ",";
    msg = msg + Float_to_String(  toggle_rate,1                  ) + ",";
    msg = msg + std::to_string(   pwr_err/1000.0                 ) + ",";
    msg = msg + std::to_string(   pwr_err_filt/1000.0            ) + ",";
    msg = msg + "\n";

    if (m_use_outputfile == true)
    {
        m_outputfile << msg;
        m_outputfile.flush();
    }

    #if USE_SOCKET
        if (m_client_socket > 0)
        {
            char buffer[256];
            std::string rcv_data;
            msg = m_log_msg_test_type + msg;

            strcpy(buffer, msg.c_str());

            if (write(m_client_socket, (char*)&buffer, strlen(buffer)) > 0) {
                // get confirmation from server
                memset(buffer, '\0', sizeof(buffer));
                if (read(m_client_socket, (char*)&buffer, sizeof(buffer)) > 0)
                {
                    rcv_data = buffer;
                    if (StrMatchNoCase(rcv_data, "OK"))
                        LogMessage(LOG_DEBUG, "msg received by server");
                    else
                        LogMessage(LOG_ERROR, "msg not received by server: " + msg);
                }
                else
                {
                    LogMessage(LOG_DEBUG, "Socket error, no acknoledge received from server");
                }
            }
        }
    #endif
}


void PowerTest::WriteToLeakCalibrationFile(double idle_pwr, int temperature, double raw_power, int avg_temp, double avg_power, double a, double b, double c, double error)
{
    if (m_use_leak_calib_outputfile == true)
    {
        m_leak_calib_outputfile
            << Float_to_String(  idle_pwr,10 ) << ","
            << std::to_string(   temperature ) << ","
            << Float_to_String(  raw_power,10) << ","
            << std::to_string(   avg_temp    ) << ","
            << std::to_string(   avg_power   ) << ","
            << Float_to_String(  a,10        ) << ","
            << Float_to_String(  b,10        ) << ","
            << Float_to_String(  c,10        ) << ","
            << Float_to_String(  error,10    ) << ","
            << "\n";
        m_leak_calib_outputfile.flush();
    }
}


void PowerTest::GetPwrResources()
{
    m_pwr_resource.num_slice   = 0;
    m_pwr_resource.num_dsp48e2 = 0;
    m_pwr_resource.num_ramb36  = 0;
    m_pwr_resource.num_uram288 = 0;

    //int pwr_krnl = static_cast<std::underlying_type<KrnlType>::type>(KRNL_TYPE_PWR);
    for (int kernel_idx = 0; kernel_idx < m_device->GetNumKernels(KRNL_TYPE_PWR) ; kernel_idx++)
    {
        DeviceInterface::Build_Info krnl_bi = m_device->GetKrnlBI(KRNL_TYPE_PWR, kernel_idx, KERNEL_CORE_IDX_UNUSED);
        m_pwr_resource.num_slice   += krnl_bi.num_reg;
        m_pwr_resource.num_dsp48e2 += krnl_bi.num_dsp48e2;
        m_pwr_resource.num_ramb36  += krnl_bi.num_ramb36;
        m_pwr_resource.num_uram288 += krnl_bi.num_uram288;
    }

    LogMessage(LOG_DEBUG, "total Slice: " + std::to_string(m_pwr_resource.num_slice));
    LogMessage(LOG_DEBUG, "total DSP  : " + std::to_string(m_pwr_resource.num_dsp48e2));
    LogMessage(LOG_DEBUG, "total BRAM : " + std::to_string(m_pwr_resource.num_ramb36));
    LogMessage(LOG_DEBUG, "total URAM : " + std::to_string(m_pwr_resource.num_uram288));
}

double PowerTest::GetPwrEstimation()
{
    double pwr = 0.0;

    pwr = (m_pwr_resource.num_slice * PWR_SLICE) + (m_pwr_resource.num_dsp48e2 * PWR_DSP) + (m_pwr_resource.num_ramb36 * PWR_BRAM) + (m_pwr_resource.num_uram288 * PWR_URAM);
    LogMessage(LOG_STATUS, "Estimated available power based on the resources: " + Float_to_String<double>(pwr,1) + "W");
    return pwr;
}

double PowerTest::ComputeMaxPwr()
{

    // create linear regression
    //      Regression Equation (y) = a + bx
    //      Slope(b) = (NΣXY - (ΣX)(ΣY)) / (NΣX2 - (ΣX)2)
    //      Intercept(a) = (ΣY - b(ΣX)) / N
    double SumX, SumY, SumXY, SumX2;
    int N = m_pwr_meas_list.size();

    SumX = 0.0;
    SumY = 0.0;
    SumXY = 0.0;
    SumX2 = 0.0;

    for (meas n : m_pwr_meas_list)
    {
        SumX  += n.percent;
        SumX2 += n.percent * n.percent;
        SumY  += n.power;
        SumXY += n.percent * n.power;
        LogMessage(LOG_DEBUG, "percent: " + Float_to_String<double>(n.percent,3) + "%, pwr: " + Float_to_String<double>(n.power,3) + "W");
    }
    // LogMessage(LOG_DEBUG, "SumX: " + Float_to_String<double>(SumX,1));
    // LogMessage(LOG_DEBUG, "SumX2: " + Float_to_String<double>(SumX2,1));
    // LogMessage(LOG_DEBUG, "SumY: " + Float_to_String<double>(SumY,1)) ;
    // LogMessage(LOG_DEBUG, "SumXY: " + Float_to_String<double>(SumXY,1));

    double slope     = ( ((double)N*SumXY) - SumX*SumY ) / ( (double)N*SumX2 - SumX*SumX );
    double intercept =  (SumY - slope*SumX)/(double)N;
    LogMessage(LOG_DEBUG, "slope: " + std::to_string(slope));
    LogMessage(LOG_DEBUG, "intercept: " + std::to_string(intercept));
    double max_power = intercept + 100 * slope;

    return max_power;

}

void PowerTest::CalibrationMeasSorting(double idle_power, std::list<meas_temp_pwr> *meas_list, std::list<meas_temp_pwr> *leak_list)
{
    // create a list
    meas_temp_pwr meas;

    double avg_power = 0.0;
    std::list<double> power_list;
    std::list<double> temp_powerlist_array[MAX_CALIB_TEMP]; // from 0C to MAX_CALIB_TEMPC

    for (meas_temp_pwr n : *meas_list)
    {
        if (!temp_powerlist_array[n.temp].empty())
        {
            // add the power to the list for the temperature
            power_list = temp_powerlist_array[n.temp];
            power_list.push_back(n.power);
        }
        else
        {
            // new element; add it to the map
            power_list.clear();
            power_list.push_back(n.power);
            //LogMessage(LOG_DEBUG, "New list created for " + std::to_string(n.temp));
        }
        temp_powerlist_array[n.temp] = power_list;

        avg_power += n.power;
    }
    avg_power = avg_power/meas_list->size();

    double power;
    int ii;

    // for each temperature, compute the average power of the list of power and substract the idle power
    for (int temp=0; temp < MAX_CALIB_TEMP; temp++)
    {
        power_list = temp_powerlist_array[temp];
        if (!power_list.empty())
        {
            //LogMessage(LOG_DEBUG, "List for " + std::to_string(temp));
            ii = 0;
            power = 0.0;
            for (auto pwr : power_list)
            {
                //LogMessage(LOG_DEBUG, "" + std::to_string(pwr));
                // remove out of range value, unless the input list is already a list of leakage
                if ( (pwr < (avg_power*1.5)) || (idle_power == 0.0) )
                {
                    power += pwr;
                    ii++;
                }
            }
            if (ii != 0)
            {
                meas.temp = temp;
                meas.power = (power/ii)- idle_power;
                WriteToLeakCalibrationFile(0.0,0,0.0,meas.temp,meas.power,0.0,0.0,0.0,0.0);
                leak_list->push_back(meas);
            }
        }
    }
}
double PowerTest::LeastSquareError(double a, double b, double c, std::list<meas_temp_pwr> meas_list)
{
    // compute the least square error between the real sample and the exponential fitting curve
    double pwr = 0.0;
    double error = 0.0;
    for (meas_temp_pwr n : meas_list)
    {
        pwr = a + b*exp(c*n.temp);
        error += pow((n.power - pwr),2.0);
    }
    return error;
}
void PowerTest::CalibrationExpFitting(leakage_exp_curve *leakage_curve, std::list<meas_temp_pwr> *leak_list)
{
    meas_temp_pwr first_meas;
    meas_temp_pwr prev_meas;
    double Sk = 0.0;
    double Xk_minus_X1 = 0.0;
    double Yk_minus_Y1 = 0.0;

    double sum_Sk_square = 0.0;
    double sum_Xk_minus_X1_square = 0.0;
    double sum_Xk_minus_X1_mult_Sk = 0.0;
    double sum_Xk_minus_X1_mult_Yk_minus_Y1 = 0.0;
    double sum_Yk_minus_Y1_mult_Sk = 0.0;

    bool skip_first = true;
    for (meas_temp_pwr n : *leak_list)
    {
        // LogMessage(LOG_DEBUG, " " + std::to_string(n.temp) + " " + std::to_string(n.power));
        if (skip_first == true)
        {
            skip_first = false;
            first_meas = n;
            Sk = 0.0;
        }
        else
        {
            Sk = Sk + 0.5*(n.power + prev_meas.power)*(n.temp - prev_meas.temp);
            sum_Sk_square += Sk*Sk;
            Xk_minus_X1 = n.temp - first_meas.temp;
            Yk_minus_Y1 = n.power - first_meas.power;
            sum_Xk_minus_X1_square += Xk_minus_X1 * Xk_minus_X1;
            sum_Xk_minus_X1_mult_Sk += Xk_minus_X1 * Sk;
            sum_Xk_minus_X1_mult_Yk_minus_Y1 += Xk_minus_X1 * Yk_minus_Y1;
            sum_Yk_minus_Y1_mult_Sk += Yk_minus_Y1 * Sk;
        }
        prev_meas = n;
    }
    LogMessage(LOG_DEBUG, "sum_Xk_minus_X1_square: " + std::to_string(sum_Xk_minus_X1_square));
    LogMessage(LOG_DEBUG, "sum_Xk_minus_X1_mult_Sk: " + std::to_string(sum_Xk_minus_X1_mult_Sk));
    LogMessage(LOG_DEBUG, "sum_Sk_square: " + std::to_string(sum_Sk_square));
    LogMessage(LOG_DEBUG, "sum_Xk_minus_X1_mult_Yk_minus_Y1: " + std::to_string(sum_Xk_minus_X1_mult_Yk_minus_Y1));
    LogMessage(LOG_DEBUG, "sum_Yk_minus_Y1_mult_Sk: " + std::to_string(sum_Yk_minus_Y1_mult_Sk));

    double determinant = sum_Sk_square * sum_Xk_minus_X1_square - sum_Xk_minus_X1_mult_Sk*sum_Xk_minus_X1_mult_Sk;
    LogMessage(LOG_DEBUG, "determinant: " + std::to_string(determinant));
    double B1 = ( (0.0-sum_Xk_minus_X1_mult_Sk)*sum_Xk_minus_X1_mult_Yk_minus_Y1 + sum_Xk_minus_X1_square* sum_Yk_minus_Y1_mult_Sk ) / determinant;
    double c2 = B1;
    LogMessage(LOG_DEBUG, "c2: " + std::to_string(c2));

    int ii = 0;
    double Ok = 0.0;
    double sum_Ok = 0.0;
    double sum_Ok_square = 0.0;
    double sum_Yk = 0.0;
    double sum_Yk_mult_Ok = 0.0;

    for (meas_temp_pwr n : *leak_list)
    {
        ii += 1;
        Ok = exp(c2 * n.temp);
        sum_Ok += Ok;
        sum_Ok_square += Ok*Ok;
        sum_Yk += n.power;
        sum_Yk_mult_Ok += n.power * Ok;
    }

    LogMessage(LOG_DEBUG, "sum_Ok: " + std::to_string(sum_Ok));
    LogMessage(LOG_DEBUG, "sum_Ok_square: " + std::to_string(sum_Ok_square));
    LogMessage(LOG_DEBUG, "n: " + std::to_string(ii));
    LogMessage(LOG_DEBUG, "sum_Yk: " + std::to_string(sum_Yk));
    LogMessage(LOG_DEBUG, "sum_Yk_mult_Ok: " + std::to_string(sum_Yk_mult_Ok));
    determinant = ii*sum_Ok_square - sum_Ok*sum_Ok;
    LogMessage(LOG_DEBUG, "determinant: " + std::to_string((double)determinant));
    double a2 = (sum_Ok_square*sum_Yk - sum_Ok*sum_Yk_mult_Ok)/determinant;
    double b2 = (ii*sum_Yk_mult_Ok - sum_Ok*sum_Yk)/determinant;
    LogMessage(LOG_DEBUG, "a2: " + std::to_string(a2));
    LogMessage(LOG_DEBUG, "b2: " + std::to_string(b2));

    LogMessage(LOG_INFO, "calibration: leakage exponential fitting: leakage power = " + Float_to_String(a2,10)+ " + " + Float_to_String(b2,10) +" * exp (" + Float_to_String(c2,10) + " * temperature)");

    // compute the least square error
    double least_square_error = 0.0;
    least_square_error = LeastSquareError(a2,b2,c2,*leak_list);
    LogMessage(LOG_INFO, "calibration: leakage exponential fitting: least square error: " + std::to_string(least_square_error) + " over " + std::to_string(leak_list->size()) + " samples");
    leakage_curve->a = a2;
    leakage_curve->b = b2;
    leakage_curve->c = c2;
    leakage_curve->err = least_square_error;
    WriteToLeakCalibrationFile(0.0,0,0.0,0,0.0,a2,b2,c2, least_square_error);
}
double PowerTest::LeakagePower(int temperature, leakage_exp_curve leakage_curve )
{
    double leakage_power = 0.0;
    leakage_power = leakage_curve.a + leakage_curve.b * exp(leakage_curve.c * (temperature+273.0));
    LogMessage(LOG_DEBUG, "leakage power @" + std::to_string(temperature) + "C: " + Float_to_String(leakage_power,1) + "W" );
    return leakage_power;
}


double PowerTest::XPE_Leakage(int temperature )
{
    double leakage_power = 0.0;
    leakage_power = m_xbtest_pfm_def.physical.thermal.xpe_leakage.a + m_xbtest_pfm_def.physical.thermal.xpe_leakage.b * exp(m_xbtest_pfm_def.physical.thermal.xpe_leakage.c * (temperature));
    LogMessage(LOG_DEBUG, "XPE Leakage @" + std::to_string(temperature) + "C: " + Float_to_String(leakage_power,1) + "W" );
    return leakage_power;
}


int PowerTest::ComputeThrottleOffset(double pwr_err, int temperature, double StaticAvailPower, bool limit_swing, leakage_exp_curve leakage_curve)
{
    int offset = 0;
    double PowerThrottle     = 0.0;
    double leak_pwr = LeakagePower(temperature, leakage_curve) * 1000.0; // w to milli watt
    double total_pwr = StaticAvailPower + leak_pwr;
    LogMessage(LOG_DEBUG, "total_pwr @" + std::to_string(temperature) + "C = " + Float_to_String(StaticAvailPower,1)+ "W + " + Float_to_String(leak_pwr,1) + "W" );

    PowerThrottle = total_pwr / (QTY_THROTTLE_STEP-1);

    offset = round(pwr_err/PowerThrottle);
    LogMessage(LOG_DEBUG, "throttle offset " + std::to_string(offset) );

    if (limit_swing == true)
    {
        // limit to 1%
        if (abs(offset) > QTY_THROTTLE_STEP/100)
        {
            if (offset < 0) offset = 0 - QTY_THROTTLE_STEP/100;
            else offset = QTY_THROTTLE_STEP/100;
            LogMessage(LOG_DEBUG, "throttle update clipped to " + std::to_string(offset) );
        }
    }

    return offset;
}

int PowerTest::ComputeThrottleForPwr(double target_power, double idle_power, int temperature, double StaticAvailPower, leakage_exp_curve leakage_curve)
{
    int throttle = 0;

    double leak_pwr = LeakagePower(temperature, leakage_curve);
    double cu_pwr = StaticAvailPower / 1000.0;
    LogMessage(LOG_DEBUG, "total_pwr @" + std::to_string(temperature) + "C = " + Float_to_String(cu_pwr,2)+ "W + " + Float_to_String(leak_pwr,2) + "W with an idle power of " + Float_to_String(idle_power,2) + "W");

    throttle = round( (target_power - idle_power - leak_pwr)*(QTY_THROTTLE_STEP-1)/cu_pwr);
    LogMessage(LOG_DEBUG, "throttle value: " + std::to_string(throttle) );

    if (throttle > (QTY_THROTTLE_STEP-1))
    {
        throttle = QTY_THROTTLE_STEP-1;
        LogMessage(LOG_DEBUG, "throttle clipped to " + std::to_string(throttle) );
    }

    return throttle;
}

bool PowerTest::SendFanCtrlfile(std::string fan_ctrl_file)
{

    std::string sys_cmd;
    sys_cmd = "./" + fan_ctrl_file;

    m_pipe = NULL;
    m_pipe = popen(sys_cmd.c_str(), "r");

    if (m_pipe == 0)
    {
        LogMessage(LOG_FAILURE, "Failed to execute command: " + sys_cmd);
        return false;
    }
    return true;
}


bool PowerTest::OpenSocketClient(std::string host, uint port)
{
    bool failure = false;

    // versus AF_LOCAL; reliable, bidirectional; system picks protocol (TCP)
    m_client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_client_socket < 0)
    {
        m_abort = true;
        LogMessage(LOG_FAILURE, "Failed to open client socket");
    }

    // localhost: 127.0.0.1
    struct hostent* hptr = gethostbyname(host.c_str());
    if (!hptr)
    {
        m_abort = true;
        LogMessage(LOG_FAILURE, "Failed to open client socket: could get host by name");
    }

    // versus AF_LOCAL
    if (hptr->h_addrtype != AF_INET)
    {
        m_abort = true;
        LogMessage(LOG_FAILURE, "Failed to open client socket: bad address family");
    }

    // connect to the server: configure server's address 1st
    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = ((struct in_addr*) hptr->h_addr_list[0])->s_addr;
    // convert port number in big-endian
    saddr.sin_port = htons(port);

    if (connect(m_client_socket, (struct sockaddr*) &saddr, sizeof(saddr)) < 0)
    {
        failure = true;
        LogMessage(LOG_FAILURE, "Failed to connect client");
    }

    return failure;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int PowerTest::RunThread( PowerTestcaseCfg_t TC_Cfg, std::list<TestItConfig_t> *Tests_list )
{

    bool test_failure           = false;
    bool test_it_failure        = false;
    double meas_power_filter = 0.0;
    double power_filt = 0.0;
    double power_lpf = 0.0;
    DeviceInterface::Device_Info device_info;
    double percent = 0.0;
    int throttle = 0;
    int throttle_update = 0;
    int initial_temperature = 0;
    int power_in_tol_cnt ;
    int power_out_tol_cnt;
    int power_tol_cnt_check = 0;

    int raw_power_in_tol_cnt;
    int raw_power_out_tol_cnt;
    int raw_power_tol_cnt_check = 0;

    double StaticAvailPower  = 0.0;
    double idle_power        = 0.0;
    double EstimatedPower    = 0.0;
    double cal_pwr_range     = 0.0;
    double max_cal_percent   = 0.0;
    double cal_percent       = 0.0;
    double pwr_calib_percent = 0.0;
    double power_cal         = 0.0;

    std::list<meas_temp_pwr>  leak_meas_list;
    std::list<meas_temp_pwr>  dummy_leak_meas_list;
    std::list<meas_temp_pwr>  leakage_list;
    std::list<meas_temp_pwr>  global_leakage_list;
    leakage_exp_curve leakage_curve;
    int reach_temp = 0;
    int leak_calib_max_temp_reached = 0;
    uint jj;

    int leakage_calib_low_temp = 0;
    int leakage_calib_high_temp = 0;

    double base_power        = 0.0;

    meas_temp_pwr temperature_power;

    // return -1(Abort), 0(Pass), 1(Fail)
    int ret = 0;

    // toggle rate update
    uint TR_update_rate = 2;
    if ( (StrMatchNoCase(m_xbtest_pfm_def.info.name, "xilinx_u50_xdma_201910_1")) || (StrMatchNoCase(m_xbtest_pfm_def.info.name, "xilinx_u50_xdma_201920_1")) || (StrMatchNoCase(m_xbtest_pfm_def.info.name, "xilinx_u50_xdma_201920_2")) )
    {
        TR_update_rate = 4; // TODO remove when CR-1037128 fixed
        //m_pwr_err_filter_alpha = 3.0;
    }
    //m_pwr_err_filter_alpha = 10.0;

    LogMessage(LOG_INFO, "Start all power compute units with a null toggle rate");
    StartPowerKernel(TC_Cfg);

    if (m_leakage_use_fan_ctrl_file == true)
    {
        LogMessage(LOG_INFO, "Speed up fans by using: " + m_fan_max_file_name);
        SendFanCtrlfile(m_fan_max_file_name);
        // WaitSecTick 2 sec to get the first measurement
        WaitSecTick(2);
    }

    #if USE_SOCKET

        //open client server and connect to it
        m_abort = OpenSocketClient("localhost", 10000);
        /*
        // Write some stuff and read the echoes.
        LogMessage(LOG_INFO,"Connect to server, about to write some stuff...");
        int i;
        char buffer[256];
        std::string data;
        for (i = 0; i < 50  && (m_abort == false); i++) {
            WaitSecTick(1);
            //clear the buffer
            memset(&buffer, 0, sizeof(buffer));
            data = std::to_string((2*i+1));
            strcpy(buffer, data.c_str());

            if (write(m_client_socket, (char*)&buffer, strlen(buffer)) > 0) {
                // get confirmation echoed from server and print
                memset(buffer, '\0', sizeof(buffer));
                if (read(m_client_socket, (char*)&buffer, sizeof(buffer)) > 0)
                    data = buffer;
                    LogMessage(LOG_INFO, "received: |" + data + "|");
            }
        }
        */
    #endif


    if ((StrMatchNoCase(TC_Cfg.test_sequence_mode, TEST_SEQUENCE_MODE_MEMBER_DUR_WATT)) && (m_abort == false))
    {
        // wait for all other test to have started
        LogMessage(LOG_INFO, "Calibration - start: wait for other test to start");

        percent = 0.0;
        SetClkThrottle(Percent2Throttle(percent), true);
        WaitSecTick(5);
        temperature_power = MeasPwr(5, &device_info, percent, CALIBRATION_POWER);  // idle power of the calibrated power rails
        idle_power = temperature_power.power;
        initial_temperature = device_info.temperature[0];

        // from teh total calibration power available, remove the idle power computed on the calibrated power rail only
        //  this is the total power actually available for calibration
        cal_pwr_range = (double)m_xbtest_pfm_def.physical.power.max_calibration - idle_power;

        LogMessage(LOG_INFO, "Calibration: idle calibration power: " + Float_to_String<double>(idle_power,1) + "W");

        if (m_power_calibration == 0)
        {
            if (cal_pwr_range <= 0.0)
            {
                LogMessage(LOG_FAILURE, "Impossible to perform power calibration as the idle power of the board is bigger than the defined calibration power: " + Float_to_String<double>(idle_power,1) + "W >= " + std::to_string(m_xbtest_pfm_def.physical.power.max_calibration) + "W" );
                m_abort = true;
            }
            else
            {
                GetPwrResources();
                EstimatedPower = GetPwrEstimation()*1.2; // take a margin of 20%
                // compute the max swing of power that the card can cope with
                LogMessage(LOG_INFO, "Calibration - Power: max authorised calibration power: " + std::to_string(m_xbtest_pfm_def.physical.power.max_calibration) + "W");
                LogMessage(LOG_INFO, "Calibration - Power: based on resource count, when toggle rate is 100%, the max generated power could be " + Float_to_String<double>(EstimatedPower,1) + "W"  );

                // check if the half EstimatedPower is NOT bigger than than the cal_pwr_range,
                // power calibration is done with toggle rate @ 50%

                pwr_calib_percent = 50.0;
                while ((EstimatedPower/(100.0/pwr_calib_percent)) > cal_pwr_range)
                {
                    pwr_calib_percent = pwr_calib_percent - 5.0;
                    LogMessage(LOG_DEBUG, "Calibration - Power: remove 5%: "  + Float_to_String<double>(pwr_calib_percent,0) + "%" );

                }
                LogMessage(LOG_INFO, "Calibration - Power: use a toggle rate of "  + Float_to_String<double>(pwr_calib_percent,0) + "% during the calibration");

                const int NUM_PWR_CAL = 3;
                double pwr_cal_iter [NUM_PWR_CAL];

                double idle_a  = 0.0;
                double idle_b  = 0.0;
                power_cal  = 0.0;

                percent = 0.0;
                SetClkThrottle(Percent2Throttle(percent), false);
                WaitSecTick(2+TR_update_rate);
                temperature_power = MeasPwr(5, &device_info, percent, CALIBRATION_POWER);
                idle_a = temperature_power.power;// - XPE_Leakage(temperature_power.temp);

                for (uint j = 0; j < NUM_PWR_CAL && (m_abort == false); j++)
                {
                    if (j != 0)
                        idle_a = idle_b;

                    percent = pwr_calib_percent;
                    SetClkThrottle(Percent2Throttle(percent), false);
                    WaitSecTick(2+TR_update_rate);
                    temperature_power = MeasPwr(5, &device_info, percent, CALIBRATION_POWER);
                    power_cal = temperature_power.power;// - XPE_Leakage(temperature_power.temp);

                    percent = 0.0;
                    SetClkThrottle(Percent2Throttle(percent), false);
                    WaitSecTick(2+TR_update_rate);
                    temperature_power = MeasPwr(5, &device_info, percent, CALIBRATION_POWER);
                    idle_b = temperature_power.power;// - XPE_Leakage(temperature_power.temp);

                    pwr_cal_iter[j] = (100/pwr_calib_percent) * (power_cal - ((idle_a+idle_b)/2) );
                    LogMessage(LOG_DEBUG, "Calibration - power: intermediate estimation of power available: " + Float_to_String<double>(pwr_cal_iter[j] ,1) + "W");
                }

                // compute average
                if (m_abort == false)
                {
                    StaticAvailPower = 0.0;
                    for (uint k = 0; k < NUM_PWR_CAL; k++)
                    {
                        LogMessage(LOG_DEBUG, "Calibration - power: " + Float_to_String<double>(pwr_cal_iter[k],1) + "W ");
                        StaticAvailPower += pwr_cal_iter[k];
                    }
                    StaticAvailPower = StaticAvailPower/NUM_PWR_CAL;

                    // calibration failed if the computed power is negative or too far away from the power estamatied via the resource counting
                    if ( (StaticAvailPower <= 0) || ( StaticAvailPower <= EstimatedPower * 0.5) || ( StaticAvailPower >= EstimatedPower * 1.5) )
                    {
                        LogMessage(LOG_FAILURE, "Calibration - power: power available is too far (more than 50%) from the power estimated based on ressource count. " + Float_to_String<double>(StaticAvailPower ,1) + "W, too far away from estimated " + Float_to_String<double>(EstimatedPower,1) + "W");
                        m_abort = true;
                    }
                    else
                    {
                        LogMessage(LOG_PASS, "Calibration - power: power available, " + Float_to_String<double>(StaticAvailPower ,1) + "W");
                    }
                }

                percent = 0.0;
                SetClkThrottle(Percent2Throttle(percent), true);
                WaitSecTick(2+TR_update_rate);
            }
        }
        else
        {
            StaticAvailPower = m_power_calibration;
        }


        if ( (TC_Cfg.num_leakage_calib != 0) && (m_abort == false) )
        {

            leakage_calib_low_temp = initial_temperature + 1;
            if (TC_Cfg.leakage_calib_low_temp != 0)
                leakage_calib_low_temp = TC_Cfg.leakage_calib_low_temp;

            // stop 1C below the limit value to avoid the device_mgt to brutally abort if limit reached.
            leakage_calib_high_temp = m_xbtest_pfm_def.physical.thermal.temp_sources[0].limit - 2;
            if (TC_Cfg.leakage_calib_high_temp != 0)
            {
                if ( TC_Cfg.leakage_calib_high_temp <= (m_xbtest_pfm_def.physical.thermal.temp_sources[0].limit - 2)  )
                {
                    leakage_calib_high_temp = TC_Cfg.leakage_calib_high_temp;
                }
            }

            // from teh total power avaible for calibration, remove the power coming from the leakage,
            //  as we'll heat up the board, this leakage opwer will be added to the dynamic power
            // cal_pwr_range -= XPE_Leakage(leakage_calib_high_temp);

            max_cal_percent = floor(100.0*cal_pwr_range/StaticAvailPower);
            if (max_cal_percent > 100.0) max_cal_percent = 100.0;
            cal_percent = max_cal_percent*9.0/10.0;

            m_individual_leakage_curve_list.clear();

            jj = 0;
            while (jj < TC_Cfg.num_leakage_calib && (m_abort == false))
            {

                LogMessage(LOG_INFO, "Calibration - leakage: attempt number " + std::to_string(jj+1));
                dummy_leak_meas_list.clear();
                leak_meas_list.clear();
                percent = 0.0;
                SetClkThrottle(Percent2Throttle(percent), false);
                WaitSecTick(2);
                LeakCalibReachTemp(leakage_calib_low_temp, COOL_DOWN, TC_Cfg.leakage_calib_timeout, &device_info, &dummy_leak_meas_list, percent);

                LogMessage(LOG_INFO, "Calibration - leakage: let's try to heat-up the FPGA to " + std::to_string(leakage_calib_high_temp) + "C, use fixed toggle rate of " + Float_to_String<double>(cal_percent,1) + "%");
                if (m_leakage_use_fan_ctrl_file == true)
                {
                    LogMessage(LOG_INFO, "Calibration - leakage: Slow down fans by using: " + m_fan_min_file_name);
                    SendFanCtrlfile(m_fan_min_file_name);
                }

                SetClkThrottle(Percent2Throttle(cal_percent), false);
                reach_temp = LeakCalibReachTemp(leakage_calib_high_temp, HEAT_UP, TC_Cfg.leakage_calib_timeout, &device_info, &dummy_leak_meas_list, cal_percent);
                if (reach_temp == -1)
                {
                    cal_percent -= 5; // remove 5%
                    LogMessage(LOG_WARN, "Calibration - leakage: calibration toggle rate is too high, too much power drained, reduce toggle rate to " + Float_to_String<double>(cal_percent,1) + "% and restarts the whole calibaration");
                    if (m_leakage_use_fan_ctrl_file == true)
                    {
                        LogMessage(LOG_INFO, "Calibration - leakage: Speed up fans by using: " + m_fan_max_file_name);
                        SendFanCtrlfile(m_fan_max_file_name);
                    }
                    m_individual_leakage_curve_list.clear();
                    global_leakage_list.clear();
                    jj = 0;
                }
                else
                {
                    jj++;
                    if (reach_temp > leak_calib_max_temp_reached)
                        leak_calib_max_temp_reached = reach_temp;

                    LogMessage(LOG_INFO, "Calibration - leakage: let cool down the board and take measurements");
                    percent = 0.0;
                    SetClkThrottle(Percent2Throttle(percent), false);
                    WaitSecTick(2);
                    if (m_leakage_use_fan_ctrl_file == true)
                    {
                        LogMessage(LOG_INFO, "Calibration - leakage: Speed up fans by using: " + m_fan_max_file_name);
                        SendFanCtrlfile(m_fan_max_file_name);
                    }
                    LeakCalibReachTemp(leakage_calib_low_temp, COOL_DOWN, TC_Cfg.leakage_calib_timeout, &device_info, &leak_meas_list, percent);
                    temperature_power = MeasPwr(5, &device_info, percent, CALIBRATION_POWER);
                    idle_power = temperature_power.power;

                    //idle_power = idle_power - LeakagePower(device_info.temperature[0], U250_XPE_LEAK_TEMP_EXT_PROC_MAX);
                    WriteToLeakCalibrationFile(idle_power,temperature_power.temp,0.0,0,0.0,0.0,0.0,0.0,0.0);

                //    CalibrationMeasSorting(idle_power, &leak_meas_list, &leakage_list);
                    // after each cool down the leakage exp fitting is computed
                //    CalibrationExpFitting(&leakage_curve, &leakage_list);

                    // for (auto n : leakage_list)
                    // {
                    //     LogMessage(LOG_STATUS,"leakage_list " + std::to_string(n.temp) + "C, " + Float_to_String<double>(n.power,2) + "W");
                    // }

                    m_individual_leakage_curve_list.push_back(leakage_curve);
                    // add the leakage_list to the global list
                    global_leakage_list.splice(global_leakage_list.end(), leakage_list);

                    // for (auto n : global_leakage_list)
                    // {
                    //     LogMessage(LOG_STATUS,"global_leakage_list " + std::to_string(n.temp) + "C, " + Float_to_String<double>(n.power,2) + "W");
                    // }
                }
            }

            // as summary, display all individual leakage curve fitting
            LogMessage(LOG_INFO, "Calibration - leakage: calibration curve: f(x) = a + b * e^(c*x), between " + std::to_string(leakage_calib_low_temp) + "C and " + std::to_string(leak_calib_max_temp_reached) + "C");
            jj = 0;
            for (leakage_exp_curve n : m_individual_leakage_curve_list)
            {
                LogMessage(LOG_STATUS, "Calibration - leakage: trial " + std::to_string(jj) + ": \t a: " + Float_to_String<double>(n.a,10) + ", \t b: " + Float_to_String<double>(n.b,10)+ ", \t c: " + Float_to_String<double>(n.c,10) + ", \t least square error: " + Float_to_String<double>(n.err,10));
                jj++;
            }

            // with all leakage value, compute teh final curve fitting
            //  as this is a list of multiple trial, power for identical temperature are present, so compute their average for each temperature
            //  in this case, as the imput list is a leakage list, the idle power has already been removed
        //    CalibrationMeasSorting(0.0, &global_leakage_list, &leakage_list);
            // for (auto n : leakage_list)
            // {
            //     LogMessage(LOG_INFO,"global_leakage_list sorted " + std::to_string(n.temp) + "C, " + Float_to_String<double>(n.power,2) + "W");
            // }
        //    CalibrationExpFitting(&leakage_curve, &leakage_list);
            LogMessage(LOG_STATUS, "Calibration - leakage: Global expo curve fitting: \t a: " + Float_to_String<double>(leakage_curve.a,10) + ", \t b: " + Float_to_String<double>(leakage_curve.b,10)+ ", \t c: " + Float_to_String<double>(leakage_curve.c,10) + ", \t least square error: " + Float_to_String<double>(leakage_curve.err,10));
        }
        else
        {
            leakage_curve.a = m_xbtest_pfm_def.physical.thermal.calibration.a;
            leakage_curve.b = m_xbtest_pfm_def.physical.thermal.calibration.b;
            leakage_curve.c = m_xbtest_pfm_def.physical.thermal.calibration.c;
        }

        if (TC_Cfg.use_leakage_model == false)
        {
            leakage_curve.a = 0.0;
            leakage_curve.b = 0.0;
            leakage_curve.c = 0.0;
        }

        LogMessage(LOG_INFO, "Calibration - end");

    }

    percent = 0.0;
    SetClkThrottle(Percent2Throttle(percent), true);
    WaitSecTick(2+TR_update_rate);
    temperature_power =  MeasPwr(5, &device_info, percent, NORMAL_POWER);
    base_power =  temperature_power.power - LeakagePower(temperature_power.temp, leakage_curve);
    LogMessage(LOG_INFO, "Base power: " + Float_to_String(base_power,2));

    // reset filter
    power_filt = device_info.Power_mW;
    meas_power_filter = device_info.Power_mW;

    int test_it_cnt = 1;

    double power_tol_mW = 0.0;
    test_failure    = false;
    test_it_failure = false;

    // power in milliwatt
    StaticAvailPower = 1000.0* StaticAvailPower;

    double pwr_step  = StaticAvailPower / (QTY_THROTTLE_STEP-1);

    for (auto test_it: *Tests_list)
    {
        if (m_abort == true)
            break;

        test_it_failure = false;
        LogMessage(LOG_INFO, "Start Test: " + std::to_string(test_it_cnt));

        power_tol_mW = ComputePowerTolerance(test_it.target_power);

        LogMessage(LOG_INFO, "\t Duration: " + std::to_string(test_it.duration) + "s");
        if (StrMatchNoCase(TC_Cfg.test_sequence_mode, TEST_SEQUENCE_MODE_MEMBER_DUR_TOG))
            LogMessage(LOG_INFO, "\t Toggle rate: " + std::to_string(test_it.toggle_rate) + "%");
        else if (StrMatchNoCase(TC_Cfg.test_sequence_mode, TEST_SEQUENCE_MODE_MEMBER_DUR_WATT))
            LogMessage(LOG_INFO, "\t Power target: " + std::to_string(test_it.target_power) + "W +/- " + Float_to_String<double>(power_tol_mW/1000.0,1) + "W");

        if (StrMatchNoCase(TC_Cfg.test_sequence_mode, TEST_SEQUENCE_MODE_MEMBER_DUR_TOG))
        {
            percent = double(test_it.toggle_rate);
            SetClkThrottle( Percent2Throttle(percent), true );
        }

        bool power_in_range = false;
        bool power_reached  = false;
        uint power_reach_time = 0;

        int power_in_range_cnt = 0;

        double pwr_err = 0.0;
        double pwr_err_filt = 0.0;
        double pwr_filt_err = 0.0;

        bool new_TR = false;

        power_in_tol_cnt = 0;
        power_out_tol_cnt = 0;
        raw_power_in_tol_cnt = 0;
        raw_power_out_tol_cnt = 0;

        // Loop until done or abort
        for (uint j = 0; j<test_it.duration && (m_abort == false); j++)
        {
            uint i = test_it.duration - j;
            new_TR = false;

            m_devicemgt->WaitFor1sTick();
            if ((j % NUM_SEC_WATCHDOG) == 0) ResetWatchdog();

            if (!IPC_queue.empty())
            {
                if (IPC_queue.front() == 0)
                {
                    LogMessage(LOG_DEBUG, "IPC: A Memory test is over");
                }
                IPC_queue.pop();
            }

            device_info = m_devicemgt->GetPowerTestMeas();

            if (StrMatchNoCase(TC_Cfg.test_sequence_mode, TEST_SEQUENCE_MODE_MEMBER_DUR_TOG))
            {
                if (i == test_it.duration)
                {
                    // reset filter with the first measurement
                    meas_power_filter = device_info.Power_mW;
                }
                else
                {
                    meas_power_filter = (1.0-PWR_FILT_APLHA)*device_info.Power_mW + (PWR_FILT_APLHA*meas_power_filter);
                }
                WriteToMeasurementFile(device_info, 0, meas_power_filter, test_it.toggle_rate, 0.0, 0.0);

                std::string temp_str = "\t" + std::to_string(i) + " sec. remaining, Temp 0: " + std::to_string(device_info.temperature[0]) + " C; current Power: " +  Float_to_String<double>(device_info.Power_mW/1000.0,1) + " W / filtered " + Float_to_String<double>(meas_power_filter/1000.0,1) + " W; toggle rate: " + Float_to_String<double>(percent,1) + " %)";
                LogMessage(LOG_STATUS, temp_str);

            }
            else if (StrMatchNoCase(TC_Cfg.test_sequence_mode, TEST_SEQUENCE_MODE_MEMBER_DUR_WATT))
            {
                if (i == test_it.duration)
                {
                    // reset filter with the first measurement
                    meas_power_filter = device_info.Power_mW;
                }
                else
                {
                    // moving average filter of the power
                    meas_power_filter = (1.0-PWR_FILT_APLHA)*device_info.Power_mW + (PWR_FILT_APLHA*meas_power_filter);
                }

                power_filt = meas_power_filter;

                // pwr_err = target - measured
                //    pwr_err > 0 not enough power is created
                //    pwr_err < 0 too much power is created
                pwr_err = (double)test_it.target_power*1000.0 - device_info.Power_mW;
                pwr_filt_err = (double)test_it.target_power*1000.0 - power_filt;

                // LPF filter of the error
                pwr_err_filt = pwr_err_filt + (pwr_err - pwr_err_filt) / (double)m_pwr_err_filter_alpha;

                if (power_reached == true)
                {
                    if (abs(pwr_err) <= power_tol_mW)
                        raw_power_in_tol_cnt ++;
                    else
                        raw_power_out_tol_cnt ++;

                    if (abs(pwr_filt_err) <= power_tol_mW)
                        power_in_tol_cnt ++;
                    else
                        power_out_tol_cnt ++;
                }
                else
                {
                    if (abs(pwr_err) <= power_tol_mW)
                    {
                        power_reached = true;
                        power_reach_time = test_it.duration - i;
                        //LogMessage(LOG_INFO, "Power target reached after " + std::to_string(power_reach_time) + "s");
                    }
                }

                // this controls the toggle rate limitation, enable it when inside tolerances for 3 consecutive seconds
                if (power_in_range == true)
                {
                    if (abs(pwr_err) <= 3*power_tol_mW)
                    {
                        if (abs(pwr_filt_err) <= power_tol_mW)
                        {
                            if (power_in_range_cnt < TOGGLE_VARIATION_LIMIT_EN) power_in_range_cnt++;
                        }
                        else
                        {
                            if (power_in_range_cnt > 0) power_in_range_cnt--;
                        }
                    }
                }
                else
                {
                    power_in_range_cnt = 0;

                    if (abs(pwr_err) <= 3*power_tol_mW)
                    {
                        power_in_range = true;
                        LogMessage(LOG_INFO, "Close to target power, reset filters");
                        // reset filter
                        power_filt = device_info.Power_mW;
                        pwr_filt_err = pwr_err;
                        meas_power_filter = (double)test_it.target_power*1000.0;

                    }
                }

                if (TC_Cfg.use_leakage_model == true)
                {
                    // LPF
                    power_lpf = power_lpf + (device_info.Power_mW - power_lpf) / (double)m_pwr_err_filter_alpha;
                    pwr_err_filt = (double)test_it.target_power*1000.0 - power_lpf;

                    if (j < TR_update_rate)
                    {
                        power_lpf = (double)test_it.target_power*1000.0;
                        pwr_err_filt = 0.0;
                        pwr_err = 0.0;
                    }

                    // overwrite value for storing into file
                    power_filt = power_lpf;
                }


                if ( (i % TR_update_rate) == 0 )
                {
                    LogMessage(LOG_DEBUG, "throttle before: " + std::to_string(throttle) );
                    if (TC_Cfg.use_leakage_model == false)
                    {
                        LogMessage(LOG_DEBUG, "pwr error " +  Float_to_String<double>(pwr_err/1000.0,1) + "W" );
                        throttle_update = 0;

                        double leak_pwr = LeakagePower(device_info.temperature[0], leakage_curve); // in W
                        LogMessage(LOG_DEBUG, "total_pwr @" + std::to_string(device_info.temperature[0]) + "C = " + Float_to_String(StaticAvailPower/1000.0,1)+ "W + " + Float_to_String(leak_pwr,1) + "W" );

                        throttle_update = round(pwr_err/pwr_step*0.6);
                        LogMessage(LOG_DEBUG, "throttle theoretical update " + std::to_string(round(pwr_err/pwr_step)) +  ", actual update (60%): " + std::to_string(throttle_update));
                    }
                    else
                    {
                        // theoretical value
                        if ( (j == 0) || (m_open_loop == true) )
                        {
                            throttle = ComputeThrottleForPwr((double)test_it.target_power, base_power, device_info.temperature[0], StaticAvailPower, leakage_curve);
                            throttle_update = 0;
                        }
                        else
                        {
                            throttle_update = round(pwr_err_filt/pwr_step);
                        }

                        LogMessage(LOG_DEBUG, "throttle offset due to error: " + std::to_string(throttle_update) );

                    }

                    if ( abs(throttle_update) > (QTY_THROTTLE_STEP /5) )
                    {
                        // limit to update to 20%
                        if (throttle_update < 0) throttle_update = 0 - QTY_THROTTLE_STEP/5;
                        else throttle_update = QTY_THROTTLE_STEP/5;
                        LogMessage(LOG_DEBUG, "throttle offset clipped to 20%: " + std::to_string(throttle_update) );

                    }

                    throttle += throttle_update;

                    // can only be > 0 and <100
                    if (throttle > QTY_THROTTLE_STEP) throttle = QTY_THROTTLE_STEP;
                    if (throttle < 0) throttle = 0;
                    LogMessage(LOG_DEBUG, "throttle after: " + std::to_string(throttle) );
                    SetClkThrottle(throttle, false);
                    percent = Throttle2Percent(throttle);
                    new_TR = true;
                }

                WriteToMeasurementFile(device_info, test_it.target_power, power_filt, percent, pwr_err, pwr_err_filt);

                // display every loop
                std::string temp_str = "\t" + std::to_string(i) + " sec. remaining; " + std::to_string(device_info.temperature[0]) + "C; Power (" + std::to_string(test_it.target_power) + "W): current " +  Float_to_String<double>(device_info.Power_mW/1000.0, 1) + ", filtered " +  Float_to_String<double>(power_filt/1000.0,1) + "; Error: " + Float_to_String<double>(0.0-pwr_err/1000.0,1) + ", filtered " + Float_to_String<double>(0.0-pwr_err_filt/1000.0,1) + "; ";
                if (new_TR == true)
                    temp_str += "new TogRate: " + Float_to_String<double>(percent,1) + " %)";
                else
                    temp_str += "old TogRate: " + Float_to_String<double>(percent,1) + " %)";
                LogMessage(LOG_STATUS, temp_str);

            }
        }

        // test over, check if the target power has been reach and when
        // also check if the filtered power values are within the tolerance
        if ((StrMatchNoCase(TC_Cfg.test_sequence_mode, TEST_SEQUENCE_MODE_MEMBER_DUR_WATT)) && (m_abort == false))
        {
            // check if the target power has been reached
            if (power_reached == false)
            {
                LogMessage(LOG_ERROR, "Power test failed: could not reach the target power of " + std::to_string(test_it.target_power) + "W +/-" + Float_to_String<double>(power_tol_mW/1000.0,1) + "W");
                test_it_failure = true;
            }
            else
            {
                if (TC_Cfg.pwr_target_reach_time != DISABLE_PWR_TARGET_REACH_TIME_CHECK)
                {
                    if (power_reach_time <= TC_Cfg.pwr_target_reach_time)
                    {
                        LogMessage(LOG_PASS, "Target power reached before the limit: " + std::to_string(test_it.target_power) + "W +/-" + Float_to_String<double>(power_tol_mW/1000.0,1) + "W, in " + std::to_string(power_reach_time) + " s < " + std::to_string(TC_Cfg.pwr_target_reach_time) + "s");
                    }
                    else
                    {
                        LogMessage(LOG_ERROR, "Target power reached AFTER the limit: " + std::to_string(test_it.target_power) + "W +/-" + Float_to_String<double>(power_tol_mW/1000.0,1) + "W, in " + std::to_string(power_reach_time) + "s > " + std::to_string(TC_Cfg.pwr_target_reach_time) + "s");
                        test_it_failure = true;
                    }
                }
                else
                {
                        LogMessage(LOG_INFO, "Target power reached: " + std::to_string(test_it.target_power) + "W +/-" + Float_to_String<double>(power_tol_mW/1000.0,1) + "W  in " + std::to_string(power_reach_time) + " seconds");
                }

                //check if the power stays within the tolerance
                power_tol_cnt_check = CheckPowerRange(power_in_tol_cnt, power_out_tol_cnt);
                if (power_tol_cnt_check >= TC_Cfg.power_stability_tol)
                {
                    std::string temp_str = "More than " + std::to_string(TC_Cfg.power_stability_tol) + "% of filtered power values are inside the tolerance: " +  std::to_string(power_in_tol_cnt) + " of " + std::to_string(power_in_tol_cnt + power_out_tol_cnt) + " values (" + std::to_string(power_tol_cnt_check)  + "%) are in " + Float_to_String<double>(power_tol_mW/1000.0,1) + "W of tolerance";
                    LogMessage(LOG_PASS, temp_str);
                }
                else
                {
                    std::string temp_str = std::to_string(TC_Cfg.power_stability_tol) + "% Tolerance specification is not met. Only " + std::to_string(power_in_tol_cnt) + " of " + std::to_string(power_in_tol_cnt + power_out_tol_cnt) + " filtered power values (" + std::to_string(power_tol_cnt_check)  + "%) are inside the " + Float_to_String<double>(power_tol_mW/1000.0,1) + "W of tolerance";
                    LogMessage(LOG_ERROR, temp_str);
                    test_it_failure = true;
                }
            }

            raw_power_tol_cnt_check = CheckPowerRange(raw_power_in_tol_cnt, raw_power_out_tol_cnt);
            std::string temp_str = std::to_string(raw_power_tol_cnt_check) + "% of raw power values within " + Float_to_String<double>(power_tol_mW/1000.0,1) + "W tolerance: " +  std::to_string(raw_power_in_tol_cnt) + " of " + std::to_string(raw_power_in_tol_cnt+raw_power_out_tol_cnt) + " values";

            LogMessage(LOG_INFO, temp_str);

        }

        LogMessage(LOG_INFO, "End Test: " + std::to_string(test_it_cnt));

        test_failure |= (test_it_failure || m_abort);

        test_it_cnt++;

    }

    LogMessage(LOG_INFO, "Stop Power Compute units gradually");
    double step_down;
    for (int jj = 3; jj>=0; jj--)
    {
        step_down = jj*percent/4;
        SetClkThrottle( Percent2Throttle(step_down), true );
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        //MeasPwr(1, &device_info, step_down, NORMAL_POWER);
    }
    StopPowerKernel();

    #if USE_SOCKET

        if (m_client_socket > 0)
        {
            // close the connection
            close(m_client_socket);
        }

    #endif

    // check for overall test failure
    if (m_abort == true)
    {
        ret = -1;
    }
    else if (test_failure == true)
    {
        ret = 1;
    }

    return ret;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool PowerTest::StartTestAndEnableWatchdog()
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

    for (int kernel_idx=0; kernel_idx<m_num_krnls && (krnl_already_started == false); kernel_idx++)
    {
        m_device->WritePwrKernel(kernel_idx, CMN_CTRL_STATUS_ADDR, CMN_STATUS_START);
        read_data = m_device->ReadPwrKernel(kernel_idx, CMN_CTRL_STATUS_ADDR);
        if ((read_data & CMN_STATUS_ALREADY_START) == CMN_STATUS_ALREADY_START)
        {

            read_data = m_device->ReadPwrKernel(kernel_idx, CMN_WATCHDOG_ADDR);
            // check if watchdog is already enable and error is detected
            if ( ((read_data & CMN_WATCHDOG_EN) == CMN_WATCHDOG_EN) && ((read_data & CMN_WATCHDOG_ALARM) == CMN_WATCHDOG_ALARM) )
            {
                LogMessage(LOG_CRIT_WARN,"Watchdog has been triggered during previous test (power CU " + std::to_string(kernel_idx) + ") but start this test");
                // it's safe to restart the kernel, but first clear the start bit and the watchdog
                // stop the kernel
                m_device->WritePwrKernel(kernel_idx, CMN_CTRL_STATUS_ADDR, 0x0); // this also clears the alreay_start bit
                // stop watchdog and clear error
                m_device->WritePwrKernel(kernel_idx, CMN_WATCHDOG_ADDR, CMN_WATCHDOG_ALARM);

                // start the test
                m_device->WritePwrKernel(kernel_idx, CMN_CTRL_STATUS_ADDR, CMN_STATUS_START);
            }
            else
            {
                LogMessage(LOG_ERROR,"Test already running on power CU " + std::to_string(kernel_idx) + ". By trying to start another test, this may cause error(s) in currently running test. If no tests are running, you card is maybe in unkwown state, first re-validate it, then try xbtest again");
                krnl_already_started = true;
            }

        }
    }

    for (int kernel_idx=0; kernel_idx<m_num_krnls; kernel_idx++)
    {
        read_data = m_device->ReadPwrKernel(kernel_idx, CMN_WATCHDOG_ADDR);
        if ((read_data & CMN_WATCHDOG_ALARM) == CMN_WATCHDOG_ALARM)
        {
            LogMessage(LOG_WARN,"Watchdog has been triggered during previous test (power CU " + std::to_string(kernel_idx) + ").");
        }
    }

    // enable the watchdog if the kernel was't started
    if (krnl_already_started == false)
    {
        for (int kernel_idx=0; kernel_idx<m_num_krnls; kernel_idx++)
        {
            // start watchdog and clear any previous alarm
            read_data = CMN_WATCHDOG_EN | CMN_WATCHDOG_ALARM;
            m_device->WritePwrKernel(kernel_idx, CMN_WATCHDOG_ADDR, read_data);
        }
    }

    return krnl_already_started;
}

bool PowerTest::StopTestAndDisableWatchdog()
{
    uint read_data;
    bool error = false;

    // stop the kernel and check if the "already started" is present,
    // meanign that another test tried to start teh kernl too

    for (int kernel_idx=0; kernel_idx<m_num_krnls; kernel_idx++)
    {
        read_data = m_device->ReadPwrKernel(kernel_idx, CMN_CTRL_STATUS_ADDR);
        if ((read_data & CMN_STATUS_ALREADY_START) == CMN_STATUS_ALREADY_START)
        {
            LogMessage(LOG_ERROR,"Another test tried to access power CU " + std::to_string(kernel_idx) + "). This may have caused error to this test");
            error = true;
        }
        // stop the kernel
        m_device->WritePwrKernel(kernel_idx, CMN_CTRL_STATUS_ADDR, 0x0);
    }

    // disable the watchdog
    for (int kernel_idx=0; kernel_idx<m_num_krnls; kernel_idx++)
    {
        read_data = m_device->ReadPwrKernel(kernel_idx, CMN_WATCHDOG_ADDR);
        if ((read_data & CMN_WATCHDOG_ALARM) == CMN_WATCHDOG_ALARM)
        {
            LogMessage(LOG_ERROR,"Watchdog alarm detected (power CU " + std::to_string(kernel_idx) + "). This may have caused error to this test");
            error = true;
        }
        // disable watchdog and clear any alarm detected
        m_device->WritePwrKernel(kernel_idx, CMN_WATCHDOG_ADDR, CMN_WATCHDOG_ALARM);
    }

    return error;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void PowerTest::Run()
{
    Json_Parameters_t::iterator it;

    m_state     = TestState::TS_RUNNING;
    m_result    = TestResult::TR_FAILED;

    PowerTestcaseCfg_t TC_Cfg;

    LogMessage(LOG_STATUS, "Run");

    // parse testcase parameters
    bool global_settings_failure = false;

    // Get verbosity if defined in JSON, else use top level one
    int verbosity = (int)m_global_config.verbosity;
    bool verbos_ret = GetVerbosity(&(m_test_parameters.param), &verbosity);
    if (verbos_ret == true)
        LogMessage(LOG_FAILURE, VERBOSITY_FAILURE);
    global_settings_failure |= verbos_ret;
    m_global_config.verbosity = static_cast<LogLevel>(verbosity);


    m_min_power = m_xbtest_pfm_def.physical.power.power_target.min;
    m_max_power = m_xbtest_pfm_def.physical.power.power_target.max;

    // Test parameters
    global_settings_failure |= GetJsonParamStr(TEST_SOURCE_MEMBER,          SUPPORTED_TEST_SOURCE,          &(TC_Cfg.test_source),          TEST_SOURCE_MEMBER_JSON);
    global_settings_failure |= GetJsonParamStr(TEST_SEQUENCE_MODE_MEMBER,   SUPPORTED_TEST_SEQUENCE_MODE,   &(TC_Cfg.test_sequence_mode),   TEST_SEQUENCE_MODE_MEMBER_DUR_WATT);

    // macro enable
    global_settings_failure |= GetJsonParamBool(POWER_ENABLE_REG_MEMBER,  &(TC_Cfg.mode_enable_REG),  true);
    global_settings_failure |= GetJsonParamBool(POWER_ENABLE_DSP_MEMBER,  &(TC_Cfg.mode_enable_DSP),  true);
    global_settings_failure |= GetJsonParamBool(POWER_ENABLE_BRAM_MEMBER, &(TC_Cfg.mode_enable_BRAM), true);
    global_settings_failure |= GetJsonParamBool(POWER_ENABLE_URAM_MEMBER, &(TC_Cfg.mode_enable_URAM), true);

    if (TC_Cfg.mode_enable_REG == false)    LogMessage(LOG_WARN, "All FF's are disabled, they won't be used to consume power");
    if (TC_Cfg.mode_enable_DSP == false)    LogMessage(LOG_WARN, "All DSP's are disabled, they won't be used to consume power");
    if (TC_Cfg.mode_enable_BRAM == false)   LogMessage(LOG_WARN, "All BRAM's are disabled, they won't be used to consume power");
    if (TC_Cfg.mode_enable_URAM == false)   LogMessage(LOG_WARN, "All URAM's are disabled, they won't be used to consume power");

    // power stability tolerance
    global_settings_failure |= GetJsonParamNum<int>(POWER_STABILITY_TOL_MEMBER, MIN_POWER_STABILITY_TOL, NOM_POWER_STABILITY_TOL, MAX_POWER_STABILITY_TOL, &(TC_Cfg.power_stability_tol));
    if (TC_Cfg.power_stability_tol != NOM_POWER_STABILITY_TOL)
    {
        LogMessage(LOG_WARN, "Power stability tolerance overwritten to " + std::to_string(TC_Cfg.power_stability_tol) + "%");
    }

    // min power tolerance
    global_settings_failure |= GetJsonParamNum<uint>(POWER_TOLERANCE_MEMBER, MIN_POWER_TOLERANCE, NOM_POWER_TOLERANCE, MAX_POWER_TOLERANCE, &(m_min_power_toreance));
    if (m_min_power_toreance != NOM_POWER_TOLERANCE)
    {
        LogMessage(LOG_WARN, "Minimum power tolerance overwritten to " + std::to_string(m_min_power_toreance) + "W");
    }

    // target power reach time
    global_settings_failure |= GetJsonParamNum<uint>(POWER_TARGET_REACH_TIME_MEMBER, 0, DISABLE_PWR_TARGET_REACH_TIME_CHECK, MAX_UINT_VAL, &(TC_Cfg.pwr_target_reach_time));
    if (TC_Cfg.pwr_target_reach_time != DISABLE_PWR_TARGET_REACH_TIME_CHECK)
    {
        LogMessage(LOG_WARN, "Target power reach time verification is enabled");
    }

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
        m_outputfile << "Raw power"         << ",";
        m_outputfile << "Filtered power"    << ",";
        m_outputfile << "Target power"      << ",";
        m_outputfile << "Toggle Rate"       << ",";
        m_outputfile << "Error"             << ",";
        m_outputfile << "Error Filtered"    << ",";
        m_outputfile << "\n";
        m_outputfile.flush();
    }

    // use leakage model
    global_settings_failure |= GetJsonParamBool(POWER_USE_LEAKAGE_MODEL_MEMBER,  &(TC_Cfg.use_leakage_model),  false);
    if (TC_Cfg.use_leakage_model == true)
    {
        LogMessage(LOG_WARN, "Include leakage model when regulating the power consumed");

        // open loop?
        global_settings_failure |= GetJsonParamBool(POWER_OPEN_LOOP_MEMBER,  &(m_open_loop),  false);
        if (m_open_loop == true)    LogMessage(LOG_CRIT_WARN, "OPEN LOOP mode, no control of the power");
    }

    // num_leakage_calib
    m_leakage_use_fan_ctrl_file  = false;
    global_settings_failure |= GetJsonParamNum<uint>(POWER_NUM_LEAKAGE_CALIBRATION_MEMBER, 0, 0, MAX_UINT_VAL, &(TC_Cfg.num_leakage_calib));
    if (TC_Cfg.num_leakage_calib != 0)
    {
        LogMessage(LOG_WARN, "Leakage calibration is enabled for " + std::to_string(TC_Cfg.num_leakage_calib) + " trials");

        // num_leakage_calib
        global_settings_failure |= GetJsonParamNum<uint>(POWER_LEAKAGE_CALIBRATION_TIMEOUT_MEMBER, 0, 60, MAX_UINT_VAL, &(TC_Cfg.leakage_calib_timeout));
        LogMessage(LOG_WARN, "Leakage calibration timeout is " + std::to_string(TC_Cfg.leakage_calib_timeout) + " sec");

        // fan control files
        it = FindJsonParam(&(m_test_parameters.param), POWER_SET_FAN_MAX_FILE_MEMBER);
        if (it != m_test_parameters.param.end())
        {
            m_fan_max_file_name = TestcaseParamCast<std::string>(it->second);
            it = FindJsonParam(&(m_test_parameters.param), POWER_SET_FAN_MIN_FILE_MEMBER);
            if (it != m_test_parameters.param.end())
            {
                m_fan_min_file_name = TestcaseParamCast<std::string>(it->second);
                m_leakage_use_fan_ctrl_file  = true;
                LogMessage(LOG_WARN, "Use fan control files");
            }
        }

        // temperature range
        global_settings_failure |= GetJsonParamNum<int>(POWER_LEAKAGE_CALIBRATION_LOW_TEMP_MEMBER, 0, 0, m_xbtest_pfm_def.physical.thermal.temp_sources[0].limit, &(TC_Cfg.leakage_calib_low_temp));
        if (TC_Cfg.leakage_calib_low_temp != 0)
            LogMessage(LOG_WARN, "Leakage calibration low temperature " + std::to_string(TC_Cfg.leakage_calib_low_temp) + " C");

        global_settings_failure |= GetJsonParamNum<int>(POWER_LEAKAGE_CALIBRATION_HIGH_TEMP_MEMBER, 0, 0, m_xbtest_pfm_def.physical.thermal.temp_sources[0].limit, &(TC_Cfg.leakage_calib_high_temp));
        if (TC_Cfg.leakage_calib_high_temp != 0)
            LogMessage(LOG_WARN, "Leakage calibration high temperature " + std::to_string(TC_Cfg.leakage_calib_high_temp) + " C");


        it = FindJsonParam(&(m_test_parameters.param), POWER_LEAKAGE_CALIBRATION_RESULT_FILE_MEMBER);
        if (it != m_test_parameters.param.end())
        {
            m_leak_calib_outputfile_name = TestcaseParamCast<std::string>(it->second);
            m_use_leak_calib_outputfile  = true;

            global_settings_failure |= OpenOutputFile(m_leak_calib_outputfile_name + ".csv", &m_leak_calib_outputfile );

            m_leak_calib_outputfile << "Idle Power"   << ",";
            std::string col_name;
            for (uint i=0; i<m_xbtest_pfm_def.physical.thermal.num_temp_sources; i++)
            {
                col_name = "Temperature[" + std::to_string(i) + "]";
                m_leak_calib_outputfile << col_name << ",";
            }

            m_leak_calib_outputfile << "Raw power"          << ",";
            m_leak_calib_outputfile << "Avg Temp"           << ",";
            m_leak_calib_outputfile << "Avg Power"          << ",";
            m_leak_calib_outputfile << "expo fit a"         << ",";
            m_leak_calib_outputfile << "expo fit b"         << ",";
            m_leak_calib_outputfile << "expo fit c"         << ",";
            m_leak_calib_outputfile << "Least Square Err"   << ",";
            m_leak_calib_outputfile << "\n";
            m_leak_calib_outputfile.flush();
        }
    }

    // skip power calibration if defined in test.json
    m_power_calibration = 0;
    global_settings_failure |= GetJsonParamNum<uint>(POWER_PWR_CALIBRATION_MEMBER, 0, 0, MAX_UINT_VAL, &(m_power_calibration));
    if (m_power_calibration != 0)
    {
        LogMessage(LOG_WARN, "power calibration skipped and use user-deined value of " + std::to_string(m_power_calibration) + "W");
    }

    // skip power calibration if defined in test.json
    m_pwr_err_filter_alpha = 3;
    global_settings_failure |= GetJsonParamNum<uint>(POWER_PWR_FILTER_ALPHA_MEMBER, 1, 3, MAX_UINT_VAL, &(m_pwr_err_filter_alpha));
    if (m_pwr_err_filter_alpha != 3)
    {
        LogMessage(LOG_WARN, "power filter alpha: " + std::to_string(m_pwr_err_filter_alpha));
    }

    int thread_state = 1;
    bool parse_failure = false;

    if (global_settings_failure == true) m_abort = true;

    if (m_abort == false)
    {
        LogMessage(LOG_INFO, "Test parameters:"                                                                               );
        LogMessage(LOG_INFO, "\t- " + std::string(TEST_SOURCE_MEMBER.name)            + ": " + TC_Cfg.test_source                  );
        LogMessage(LOG_INFO, "\t- " + std::string(TEST_SEQUENCE_MODE_MEMBER.name)     + ": " + TC_Cfg.test_sequence_mode           );

        LogMessage(LOG_INFO, "Start checking test sequence parameters" );
        parse_failure = ParseTestSequenceSettings(TC_Cfg, &m_test_it_list);

        if (m_abort == false)
        {
            if (parse_failure == false)
            {
                LogMessage(LOG_PASS, "Checking test parameters finished" );
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
                m_thread_future = std::async(std::launch::async, &PowerTest::RunThread, this, TC_Cfg, &m_test_it_list);
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

bool PowerTest::ParseTestSequenceSettings( PowerTestcaseCfg_t TC_Cfg, std::list<TestItConfig_t> *test_list )
{
    bool parse_failure = false;
    uint  parse_error_cnt = 0;
    int  test_cnt = 0;
    TestItConfig_t test_it_cfg;

    std::vector<Power_Test_Sequence_Parameters_t> test_sequence;
    Json_Parameters_t::iterator it = FindJsonParam(&(m_test_parameters.param), TEST_SEQUENCE_MEMBER);
    if (it != m_test_parameters.param.end())
        test_sequence = TestcaseParamCast<std::vector<Power_Test_Sequence_Parameters_t>>(it->second);

    for (auto test_seq_param : test_sequence)
    {
        if (m_abort == true) break;
        test_cnt ++;
        bool parse_it_failure = false;

        if (parse_it_failure == false)
        {
            parse_it_failure |= CheckTime(test_seq_param.duration);
            test_it_cfg.duration = test_seq_param.duration;
        }
        if (parse_it_failure == false)
        {
            if (StrMatchNoCase(TC_Cfg.test_sequence_mode, TEST_SEQUENCE_MODE_MEMBER_DUR_TOG))
            {
                parse_it_failure |= CheckToggleRate(test_seq_param.power_toggle);
                test_it_cfg.toggle_rate = test_seq_param.power_toggle;
                test_it_cfg.target_power = 0;
            }
            else
            {
                parse_it_failure |= CheckTargetPower(test_seq_param.power_toggle);
                test_it_cfg.target_power = test_seq_param.power_toggle;
                test_it_cfg.toggle_rate = 0;
            }
        }

        parse_failure |= parse_it_failure;

        if (parse_it_failure == true)
        {
            LogMessage(LOG_FAILURE, "Test "+ std::to_string(test_cnt) + ": invalid parameters" );
            parse_error_cnt ++;
            if (parse_error_cnt > MAX_NUM_PARSER_ERROR) break;
        }
        else
        {
            test_list->push_back(test_it_cfg);
            std::string params = "";
            params += std::to_string(test_it_cfg.duration) + ", ";

            if (StrMatchNoCase(TC_Cfg.test_sequence_mode, TEST_SEQUENCE_MODE_MEMBER_DUR_TOG))
                params += std::to_string(test_it_cfg.toggle_rate);
            else
                params += std::to_string(test_it_cfg.target_power);
            LogMessage(LOG_DEBUG, "Test " + std::to_string(test_cnt) + " parameters: " + params);
        }
    }

    return parse_failure;
}
