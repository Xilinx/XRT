#include "input_validation.h"

void validate_device_index(int device_index, int device_cnt)
{
	if (device_index < 0)
	{
		throw runtime_error("Device index cannot be negative");
	}
	if (device_index >= device_cnt)
	{
		throw runtime_error("Device index cannot exceed total device count, probe first to discover devices");
	}
	return;
}

bool device_exist(unordered_map<string, DeviceMeta *> &dict, string device_name)
{
	auto find_res = dict.find(device_name);
	if (find_res != dict.end())
	{
		return true;
	}
	return false;
}

void check_can_open_device(unordered_map<string, DeviceMeta *> &dict, string device_name)
{
	if (device_exist(dict, device_name))
	{
		throw runtime_error("Device cannot be opened twice, close first");
	}
	return;
}

void check_can_close_device(unordered_map<string, DeviceMeta *> &dict, string device_name)
{
	if (!device_exist(dict, device_name))
	{
		throw runtime_error("Cannot close a device that does not exist");
	}
	return;
}

void check_can_reset_device(unordered_map<string, DeviceMeta *> &dict, string device_name)
{
	if (!device_exist(dict, device_name))
	{
		throw runtime_error("Cannot reset a device that does not exist");
	}
	return;
}

void check_can_get_device_info(unordered_map<string, DeviceMeta *> &dict, string device_name)
{
	if (!device_exist(dict, device_name))
	{
		throw runtime_error("Cannot get device info. Device not exist");
	}
	return;
}

void check_can_reclock_device(unordered_map<string, DeviceMeta *> &dict, string device_name)
{
	if (!device_exist(dict, device_name))
	{
		throw runtime_error("Cannot reclock device. Device not exist");
	}
	return;
}

void check_can_lock_device(unordered_map<string, DeviceMeta *> &dict, string device_name)
{
	if (!device_exist(dict, device_name))
	{
		throw runtime_error("Cannot lock device. Device not exist");
	}
	if (dict[device_name]->locked)
	{
		throw runtime_error("Cannot lock device. Device already locked");
	}
	return;
}

void check_can_unlock_device(unordered_map<string, DeviceMeta *> &dict, string device_name)
{
	if (!device_exist(dict, device_name))
	{
		throw runtime_error("Cannot lock device. Device not exist");
	}
	if (!dict[device_name]->locked)
	{
		throw runtime_error("Cannot lock device. Device already unlocked");
	}
	return;
}

void check_can_get_device_usage(unordered_map<string, DeviceMeta *> &dict, string device_name)
{
	if (!device_exist(dict, device_name))
	{
		throw runtime_error("Cannot get device usage. Device not exist");
	}
	return;
}

void check_can_get_device_error(unordered_map<string, DeviceMeta *> &dict, string device_name)
{
	if (!device_exist(dict, device_name))
	{
		throw runtime_error("Cannot get device error. Device not exist");
	}
	return;
}

void check_can_allocate_buffer(unordered_map<string, DeviceMeta *> &dict, string device_name) {
	if (!device_exist(dict, device_name))
	{
		throw runtime_error("Cannot allocate buffer on this device. Device not exist");
	}
	return;
}