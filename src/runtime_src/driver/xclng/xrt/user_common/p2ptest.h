#ifndef P2PTEST_H
#define P2PTEST_H

#include <chrono>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>


#include "driver/include/xclhal2.h"
#include "driver/include/xclbin.h"
using namespace std;
bool matched = true;
uint64_t ddr_bank_size = 16UL * 1024 * 1024 * 1024;
size_t chunk_size = 128 * 1024 * 1024;
char *test_string = (char *)"Hello World!";
xclDeviceHandle handle;
uint64_t p2pBoAddr_init;
uint64_t p2pBoAddr;
bool v;

int dma_write(uint64_t offset, size_t len)
{
    void *tmp = aligned_alloc(4096, len);
    if (tmp == NULL) {
        cout << "ERR: alloc mem failed" << ", err: " << strerror(errno) << endl;
        return -ENOMEM;
    }
    memset(tmp, 0, len);
    for (size_t i = 0; i < len; i += 4096)
        strcpy((char *)tmp + i, test_string);

    ssize_t s = xclUnmgdPwrite(handle, 0, tmp, len, p2pBoAddr + offset);
    free(tmp);

    if (s < 0) {
        cout << "ERR: dma_write failed" << endl;
        return -EIO;
    }
    return 0;
}

int dma_read_test(uint64_t offset, size_t len)
{
    void *tmp = aligned_alloc(4096, len);
    if (tmp == NULL) {
        cout << "ERR: alloc mem failed" << ", err: " << strerror(errno) << endl;
        return -ENOMEM;
        }

    ssize_t s = xclUnmgdPread(handle, 0, tmp, len, p2pBoAddr + offset);
    if (s < 0) {
        cout << "ERR: dma_read failed" <<endl;
        return -EIO;
    }
    for (size_t i = 0; i < len; i += 4096)
        if (strcmp((char *)tmp + i, test_string))
            matched = false;

    free(tmp);
    return 0;
}

void write_test(int id)
{
    if(v)
        cout << "Read Test: Bank" << id << " ddr addr 0x" << std::hex << p2pBoAddr << std::dec << ": ";
    for (uint64_t off = 0; off < ddr_bank_size; off += chunk_size) {
        matched = true;
        dma_write(off, chunk_size);
        if (!matched) {
            cout << "Read Test: data not identified" << endl;
            break;
        }
        if(v){
            cout << ".";
            cout.flush();
        }
    }
    cout << endl;
}

void read_test(int id)
{
    if(v)
        cout << "Write Test: Bank" << id << " ddr addr 0x" << std::hex << p2pBoAddr << std::dec << ": ";
    for (uint64_t off = 0; off < ddr_bank_size; off += chunk_size) {
        matched = true;
        dma_read_test(off, chunk_size);
        if (!matched) {
            cout << "Write Test: data not identified" << endl;
            break;
        }
        if(v){
            cout << ".";
            cout.flush();
        }
    }
    cout << endl;
}

int runp2p(int idx, string bit, bool verbose)
{
    v = verbose;
    std::string bitstreamFile = bit;
    int first_used_mem=-1;
    int deviceIndex = idx;
    xclDeviceInfo2 deviceInfo;

    handle = xclOpen(deviceIndex, "", XCL_INFO);
    if (xclGetDeviceInfo2(handle, &deviceInfo)) {
            throw std::runtime_error("Unable to obtain device information");
            return -1;
    }

    if(xclLockDevice(handle)) {
            throw std::runtime_error("Cannot lock device");
            return -1;
    }

    //load xclbin
    std::ifstream stream(bitstreamFile);
    stream.seekg(0, stream.end);
    int size = stream.tellg();
    stream.seekg(0, stream.beg);

    char *header = new char[size];
    stream.read(header, size);

    if (strncmp(header, "xclbin2", 8)) {
        throw std::runtime_error("Invalid bitstream");
    }

    const xclBin *blob = (const xclBin *)header;
    if (xclLoadXclBin(handle, blob)) {
        delete [] header;
        throw std::runtime_error("Bitstream download failed");
    }

    const axlf* top = (const axlf*)header;
    auto topo = xclbin::get_axlf_section(top, MEM_TOPOLOGY);
    struct mem_topology* topology = (mem_topology*)(header + topo->m_sectionOffset);

    for (int i=0; i<topology->m_count; ++i) {
        if (topology->m_mem_data[i].m_used) {
            first_used_mem = i;
            p2pBoAddr_init = topology->m_mem_data[i].m_base_address;
            break;
        }
    }
    delete [] header;


    p2pBoAddr = p2pBoAddr_init;
    for (int d = 0; d < 4; d++) {
    unsigned boHandle1 = xclAllocBO(handle, 1024, XCL_BO_DEVICE_RAM, first_used_mem);
    char* bo1 = (char*)xclMapBO(handle, boHandle1, true);
    memset(bo1, 0, 1024);

    //DMA write
    write_test(d);

    //read and check the result
    if(xclSyncBO(handle, boHandle1, XCL_BO_SYNC_BO_FROM_DEVICE , 1024, false))
        return 1;
    char* bo2 = (char*)xclMapBO(handle, boHandle1, false);
    if (memcmp(bo2, bo1, 1024))
        matched = false;

    p2pBoAddr = (p2pBoAddr +(ddr_bank_size*4));
    xclFreeBO(handle, boHandle1);
    if(!v)
        cout<< ".";

    }

    p2pBoAddr = p2pBoAddr_init;

    for (int d = 0; d < 4; d++) {
    unsigned boHandle1 = xclAllocBO(handle, 1024, XCL_BO_DEVICE_RAM, first_used_mem);
    char* bo1 = (char*)xclMapBO(handle, boHandle1, true);
    memset(bo1, 0, 1024);
    strcpy(bo1, test_string);

    //DMA read
    read_test(d);

    p2pBoAddr = (p2pBoAddr +(ddr_bank_size*4));
    xclFreeBO(handle, boHandle1);
    if(!v)
        cout<< ".";

    }
    cout << "\n";
return 0;
}

#endif /* P2PTEST_H */