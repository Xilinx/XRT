#include "register_read_write_api.h"

py::list read_register(string device_name, string domain, unsigned long offset, unsigned size) {
    xclAddressSpace space = convert_register_domain(domain);
    vector<unsigned> host_buffer(size, 0);
    size_t size_in_bytes = 4 * size;
    unsigned size_back = xclRead(device_dict[device_name]->handle, space, offset, (void*)host_buffer.data(), size_in_bytes);
    py::list res;
    for (unsigned i = 0; i < size; ++i) {
        py::dict word;
        word["addr"] = offset + i * 4;
        word["value"] = host_buffer[i];
        res.append(word);
    }
    cout << size_back << " bytes was read back from the registers" << endl;
    return res;
}

void write_register(string device_name, string domain, unsigned long offset, unsigned size, py::list data) {
    // TODO: implement this API using xclWrite
    //
    // Plan: this API will use xclWrite to write data which is a list of unsigned int into the registers
    // described by domain, offset, and size
    //
    // Note: this API is not concerned with performance. it is meant for debugging and profiling
}