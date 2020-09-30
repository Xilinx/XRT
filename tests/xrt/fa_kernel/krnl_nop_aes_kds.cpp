#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <memory>
#include <chrono>
#include <sys/mman.h>
#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>

#include "xrt.h"
#include "ert.h"
#include "xclbin.h"
#include "experimental/xrt-next.h"

namespace fa {
    typedef enum {
        DESC_FIFO_OVERRUN = 0x1,
        DESC_DECERR       = 0x2,
        TASKCOUNT_DECERR  = 0x4,
    } error_t;

    typedef enum {
        UNDEFINED = 0xFFFFFFFF,
        ISSUED    = 0x0,
        COMPLETED = 0x1,
    } status_t;

    typedef struct {
        uint32_t     argOffset;   // offset within the acc aperture
        uint32_t     argSize;     // size of argument in bytes
        //void        *argValue;    // value of argument
        uint32_t     argValue[];
    } descEntry_t;

    typedef struct {
        uint32_t     status;            // descriptor control synchronization word
        uint32_t     numInputEntries;   // number of input arg entries
        uint32_t     inputEntryBytes;   // total number of bytes for input args
        uint32_t     numOutputEntries;  // number of output arg entries
        uint32_t     outputEntryBytes;  // total number of bytes for output args
        //descEntry_t *inputEntries;      // array of input entries
        //descEntry_t *outputEntries;     // array of output entries
        uint32_t     data[];
    } descriptor_t;

    void print_descriptor(descriptor_t *desc) {
        int i, j;
        int off;
        descEntry_t *entry;

        std::cout << "status            0x" << std::hex << desc->status << "\n";
        std::cout << "numInputEntries   0x" << desc->numInputEntries << "\n";
        std::cout << "inputEntryBytes   0x" << desc->inputEntryBytes << "\n";
        std::cout << "numOutputEntries  0x" << desc->numOutputEntries << "\n";
        std::cout << "outputEntryBytes  0x" << desc->outputEntryBytes << "\n";
        off = 0;
        for (i = 0; i < desc->numInputEntries; i++) {
            entry = reinterpret_cast<descEntry_t *>(desc->data + off);
            std::cout << "input descEntry 0x" << i << "\n";
            std::cout << "    argOffset  0x" << entry->argOffset << "\n";
            std::cout << "    argSize    0x" << entry->argSize << "\n";
            for (j = 0; j < entry->argSize/4; j++) {
                std::cout << "    argValue   0x" << entry->argValue[j] << "\n";
            }
            off += (sizeof(descEntry_t) + entry->argSize)/4;
        }

        for (i = 0; i < desc->numOutputEntries; i++) {
            entry = reinterpret_cast<descEntry_t *>(desc->data + off);
            std::cout << "output descEntry 0x" << i << "\n";
            std::cout << "    argOffset  0x" << entry->argOffset << "\n";
            std::cout << "    argSize    0x" << entry->argSize << "\n";
            for (j = 0; j < entry->argSize/4; j++) {
                std::cout << "    argValue   0x" << entry->argValue[j] << "\n";
            }
            off += (sizeof(descEntry_t) + entry->argSize)/4;
        }
        std::cout << std::dec;
    }
};

/* The nop AES kernel needs 7 arguments
 * 0x10 DATA_IN_OFFSET, size: 8bytes
 * 0x18 DATA_IN_BYTES, size: 4bytes
 * 0x1C DATA_OUT_OFFSET, size: 8bytes
 * 0x24 DATA_OUT_LEN_AVAIL, size: 4bytes
 * 0x28 DATA_OUT_STATUS_OFFSET, size: 8bytes
 * 0x30 KEY1, size: 64bytes
 * 0x70 IV, size: 16bytes
 */

uint32_t aes_key[16] = {
    0xeb5aa3b8,
    0x17750c26,
    0x9d0db966,
    0xbcb9e3b6,
    0x510e08c6,
    0x83956e46,
    0x3bd10f72,
    0x769bf32e,
    0xfa374467,
    0x3386553a,
    0x46f91c6a,
    0x6b25d1b4,
    0x6116fa6f,
    0xd29b1a56,
    0x9c193635,
    0x10ed77d4
};

uint32_t aes_iv[4] = {
    0x149f40ae,
    0x38f1817d,
    0x32ccb7db,
    0xa6ef0e05
};

int get_input_entries_size()
{
    int size;

    /* Add entry size of DATA_IN_OFFSET */
    size = sizeof(fa::descEntry_t) + 8;

    /* Add entry size of DATA_IN_BYTES */
    size += sizeof(fa::descEntry_t) + 4;

    /* Add entry size of DATA_OUT_OFFSET */
    size += sizeof(fa::descEntry_t) + 8;

    /* Add entry size of DATA_OUT_LEN_AVAIL */
    size += sizeof(fa::descEntry_t) + 4;

    /* Add entry size of DATA_OUT_STATUS_OFFSET */
    size += sizeof(fa::descEntry_t) + 8;

    /* Add entry size of KEY */
    size += sizeof(fa::descEntry_t) + sizeof(aes_key);

    /* Add entry size of IV */
    size += sizeof(fa::descEntry_t) + sizeof(aes_iv);

    return size;
}

int get_output_entries_size()
{
    return 0;
}

int get_desc_size()
{
    int size;

    /* Descriptor - sizeof(data) */
    size = sizeof(fa::descriptor_t);

    size += 112;//get_input_entries_size();
    //size += get_input_entries_size();

    size += get_output_entries_size();

    return size;
}

#define DESC_FIFO_DEPTH 16

struct task_info {
    unsigned                in_data_boh;
    unsigned                out_data_boh;
    unsigned                out_status_boh;
    unsigned                desc_bo;
    unsigned                exec_bo;
    uint64_t                desc_paddr;
    fa::descriptor_t       *desc;
    ert_start_kernel_cmd   *ecmd;
};

void usage()
{
    printf("Usage: test -k <xclbin> -d <dev_id>\n");
}

static std::vector<char>
load_file_to_memory(const std::string& fn)
{
    if (fn.empty())
        throw std::runtime_error("No xclbin specified");

    // load bit stream
    std::ifstream stream(fn);
    if (!stream.good())
        throw std::runtime_error("xclbin path is incorrect");
    stream.seekg(0,stream.end);
    size_t size = stream.tellg();
    stream.seekg(0,stream.beg);

    std::vector<char> bin(size);
    stream.read(bin.data(), size);

    return bin;
}

inline void drop_uncompleted_task(xclDeviceHandle handle, task_info &cmd) {
    if (cmd.desc != NULL)
        munmap(cmd.desc, 4096);
 
    if (cmd.ecmd != NULL)
        munmap(cmd.ecmd, 4096);
 
    if (cmd.in_data_boh != NULLBO)
        xclFreeBO(handle, cmd.in_data_boh);

    if (cmd.out_data_boh != NULLBO)
        xclFreeBO(handle, cmd.out_data_boh);

    if (cmd.out_status_boh != NULLBO)
        xclFreeBO(handle, cmd.out_status_boh);

    if (cmd.desc_bo != NULLBO)
        xclFreeBO(handle, cmd.desc_bo);

    if (cmd.exec_bo != NULLBO)
        xclFreeBO(handle, cmd.exec_bo);
}

inline void start_fa_kernel(xclDeviceHandle handle, int cu_idx, uint64_t desc_addr)
{
    static uint32_t msb = 0;

    if (msb != desc_addr >> 32) {
        //std::cout << "Writing 0x" << std::hex << (desc_addr >> 32) << std::dec << " to 0x00 register" << std::endl;
        /* 0x00 nextDescriptorAddr_MSW register
         * This register doesn't need to change in each kick off
         */
        xclRegWrite(handle, cu_idx, 0x00, desc_addr >> 32);
        msb = desc_addr >> 32;
    }

    //std::cout << "Writing 0x" << std::hex << static_cast<int>(desc_addr) << std::dec << " to 0x04 register" << std::endl;
    /* **  Write to the LSW register will trigger the exectuion ** */
    /* 0x04 nextDescriptorAddr_LSW register */
    xclRegWrite(handle, cu_idx, 0x04, desc_addr);
}

double runTest(xclDeviceHandle handle, std::vector<std::shared_ptr<task_info>>& cmds,
               unsigned int total)
{
    int i = 0;
    unsigned int issued = 0, completed = 0;
    auto start = std::chrono::high_resolution_clock::now();

    for (auto& cmd : cmds) {
        if (xclExecBuf(handle, cmd->exec_bo))
            throw std::runtime_error("Unable to issue exec buf");
        if (++issued == total)
            break;
    }

    while (completed < total) {
        /* assume commands to the same CU finished in order */
        while (cmds[i]->ecmd->state < ERT_CMD_STATE_COMPLETED) {
            while (xclExecWait(handle, -1) == 0);
        }
        if (cmds[i]->ecmd->state != ERT_CMD_STATE_COMPLETED)
            throw std::runtime_error("CU execution failed");
        
        completed++;
        if (issued < total) {
            cmds[i]->ecmd->state = ERT_CMD_STATE_NEW;
            if (xclExecBuf(handle, cmds[i]->exec_bo))
                throw std::runtime_error("Unable to issue exec buf");
            issued++;
        }

        if (++i == cmds.size())
            i = 0;
    }

    auto end = std::chrono::high_resolution_clock::now();
    return (std::chrono::duration_cast<std::chrono::microseconds>(end - start)).count();
}

int run_test(xclDeviceHandle handle, xuid_t uuid, int bank)
{
    std::vector<std::shared_ptr<task_info>> cmds;
    //std::vector<unsigned int> cmds_per_run = { 1 };
    //std::vector<unsigned int> cmds_per_run = { 16 };
    //std::vector<unsigned int> cmds_per_run = { 16, 100, 1000, 10000 };
    std::vector<unsigned int> cmds_per_run = { 100, 1000, 10000, 100000, 1000000 };
    //int expected_cmds = 1;
    //int expected_cmds = 16;
    int expected_cmds = 1000;
    int size;

    /* descriptor size is kernel specific
     * Since descEntry and descriptor are variable size, needs pre calculate the size.
     */
    size = get_desc_size();

    std::cout << "descriptor size " << size << std::endl;

    //auto cu_idx = xclIPName2Index(handle, "fa_aes_xts2_rtl_enc:fa_aes_xts2_rtl_enc_1");
    int cu_idx = 0;
    if (xclOpenContext(handle, uuid, cu_idx, false))
        throw std::runtime_error("Cound not open context");

    for (int i = 0; i < expected_cmds; i++) {
        task_info cmd = {NULLBO, NULLBO, NULLBO, NULLBO, NULLBO, 0, NULL, NULL};
        xclBOProperties prop;
        fa::descEntry_t *entry;
        uint64_t boh_addr;
        uint32_t len;
        int off;
        int j;

        cmd.in_data_boh = xclAllocBO(handle, 4096, 0, bank);
        if (cmd.in_data_boh == NULLBO) {
            std::cout << "xclAllocBO failed in_data" << std::endl;
            break;
        }
        uint32_t *input = reinterpret_cast<uint32_t *>(xclMapBO(handle, cmd.in_data_boh, true));
        for (int j = 0; j < 1024; j++) {
            input[j] = j;
        }
        xclSyncBO(handle, cmd.in_data_boh, XCL_BO_SYNC_BO_TO_DEVICE, 4096, 0);

        cmd.out_data_boh = xclAllocBO(handle, 4096, 0, bank);
        if (cmd.out_data_boh == NULLBO) {
            std::cout << "xclAllocBO failed out_data" << std::endl;
            drop_uncompleted_task(handle, cmd);
            break;
        }

        cmd.out_status_boh = xclAllocBO(handle, 4096, 0, bank);
        if (cmd.out_status_boh == NULLBO) {
            std::cout << "xclAllocBO failed out_status" << std::endl;
            drop_uncompleted_task(handle, cmd);
            break;
        }

        cmd.exec_bo = xclAllocBO(handle, 4096, 0, XCL_BO_FLAGS_EXECBUF);
        if (cmd.exec_bo == NULLBO) {
            std::cout << "xclAllocBO failed exec_bo" << std::endl;
            drop_uncompleted_task(handle, cmd);
            break;
        }
        cmd.ecmd = reinterpret_cast<ert_start_kernel_cmd *>(xclMapBO(handle, cmd.exec_bo, true));
        if (cmd.ecmd == MAP_FAILED) {
            drop_uncompleted_task(handle, cmd);
            break;
        }

        cmd.ecmd->state = ERT_CMD_STATE_NEW;
        cmd.ecmd->opcode = ERT_START_FA;
        cmd.ecmd->type = ERT_CU;
        cmd.ecmd->count = 0x30;
        cmd.ecmd->cu_mask = 0x1;

        /* --- Construct descriptor --- */
        cmd.ecmd->data[0] = fa::ISSUED;
        cmd.ecmd->data[1] = 7;
        cmd.ecmd->data[2] = 112;
        cmd.ecmd->data[3] = 0;
        cmd.ecmd->data[4] = 0;

        off = 0;
        /* Entry for DATA_IN_OFFSET */
        xclGetBOProperties(handle, cmd.in_data_boh, &prop);
        boh_addr = prop.paddr;
        entry = reinterpret_cast<fa::descEntry_t *>(&cmd.ecmd->data[5] + off);
        entry->argOffset = 0x10;
        entry->argSize = sizeof(boh_addr);
        /* arrValue[] is aligned by bytes, use memcpy() */
        memcpy(entry->argValue, &boh_addr, entry->argSize);
        off += (sizeof(fa::descEntry_t) + entry->argSize)/4;

        /* Entry for DATA_IN_BYTES */
        entry = reinterpret_cast<fa::descEntry_t *>(&cmd.ecmd->data[5] + off);
        entry->argOffset = 0x18;
        entry->argSize = sizeof(len);
        len = 4096;
        memcpy(entry->argValue, &len, entry->argSize);
        off += (sizeof(fa::descEntry_t) + entry->argSize)/4;

        /* Entry for DATA_OUT_OFFSET */
        xclGetBOProperties(handle, cmd.out_data_boh, &prop);
        boh_addr = prop.paddr;
        entry = reinterpret_cast<fa::descEntry_t *>(&cmd.ecmd->data[5] + off);
        entry->argOffset = 0x1C;
        entry->argSize = sizeof(boh_addr);
        memcpy(entry->argValue, &boh_addr, entry->argSize);
        off += (sizeof(fa::descEntry_t) + entry->argSize)/4;

        /* Entry for DATA_OUT_LEN_AVAIL */
        entry = reinterpret_cast<fa::descEntry_t *>(&cmd.ecmd->data[5] + off);
        entry->argOffset = 0x24;
        entry->argSize = sizeof(len);
        len = 4096;
        memcpy(entry->argValue, &len, entry->argSize);
        off += (sizeof(fa::descEntry_t) + entry->argSize)/4;

        /* Entry for DATA_OUT_STATUS_OFFSET */
        xclGetBOProperties(handle, cmd.out_status_boh, &prop);
        boh_addr = prop.paddr;
        entry = reinterpret_cast<fa::descEntry_t *>(&cmd.ecmd->data[5] + off);
        entry->argOffset = 0x28;
        entry->argSize = sizeof(boh_addr);
        memcpy(entry->argValue, &boh_addr, entry->argSize);
        off += (sizeof(fa::descEntry_t) + entry->argSize)/4;

        /* Entry for KEY1 */
        entry = reinterpret_cast<fa::descEntry_t *>(&cmd.ecmd->data[5] + off);
        entry->argOffset = 0x30;
        entry->argSize = sizeof(aes_key);
        memcpy(entry->argValue, aes_key, entry->argSize);
        off += (sizeof(fa::descEntry_t) + entry->argSize)/4;

        /* Entry for IV */
        entry = reinterpret_cast<fa::descEntry_t *>(&cmd.ecmd->data[5] + off);
        entry->argOffset = 0x70;
        entry->argSize = sizeof(aes_iv);
        memcpy(entry->argValue, aes_iv, entry->argSize);
        off += (sizeof(fa::descEntry_t) + entry->argSize)/4;
        /* --- End Construct descriptor --- */

        //print_descriptor(cmd.desc);

        cmds.push_back(std::make_shared<task_info>(cmd));
    }
    /* Maybe the manchine is not able to allocate BO for all commands.
     * In this case, the cmds.size() is less than expected_cmds
     * After a command finished, re-send the command.
     */
    std::cout << "Allocated commands, expect " << expected_cmds << ", created " << cmds.size() << std::endl;
    if (!cmds.size())
        throw std::runtime_error("Can not create command");

    for (auto &num_cmds : cmds_per_run) {
        auto duration = runTest(handle, cmds, num_cmds);
        std::cout << "Commands: " << std::setw(7) << num_cmds
            << " iops: " << (num_cmds * 1000.0 * 1000.0 / duration)
            << std::endl;
    }

    for (auto &cmd : cmds) {
        munmap(cmd->desc, 4096);
        munmap(cmd->ecmd, 4096);
        xclFreeBO(handle, cmd->in_data_boh);
        xclFreeBO(handle, cmd->out_data_boh);
        xclFreeBO(handle, cmd->out_status_boh);
        xclFreeBO(handle, cmd->desc_bo);
        xclFreeBO(handle, cmd->exec_bo);
    }

    xclCloseContext(handle, uuid, 0);
    return 0;
}

int _main(int argc, char* argv[])
{
    xclDeviceHandle handle;
    std::string xclbin_fn;
    int device_id = 0;
    xuid_t uuid;
    int first_mem = 0;
    char c;

    while ((c = getopt(argc, argv, "k:d:h")) != -1) {
        switch (c) {
            case 'k':
               xclbin_fn = optarg;
               break;
            case 'd':
               device_id = std::atoi(optarg);
               break;
            case 'h':
               usage();
        }
    }

    if (xclbin_fn.empty())
        throw std::runtime_error("No xclbin");

    printf("The system has %d device(s)\n", xclProbe());

    handle = xclOpen(device_id, "", XCL_QUIET);
    if (!handle) {
        printf("Could not open device\n");
        return 1;
    }

    auto xclbin = load_file_to_memory(xclbin_fn);
    auto top = reinterpret_cast<const axlf*>(xclbin.data());
    auto topo = xclbin::get_axlf_section(top, MEM_TOPOLOGY);
    auto topology = reinterpret_cast<mem_topology*>(xclbin.data() + topo->m_sectionOffset);
    if (xclLoadXclBin(handle, top))
        throw std::runtime_error("Bitstream download failed");

    uuid_copy(uuid, top->m_header.uuid);

    for (int i = 0; i < topology->m_count; ++i) {
        if (topology->m_mem_data[i].m_used) {
            first_mem = i;
            break;
        }
    }

    run_test(handle, uuid, first_mem);

    xclClose(handle);
    return 0;
}

int main(int argc, char *argv[])
{
    try {
        _main(argc, argv);
        return 0;
    }
    catch (const std::exception& ex) {
        std::cout << "TEST FAILED: " << ex.what() << std::endl;
    }
    catch (...) {
        std::cout << "TEST FAILED" << std::endl;
    }

    return 1;
};
