#include "continuous_profile.h"
#include <stdlib.h>

#include <thread>
#include <chrono>

namespace XCL {

ThreadMonitor::~ThreadMonitor() {
	if (monitor_thread.joinable()) {
		terminate();
	}
}

void ThreadMonitor::launch() {
	willLaunch();
	setLaunch();
	monitor_thread = std::thread(&ThreadMonitor::thread_func, this, 0);
	didLaunch();
}

void ThreadMonitor::terminate() {
	willTerminate();
	setTerminate();
	monitor_thread.join();
	didTerminate();
}

void SamplingMonitor::setLaunch() {
	status_guard.lock();
	should_continue = true;
	status_guard.unlock();
}

void SamplingMonitor::setTerminate() {
	status_guard.lock();
	should_continue = false;
	status_guard.unlock();
}

void SamplingMonitor::thread_func(int id) {
	willSample();
	int interval = 1e6 / sample_freq;
	while (true) {
		status_guard.lock();
		bool continue_sample = should_continue;
		status_guard.unlock();
		if (continue_sample && !shoudEarlyTerminate()) {
			willSampleOnce();
			sampleOnce();
			didSampleOnce();
		} else {
			break;
		}
		willPause();
		std::this_thread::sleep_for (std::chrono::microseconds(interval));
		didPause();
	}
	didSample();
}

void PowerMonitor::sampleOnce() {
	auto status = readPowerStatus();
	outputPowerStatus(status);
}

void PowerMonitor::didTerminate() {
	power_dump_file.close();
}

float PowerMonitor::getFakeReading(int HI, int LO) {
	return LO+static_cast<float>(rand())/(static_cast<float>(RAND_MAX/(HI-LO)));
}

void PowerMonitor::willLaunch() {
	power_dump_file.open(dump_filename);
	power_dump_file << "Timestamp,FPGA Power Consumption,Board Power Consumption" << std::endl;
}

std::unordered_map<std::string, float> PowerMonitor::readPowerStatus() {
	std::unordered_map<std::string, float> res;
	float vccint = getFakeReading(1, 10);
	float vcc12v = getFakeReading(1, 10);
	float vcc12v_aux = getFakeReading(1, 10);
	float v3_aux = getFakeReading(1, 10);
	res["VCCINT"] = vccint;
	res["VCC12V"] = vcc12v;
	res["VCC12V_AUX"] = vcc12v_aux;
	res["V3_AUX"] = v3_aux;
	return res;
}

void PowerMonitor::outputPowerStatus(std::unordered_map<std::string, float>& status) {
	float FPGA_power = status["VCCINT"];
	float board_power = status["VCC12V"] + status["VCC12V_AUX"] + status["V3_AUX"] + FPGA_power;
	auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
	power_dump_file << timestamp << "," << FPGA_power << "," << board_power << std::endl;
}

}
