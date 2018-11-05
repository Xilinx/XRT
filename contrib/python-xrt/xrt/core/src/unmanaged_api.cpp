#include "unmanaged_api.h"

py::list unmanaged_read(string device_name, unsigned flags, unsigned long size, unsigned long offset) {
    // TODO: implement the API corresponding to xclUnmgdPread
    //
    // Plan: this API will use xclUnmgdPread to read the data from memory described by size, offset and flags
    // write the data read back into a python list in the form of unsigned int
    //
    // Note: this API does not care about performance, as it is mainly meant for debugging
    py::list res;
    return res;
}

void unmanaged_write(string device_name, unsigned flags, unsigned long size, unsigned long offset, py::list data) {
    // TODO: implement the API corresponding to xclUnmgdPread
    //
    // Plan: data should be a python array of unsigned int, and device_name is used to find device handle
    // internally it will use xclUnmgdPread to write data into the memory described by size, offset and flags
    // 
    // Note: this API does not care about performance, as it is mainly meant for debugging
}