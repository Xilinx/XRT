#include "buffer_management_api.h"

unsigned allocate_buffer(string device_name, string type, unsigned flags, unsigned size)
{
	check_can_allocate_buffer(device_dict, device_name);
	xclBOKind domain = convert_buffer_type(type);
	cout << "Allocating buffer with size: " << (size_t)size << " ..." << endl;
	cout << "Allocating buffer with flags: " << flags << " ..." << endl;
	unsigned int buffer_handle = xclAllocBO(device_dict[device_name]->handle, (size_t)size, domain, flags);
	cout << "Received buffer with handle " << buffer_handle << endl;
	BufferMeta meta;
	meta.handle = buffer_handle;
	meta.user_ptr = false;
	meta.mapped = false;
	meta.size = size;
	meta.addr = nullptr;
	device_dict[device_name]->buffer_dict[buffer_handle] = meta;
	return buffer_handle;
}

void free_buffer(string device_name, unsigned buffer_handle)
{
	xclFreeBO(device_dict[device_name]->handle, buffer_handle);
	device_dict[device_name]->buffer_dict.erase(buffer_handle);
}

void write_buffer(string device_name, unsigned buffer_handle, np::ndarray data)
{
	cout << "Getting raw data pointer from numpy array ..." << endl;
	char* data_ptr = data.get_data();
	np::dtype data_type = data.get_dtype();
	size_t size = data.shape(0) * data_type.get_itemsize();
	cout << "Loading data into buffer with size: " << size << ", element size: " << data_type.get_itemsize() << " bytes" << endl;
	xclWriteBO(device_dict[device_name]->handle, buffer_handle, (void*)data_ptr, size, 0);
}

np::ndarray read_buffer(string device_name, unsigned buffer_handle, unsigned size, unsigned skip, string type)
{
	np::dtype data_type = convert_buffer_data_type(type);
	int arr_size = size / data_type.get_itemsize();
	np::ndarray res = np::zeros(py::make_tuple(arr_size), data_type);
	char* data_ptr = res.get_data();
	xclReadBO(device_dict[device_name]->handle, buffer_handle, data_ptr, size, skip);
	return res;
}

void map_buffer(string device_name, unsigned buffer_handle, bool write) {
	void* addr = xclMapBO(device_dict[device_name]->handle, buffer_handle, write);
	device_dict[device_name]->buffer_dict[buffer_handle].addr = addr;
	device_dict[device_name]->buffer_dict[buffer_handle].mapped = true;
}

py::dict buffer_property(string device_name, unsigned buffer_handle) {
	xclBOProperties* properties = new xclBOProperties();
	py::dict res;
	auto err = xclGetBOProperties(device_dict[device_name]->handle, buffer_handle, properties);
	if (err) {
		delete properties;
		throw runtime_error("Failed to read buffer properties");
	}
	unsigned handle = properties->handle;
	unsigned flags = properties->flags;
	unsigned long size = properties->size;
	unsigned long physical_addr = properties->paddr;
	delete properties;
	res["buffer_handle"] = handle;
	res["flags"] = flags;
	res["size"] = size;
	res["physical_addr"] = physical_addr;
	return res;
}

void sync_buffer(string device_name, unsigned buffer_handle, string type, unsigned size, unsigned offset) {
	xclBOSyncDirection direction = convert_sync_buffer_type(type);
	int err = xclSyncBO(device_dict[device_name]->handle, buffer_handle, direction, size, offset);
	std::cout << "err: " << err << std::endl;
#if defined(SW_EMU)
	if (err) {
		throw runtime_error("Failed to sync buffer");
	}
#elif defined(HW_EMU)
	if (err < 0) {
		throw runtime_error("Failed to sync buffer");
	}
#else
	if (err) {
		throw runtime_error("Failed to sync buffer");
	}
#endif
}