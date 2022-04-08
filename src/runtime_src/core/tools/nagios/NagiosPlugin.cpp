#include <iostream>
#include <xrt/xrt_device.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <sstream>
#include <string>

int main(int argc, char* argv[])
{
	// Initial state is OK
	// If any issues occur below the value will be set to 1 or 2
	int status = 0;
	std::string bdf(argv[1]);
	// TODO device id or just deivce bdf should be a passed in parameter
	//
	auto device = xrt::device(bdf);

	std::stringstream output;
	
	// Process thermal data
	auto data = device.get_info<xrt::info::device::thermal>();
	std::stringstream ss;
	ss << data;
	boost::property_tree::ptree pt;
	boost::property_tree::read_json(ss, pt);
	auto thermal_pt = pt.get_child("thermals");
	output << "Thermals: " << std::endl;
	for (auto& sensor : thermal_pt) {
		// Boxes around actual thermal data
		if (sensor.second.get<bool>("is_present")) {
			auto loc = sensor.second.get<std::string>("location_id");
			auto temp = sensor.second.get<double>("temp_C"); 
			output << "{" << loc << ", " << temp << "C}" << std::endl;
			// Generate a warnings when exceeding operating temp
			// 45.0 came from xilinx documentation for u200 and u250 cards operating parameters
			if(temp > 45.0) {
				output << "OVERTEMP: " << loc << " TEMP: " << temp << std::endl;
				status = 1;
			}
		}
	}

	// Process mechanical
	data = device.get_info<xrt::info::device::mechanical>();
	ss.str("");
	ss << data;
	pt.clear();
	boost::property_tree::read_json(ss, pt);
	auto fans_pt = pt.get_child("fans");
	output << "Fans: " << std::endl;
	for (auto& fan : fans_pt) {
		// Boxes around actual mechanical data
		if (fan.second.get<bool>("is_present")) {
			auto loc = fan.second.get<std::string>("location_id");
		        auto speed = fan.second.get<int>("speed_rpm");
			output << "{" << loc << ", " << speed << "RPM}" << std::endl;;
			// Generate a warning if speed is very high?
		}
	}

	// Process electrical
	data = device.get_info<xrt::info::device::electrical>();
	ss.str("");
	ss << data;
	pt.clear();
	boost::property_tree::read_json(ss, pt);
			
	auto max_power = pt.get<double>("power_consumption_max_watts");
	auto cur_power = pt.get<double>("power_consumption_watts");
	auto power_warning = pt.get<bool>("power_consumption_warning");
	output << "Power: " << std::endl;
	output << "{Max Power: " << max_power <<  "W, Current Power: " << cur_power << "W}" << std::endl;;
	// Generate a warning if required
	if (power_warning)
		status = 1;
	if (max_power <= cur_power)
		status = 2;

	// Process memory
	data = device.get_info<xrt::info::device::memory>();
	ss.str("");
	ss << data;
	pt.clear();
	boost::property_tree::read_json(ss, pt);
	// Not all cards have memory banks
	if (!pt.empty()) {
		boost::property_tree::ptree memory_pt = pt.get_child("board.memory.memories");
		output << "Memory Banks: " << std::endl;
		for (auto& child : memory_pt) {
			// Memory tree
			if (child.second.get<bool>("enabled")) {
				auto type = child.second.get<std::string>("type");
				auto tag = child.second.get<std::string>("tag");
			        auto range_bytes_str = child.second.get<std::string>("range_bytes");
				auto range_bytes = stoull(range_bytes_str, nullptr, 16);
				auto addr = child.second.get<std::string>("base_address");
				output << "{" << tag << ", Type: " << type << ", Address: " << addr;
				// If no memory can be allocated do not present a utiliziation report
				if (range_bytes > 0) {
					auto bytes = child.second.get<uint64_t>("extended_info.usage.allocated_bytes");
					double utilization = ((bytes * 100.0)/range_bytes);
					output << ", Memory Utilization: " << utilization << "%, ";
					auto temp = child.second.get<double>("extended_info.temperature_C");
					output << "Temp: " << temp << "C";
					// Generate a warning if utilization is high?
				}
				output << "}" << std::endl;
			}
		}
	}

	// Output status before all other data
	switch (status) {
		case 0:
			std::cout << "STATUS: OK" << std::endl;
			break;
		case 1:
			std::cout << "STATUS: WARNING" << std::endl;
			break;
		default:
			std::cout << "STATUS: FAILURE" << std::endl;
			break;
	}
	std::cout << output.str();
	return status;
}

