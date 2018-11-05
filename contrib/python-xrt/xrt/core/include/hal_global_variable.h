#ifndef __HAL_GLOBAL_VARIABLE__
#define __HAL_GLOBAL_VARIABLE__

#include "device_meta.h"

extern unordered_map<string, DeviceMeta *> device_dict;

extern int known_device_cnt;

#endif