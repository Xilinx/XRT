#ifndef __DEVICE_META_CLASS__
#define __DEVICE_META_CLASS__

#include <string>
#include <unordered_map>

#include "xclhal2.h"

using namespace std;

struct BufferMeta
{
	int handle;
	int size;
	bool mapped;
	bool user_ptr;
	void* addr;
};

struct DeviceMeta
{
	string id;
	xclDeviceHandle handle;
	bool locked;
	unordered_map<int, BufferMeta> buffer_dict;
};

#endif
