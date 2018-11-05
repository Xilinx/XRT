#ifndef __INPUT_VALIDATION__
#define __INPUT_VALIDATION__

#include <unordered_map>
#include <iostream>

#include "device_meta.h"

using namespace std;

void validate_device_index(int device_index, int device_cnt);

bool device_exist(unordered_map<string, DeviceMeta *> &dict, string device_name);

void check_can_open_device(unordered_map<string, DeviceMeta *> &dict, string device_name);

void check_can_close_device(unordered_map<string, DeviceMeta *> &dict, string device_name);

void check_can_reset_device(unordered_map<string, DeviceMeta *> &dict, string device_name);

void check_can_get_device_info(unordered_map<string, DeviceMeta *> &dict, string device_name);

void check_can_reclock_device(unordered_map<string, DeviceMeta *> &dict, string device_name);

void check_can_lock_device(unordered_map<string, DeviceMeta *> &dict, string device_name);

void check_can_unlock_device(unordered_map<string, DeviceMeta *> &dict, string device_name);

void check_can_get_device_usage(unordered_map<string, DeviceMeta *> &dict, string device_name);

void check_can_get_device_error(unordered_map<string, DeviceMeta *> &dict, string device_name);

void check_can_allocate_buffer(unordered_map<string, DeviceMeta *> &dict, string device_name);

#endif