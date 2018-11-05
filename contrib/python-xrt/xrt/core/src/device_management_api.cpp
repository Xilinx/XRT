#include "device_management_api.h"

int probe_devices()
{
	int device_cnt = xclProbe();
	known_device_cnt = device_cnt;
	return device_cnt;
}

void open_device(int device_index, string device_name, string verbosity_level)
{
	validate_device_index(device_index, known_device_cnt);
	check_can_open_device(device_dict, device_name);
	xclVerbosityLevel level = convert_verbosity_level(verbosity_level);
	string log_filename = generate_log_filename(device_name);
	xclDeviceHandle handle = xclOpen(device_index, log_filename.c_str(), level);
	DeviceMeta *meta = new DeviceMeta();
	meta->handle = handle;
	meta->id = device_name;
	meta->locked = false;
	device_dict[device_name] = meta;
}

void close_device(string device_name)
{
	check_can_close_device(device_dict, device_name);
	xclClose(device_dict[device_name]->handle);
	device_dict.erase(device_name);
}

void reset_device(string device_name, string reset_type)
{
	check_can_reset_device(device_dict, device_name);
	xclResetKind type = convert_reset_kind(reset_type);
	xclResetDevice(device_dict[device_name]->handle, type);
}

py::dict get_device_info(string device_name)
{
	check_can_get_device_info(device_dict, device_name);
	xclDeviceInfo2 *info = new xclDeviceInfo2();
	int res = xclGetDeviceInfo2(device_dict[device_name]->handle, info);
	check_get_device_info_error(res);
	py::dict res_info = convert_device_info(info);
	delete info;
	return res_info;
}

void reclock_device(string device_name, int target_region, int target_freq)
{
	check_can_reclock_device(device_dict, device_name);
	const unsigned short freq = (unsigned short)target_freq;
	unsigned short region = (unsigned short)target_region;
	int res = xclReClock2(device_dict[device_name]->handle, region, &freq);
	check_reclock_device_error(res);
}

void lock_device(string device_name)
{
	check_can_lock_device(device_dict, device_name);
	int res = xclLockDevice(device_dict[device_name]->handle);
	check_lock_device_error(res);
	device_dict[device_name]->locked = true;
}

void unlock_device(string device_name)
{
	check_can_unlock_device(device_dict, device_name);
	int res = xclUnlockDevice(device_dict[device_name]->handle);
	check_unlock_device_error(res);
	device_dict[device_name]->locked = false;
}

py::dict load_bitstream(string device_name, string filename)
{
	std::ifstream bitstream(filename.c_str());
	if (!bitstream.is_open())
	{
		throw runtime_error("Cannot open bitstream file");
	}
	cout << "Bitsteam file opened successfully" << endl;
	bitstream.seekg(0, bitstream.end);
	int size = bitstream.tellg();
	cout << "Bitstream size: " << size << endl;
	bitstream.seekg(0, bitstream.beg);
	cout << "Allocating space for bitstream ..." << endl;
	char *header = new char[size];
	cout << "Space allocated" << endl;
	cout << "Reading bitstream ..." << endl;
	bitstream.read(header, size);
	cout << "Bitstream binary loaded into memory" << endl;
	if (std::strncmp(header, "xclbin2", 8))
	{
		throw std::runtime_error("Invalid bitstream file");
	}
	cout << "Casting binary to axlf format ..." << endl;
	const xclBin *blob = (const xclBin *)header;
	cout << "Axlf structure created" << endl;
	cout << "Loading bitstream into the device ..." << endl;
	if (xclLoadXclBin(device_dict[device_name]->handle, blob))
	{
		delete[] header;
		throw std::runtime_error("Bitstream download failed");
	}
	cout << "Bitstream loaded into device" << endl;
	cout << "Casting header binary to axlf header ..." << endl;
	const axlf *top = (const axlf *)header;
	cout << "Axlf header created" << endl;
	cout << "Fetching IP layout ..." << endl;
	auto ip = xclbin::get_axlf_section(top, IP_LAYOUT);
	cout << "IP Layout fetched" << endl;
	cout << "Casting binary to IP layout ..." << endl;
	struct ip_layout *layout = (ip_layout *)(header + ip->m_sectionOffset);
	cout << "IP layout created" << endl;
	py::dict res;
	py::dict ip_layout_list;
	cout << "Iterating over " << layout->m_count << " IP ..." << endl;
	for (int i = 0; i < layout->m_count; ++i)
	{
		py::dict ip;
		unsigned long cu_base_addr = layout->m_ip_data[i].m_base_address;
		unsigned cu_properties = layout->m_ip_data[i].properties;
		std::string cu_type = convert_ip_type((IP_TYPE)layout->m_ip_data[i].m_type);
		std::string cu_name = std::string((char *)layout->m_ip_data[i].m_name);
		ip["name"] = cu_name;
		ip["properties"] = cu_properties;
		ip["type"] = cu_type;
		ip["address"] = cu_base_addr;
		ip_layout_list[cu_name] = ip;
	}
	res["ip_layout"] = ip_layout_list;
	delete[] header;
	return res;
}

py::dict get_bitstream_info(string device_name, string type, int index)
{
	py::dict res;
	return res;
}

void open_context(string device_name, string xclbin_name, int ip_index, bool shared)
{
}

void close_context(string device_name, string xclbin_name, int ip_index)
{
}

void boot_device(string device_name)
{
	xclBootFPGA(device_dict[device_name]->handle);
}

int get_version()
{
	int version = (int)xclVersion();
	return version;
}

#if !defined(SW_EMU) && !defined(HW_EMU)

py::dict get_device_error(string device_name)
{
	check_can_get_device_error(device_dict, device_name);
	xclErrorStatus *info = new xclErrorStatus();
	int res = xclGetErrorStatus(device_dict[device_name]->handle, info);
	check_get_device_error_error(res);
	py::dict res_info = convert_error_info(info);
	return res_info;
}

py::dict get_device_usage(string device_name)
{
	check_can_get_device_usage(device_dict, device_name);
	xclDeviceUsage *info = new xclDeviceUsage();
	int res = xclGetUsageInfo(device_dict[device_name]->handle, info);
	check_get_device_usage_error(res);
	py::dict res_info = convert_usage_info(info);
	return res_info;
}

void remove_scan_fpga()
{
	xclRemoveAndScanFPGA();
}

#endif