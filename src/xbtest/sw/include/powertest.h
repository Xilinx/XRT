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

#ifndef _POWERTEST_H
#define _POWERTEST_H

#include "xbtestcommon.h"
#include "logging.h"
#include "deviceinterface.h"
#include "testinterface.h"
#include "devicemgt.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>

#define USE_SOCKET false


class PowerTest : public TestInterface
{

public:

    PowerTest( Xbtest_Pfm_Def_t xbtest_pfm_def, DeviceInterface *device, DeviceMgt *device_mgt, Testcase_Parameters_t test_parameters, Global_Config_t global_config );
    ~PowerTest();

    // implement virtual inherited functions
    bool PreSetup();
    void Run();
    void PostTeardown();
    void Abort();

private:
    DeviceInterface *m_device;
    Xbtest_Pfm_Def_t m_xbtest_pfm_def;
    DeviceMgt *m_devicemgt = nullptr;

    void WaitSecTick(uint quantity);

    // generic
    std::atomic<bool> m_abort;
    std::future<int> m_thread_future;

    int m_num_krnls;

    typedef struct PowerTestcaseCfg_t
    {
        std::string test_source;
        std::string test_sequence_mode;

        int         power_stability_tol;
        uint        pwr_target_reach_time;

        bool        use_leakage_model;
        uint        num_leakage_calib;
        uint        leakage_calib_timeout;
        int         leakage_calib_low_temp;
        int         leakage_calib_high_temp;

        bool        mode_enable_REG;
        bool        mode_enable_DSP;
        bool        mode_enable_BRAM;
        bool        mode_enable_URAM;
    } PowerTestcaseCfg_t;

    PowerTestcaseCfg_t m_Testcase_Cfg;

    typedef struct TestItConfig_t
    {
        uint duration;
        int target_power;
        int toggle_rate;
    } TestItConfig_t;

    int RunThread( PowerTestcaseCfg_t TC_Cfg, std::list<TestItConfig_t> *Tests_list );

    bool ParseTestSequenceSettings( PowerTestcaseCfg_t TC_Cfg, std::list<TestItConfig_t> *test_list );

    // enable/disable the various macro in order to dissipate power
    const uint PWR_CTRL_REG_ADDR = 0x20;

    const int QTY_THROTTLE_STEP = 512; // 9 bits
    const uint PWR_TOG_PAT_MASK = (2*QTY_THROTTLE_STEP)-1;

    const uint PWR_CTRL_ENABLE_REG   = (0x1 << 16);
    const uint PWR_CTRL_ENABLE_DSP   = (0x1 << 17);
    const uint PWR_CTRL_ENABLE_BRAM  = (0x1 << 18);
    const uint PWR_CTRL_ENABLE_URAM  = (0x1 << 19);

    const uint NUM_SEC_WATCHDOG      = 5; // max number of sec to wait between watch dog reset

    // m_min_power & m_max_power defined in xbtest_pfm_def.json: "power_target"
    int m_min_power;
    int m_max_power;


    const uint MAX_NUM_CONFIG_PARAM   = 2;

    const uint MIN_DURATION = 1;
    const uint MAX_DURATION = MAX_UINT_VAL;

    const uint DISABLE_PWR_TARGET_REACH_TIME_CHECK = 0;

    const int MIN_TOG = 0;
    const int MAX_TOG = 100;

    std::string m_outputfile_name;
    bool m_use_outputfile = false;
    std::ofstream m_outputfile;

    std::list<TestItConfig_t> m_test_it_list;

    void WaitPowerStable(uint duration, DeviceInterface::Device_Info *device_info, int target_power, double percent);

    int Percent2Throttle( double percent );
    double Throttle2Percent( int throttle );

    int m_previous_throttle = -1;
    bool SetClkThrottle( int throttle, bool force_write);

    void StopPowerKernel();
    void StartPowerKernel(PowerTestcaseCfg_t TC_Cfg);

    const int POWER_STABILSE_PERIOD = 5;

    // validation
    const double PWR_FILT_APLHA   = 0.66;
    //
    const int TOGGLE_VARIATION_LIMIT_EN = 3;

    const std::string TEST_SEQUENCE_MODE_MEMBER_DUR_WATT   = "config_duration_watts";
    const std::string TEST_SEQUENCE_MODE_MEMBER_DUR_TOG    = "config_duration_toggle";
    const std::set<std::string> SUPPORTED_TEST_SEQUENCE_MODE = {
        TEST_SEQUENCE_MODE_MEMBER_DUR_WATT,
        TEST_SEQUENCE_MODE_MEMBER_DUR_TOG
    };

    bool CheckTime( uint time );
    bool CheckTargetPower( int power );
    bool CheckToggleRate( int toggle_rate );

    int CheckPowerRange( int in_range, int out_range );
    double ComputePowerTolerance( int target_power );

    // by default, power tolerance = 2% with a minimum of 2W
    const double POWER_TOLERANCE_CRITERIA = 2.0; // %
    // minimum tolerance in W
    const uint MIN_POWER_TOLERANCE = 0;
    const uint NOM_POWER_TOLERANCE = 2;
    const uint MAX_POWER_TOLERANCE = MAX_UINT_VAL;
    uint m_min_power_toreance = NOM_POWER_TOLERANCE;

    // if more than 90% of the power measurement are in the Tolerance, the test pass
    // percentage: quantity of power value within the tolerance
    const int MIN_POWER_STABILITY_TOL = 0;
    const int NOM_POWER_STABILITY_TOL = 90;
    const int MAX_POWER_STABILITY_TOL = 100;

    // measurements file
    void WriteToMeasurementFile( DeviceInterface::Device_Info device_info, int power_target, double power_filter, double toggle_rate, double pwr_err, double pwr_err_filt);

    void GetPwrResources();

    typedef struct {
        uint num_slice;
        uint num_dsp48e2;
        uint num_ramb36;
        uint num_uram288;
    } krnl_resource;

    krnl_resource  m_pwr_resource;

    // from measurements @ 44C
    // from measurements @ 34C on a u250
    const double PWR_SLICE   = 0.0011596527;
    const double PWR_DSP     = 0.0091391509;
    const double PWR_BRAM    = 0.0402298851;
    const double PWR_URAM    = 0.0532894737;

    double GetPwrEstimation();
    double ComputeMaxPwr();
    void ClearPwrMeasList();
    void PowerCalibration(int num_sample, double max_percent, DeviceInterface::Device_Info *device_info, double idle_power);

    const bool CALIBRATION_POWER = true;
    const bool NORMAL_POWER      = false;


    typedef struct {
        double percent;
        double power;
    } meas;

    std::list<meas> m_pwr_meas_list;

    const bool HEAT_UP = true;
    const bool COOL_DOWN = false;

    typedef struct {
        int temp;
        double power;
    } meas_temp_pwr;

    const int MAX_CALIB_TEMP = 300;

    typedef struct {
        double a = 0.0;
        double b = 0.0;
        double c = 0.0;
        double err = 0.0;
    } leakage_exp_curve;

    meas_temp_pwr MeasPwr(uint duration, DeviceInterface::Device_Info *device_info, double percent, bool calibration);

    int LeakCalibReachTemp(int limit_temp, bool heatup, int timeout, DeviceInterface::Device_Info *device_info, std::list<meas_temp_pwr> *meas_list, double percent);

    double LeastSquareError(double a, double b, double c, std::list<meas_temp_pwr> meas_list);
    void CalibrationMeasSorting(double idle_power, std::list<meas_temp_pwr> *meas_list, std::list<meas_temp_pwr> *leak_list);
    void CalibrationExpFitting(leakage_exp_curve *leakage_curve, std::list<meas_temp_pwr> *leak_list);

    std::list<leakage_exp_curve> m_individual_leakage_curve_list;
    double LeakagePower(int temperature, leakage_exp_curve leakage_curve );
    double XPE_Leakage(int temperature);

    int ComputeThrottleOffset(double pwr_err, int temperature, double StaticAvailPower, bool power_clipping, leakage_exp_curve leakage_curve);
    int ComputeThrottleForPwr(double target_power, double idle_power, int temperature, double StaticAvailPower, leakage_exp_curve leakage_curve);

    FILE* m_pipe;

    std::string m_fan_max_file_name;
    std::string m_fan_min_file_name;
    bool m_leakage_use_fan_ctrl_file;
    bool SendFanCtrlfile(std::string fan_ctrl_file);

    std::string   m_leak_calib_outputfile_name;
    bool          m_use_leak_calib_outputfile;
    std::ofstream m_leak_calib_outputfile;
    void WriteToLeakCalibrationFile(double idle_pwr, int temperature, double raw_power, int avg_temp, double avg_power, double a, double b, double c, double error);

    uint m_pwr_err_filter_alpha;
    uint m_power_calibration = 0;

    bool m_open_loop = false;

    const leakage_exp_curve U250_XPE_LEAK_TEMP_EXT_PROC_MAX = {1.25916736180127, 0.798404389007309, 0.0300899434956833, 0.0};
    const leakage_exp_curve U250_XPE_LEAK_TEMP_IND_PROC_MAX = {0.961170921722603, 1.20681924552784, 0.0292895768551415, 0.0};

    void ResetWatchdog();
    bool StartTestAndEnableWatchdog();
    bool StopTestAndDisableWatchdog();

    bool OpenSocketClient(std::string host, uint port);
    int m_client_socket = -1;

};
#endif /* _POWERTEST_H */
