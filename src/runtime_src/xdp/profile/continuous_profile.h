/**
 * Copyright (C) 2016-2017 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#ifndef __XILINX_CONTINUOUS_PROFILING
#define __XILINX_CONTINUOUS_PROFILING

#include "rt_profile_writers.h"
#include "xrt/device/hal2.h"

#include <unordered_map>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <iostream>

namespace XCL {

class BaseMonitor {
public:
	BaseMonitor() {};
	virtual std::string get_id() = 0;
	virtual void launch() = 0;
	virtual void terminate() = 0;
	virtual ~BaseMonitor() {};
};

class ThreadMonitor : public BaseMonitor {
public:
	std::string get_id() override {return "thread_monitor";};
	void launch();
	void terminate();
	virtual ~ThreadMonitor();
protected:
	virtual void thread_func(int id) {};
	virtual void willLaunch() {};
	virtual void setLaunch() {};
	virtual void didLaunch() {};
	virtual void willTerminate() {};
	virtual void setTerminate() {};
	virtual void didTerminate() {};
private:
	std::thread monitor_thread;
};

class SamplingMonitor : public ThreadMonitor {
public:
	SamplingMonitor(int freq_in=1) : ThreadMonitor(),
			should_continue(false), sample_freq(freq_in) {};
	std::string get_id() override {return "sampling_monitor";};
protected:
	void thread_func(int id) override;
	void setLaunch() override;
	void setTerminate() override;
	virtual bool shoudEarlyTerminate() {return false;};
	virtual void willSampleOnce() {};
	virtual void sampleOnce() {};
	virtual void didSampleOnce() {};
	virtual void willSample() {};
	virtual void didSample() {};
	virtual void willPause() {};
	virtual void didPause() {};
private:
	std::mutex status_guard;
	bool should_continue;
	int sample_freq;
};

class PowerMonitor : public SamplingMonitor {
public:
	PowerMonitor(std::string dump_filename_in, int freq_in, int device_idx, std::string logfile):SamplingMonitor(freq_in) {
		dump_filename = dump_filename_in;
		dev = xrt::hal2::device(logfile.c_str(), );
	}
	std::string get_id() {return "power_monitor";};
private:
	std::unordered_map<std::string, float> readPowerStatus();
	float getFakeReading(int HI, int LO);
	void outputPowerStatus(std::unordered_map<std::string, float>& status);
protected:
	void sampleOnce() override;
	void willLaunch() override;
	void didTerminate() override;
private:
	std::string dump_filename;
	std::ofstream power_dump_file;
	xrt::hal2::device dev;
};

class ContinuousProfile {
public:
	ContinuousProfile();
	~ContinuousProfile();
	void launchMonitors(std::vector<BaseMonitor*> monitors);
	void terminateMonitors(std::vector<std::string>& monitor_ids);
private:
	std::unordered_map<std::string, BaseMonitor*> monitor_dict;
};

}

#endif
