// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
#ifndef xrtcore_util_buffer_schema_h_
#define xrtcore_util_buffer_schema_h_

#include <cstdint>
#include <map>
#include <string>

// UC Log Schema Version 0.25
// This file defines UC log schema mappings for parsing binary log messages
// from the firmware. It includes mappings for file IDs to filenames and
// log IDs to format strings.

namespace xrt_core {

/**
 * struct uc_log_map - Structure to hold UC log schema mappings
 *
 * @files: map of file IDs to filenames
 * @logs: map of log IDs to format strings
 */
struct uc_log_map
{
  std::map<uint32_t, std::string> files;  // file_id -> filename
  std::map<uint32_t, std::string> logs;   // log_id -> format string
};

inline const
uc_log_map
uc_log_schema = {
  // File ID to filename mapping
  {
    {0, "main.c"},
    {1, "dpu_control.c"},
    {2, "dpu_control.h"},
    {3, "barrier_legacy.c"},
    {4, "barrier_sema.c"},
    {5, "task_handler.c"},
    {6, "wait_handle.h"},
    {7, "queue.h"},
    {8, "uc_dma.c"},
    {9, "xrt_handler.c"},
    {10, "host_queue.c"},
    {11, "work.c"},
    {12, "dbg_handler.c"},
    {13, "profiling.c"},
    {14, "trace.c"},
    {15, "utils.c"},
    {16, "cache.c"}
  },
  
  // Log ID to format string mapping
  {
    {0, "ERROR: save level overflow\n"},
    {1, "job size 0 !!\n"},
    {2, "Invalid save page %d\n"},
    {3, "Invalid restore page %d\n"},
    {4, "Invalid page %d\n"},
    {5, "Invalid opcode: 0x%x @ %x\n"},
    {6, "  unfinished jobs:\n"},
    {7, "    job: %d state: %d pc: 0x%x\n"},
    {8, "WARN: Deadlock might have happened!!\nLast opcode PC: 0x%x page_idx: %d\n"},
    {9, "ERROR: invalid restore page index: %d\n"},
    {10, "ERROR: page sign 0x%x\n"},
    {11, "Exception!!\n\tESR: 0x%x EAR: 0x%x\n"},
    {12, "Error %d initializing XIOModule!"},
    {13, "Page-in to slot %u length 0x%x cur_save_level 0x%x\n"},
    {14, "[preemption] job: %u\n"},
    {15, "[block] job: %u\n"},
    {16, "[start] job: %u\n"},
    {17, "[launch] job: %u (%u)\n"},
    {18, "[end] job: %u\n"},
    {19, "[end] unfinished jobs number: %u\n"},
    {20, "OPCODE entering barrier (id: %d slot: %d)\n"},
    {21, "LB %u check-in\n"},
    {22, "LB %u release\n"},
    {23, "patch address (r%d r%d) "},
    {24, "0x%x : 0x%x\n"},
    {25, "collect trace info at offset %d\n"},
    {26, "invalid trace control body address: 0x%x\n"},
    {27, "pc before: %x\n"},
    {28, "pc after: %x\n"},
    {29, "[EOF] unfinished job number: %u\n"},
    {30, "post save: goes to sleep !!\n"},
    {31, "Leaving barrier: preemption point check\n"},
    {32, "preemption skip\n"},
    {33, "save ...\n"},
    {34, "save 'restore page' %d offset 0x%x to handshake\n"},
    {35, "restore ...\n"},
    {36, "load pdi skip\n"},
    {37, "load pdi ...\n"},
    {38, "[TIMESTAMP][ID] %d [HIGH] %d [LOW] %d\n"},
    {39, "load last pdi: %d ...\n"},
    {40, "[initial] opcode: %u\n"},
    {41, "[run] job: %u opcode: %u\n"},
    {42, "[unblock] job: %u\n"},
    {43, "Page-in to slot %u\n"},
    {44, "Leaving barrier: ooo execution\n"},
    {45, "Page-in to slot %u length 0x%x\n"},
    {46, "Page-in to slot %u length 0x%x cur_save_level %d\n"},
    {47, "Partition: [%u, %u], self = %u\n"},
    {48, "Log buffer: [0x%x 0x%x] "},
    {49, "size = 0x%x, id: %d\n"},
    {50, "tct %x fsl %x\n"},
    {51, "tct %x tlast = %x\n"},
    {52, "push to full queue !!\n"},
    {53, "pop from empty queue !!\n"},
    {54, "do_exec_buf\n"},
    {55, "executing control code ...\n"},
    {56, "Queue packet opcode: %u\n"},
    {57, "HSA queue empty, MB goes to sleep !\n"},
    {58, "ISR: %x\n"},
    {59, "Leaving barrier (pre-work) uc = %d\n"},
    {60, "lead: handle packet\n"},
    {61, "Leaving barrier (post-work) uc = %d\n"},
    {62, "Entering barrier (post-no_work) uc = %d\n"},
    {63, "slave %d: handle packet\n"},
    {64, "Leaving barrier (post-work) uc = %d\n"},
    {65, "illegal partition setting\n"},
    {66, "Distributing work to uc %u via BD %u\n"},
    {67, "Work distributed to extra %u uCs\n"},
    {68, "handle dbg queue...\n"},
    {69, "DBG queue packet opcode: %u\n"},
    {70, "do_dbg_rw\n"},
    {71, "Page-in to slot %u page offset 0x%x "},
    {72, "cur_save_level 0x%x\n"},
    {73, "[TIMESTAMP][ID] %d "},
    {74, "[HIGH] %d [LOW] %d\n"},
    {75, "    job: %d state: %d "},
    {76, "pc: 0x%x\n"},
    {77, "Page-in to slot %u length 0x%x "},
    {78, "cur_save_level %d\n"},
    {79, "Exception!!\n\tESR: 0x%x EAR: 0x%x "},
    {80, "PC: 0x%x\n"},
    {81, "Partition: [%u, %u], "},
    {82, "self = %u\n"},
    {83, "Waken up after preempted?\n"},
    {84, "prefetch load last elf: %d ...\n"},
    {85, "Page-in last elf to slot %u\n"},
    {86, "load elf skip\n"},
    {87, "load elf ...\n"},
    {88, "fw_state: 0x%x\n"},
    {89, "qh: 0x%x ql: 0x%x\n"},
    {90, "mpnpu base time: high 0x%x low 0x%x\n"},
    {91, "shim dma mm2s status: addr 0x%x value 0x%x\n"},
    {92, "shim dma s2mm status: addr 0x%x value 0x%x\n"},
    {93, "memtile dma mm2s status: addr 0x%x value 0x%x\n"},
    {94, "memtile dma s2mm status: addr 0x%x value 0x%x\n"},
    {95, "core memory module dma mm2s status: addr 0x%x value 0x%x\n"},
    {96, "core memory module dma s2mm status: addr 0x%x value 0x%x\n"},
    {97, "core status: addr 0x%x value 0x%x\n"},
    {98, "core pc: addr 0x%x value 0x%x\n"},
    {99, "load elf cp skip\n"},
    {100, "load elf cp ...\n"},
    {101, "break early same page ...\n"},
    {102, "break early new page ...\n"},
    {103, "load elf cp poff: 0x%x\n"},
    {104, "preempt prefetch job skipped\n"},
    {105, "offset 0x%x: 0x%x\n"},
    {106, "Handshake dump:\n"},
    {107, "hw uncorrectable error caught\n"},
    {108, "hw correctable error caught\n"},
    {109, "axi error caught\n"},
    {110, "%d cmds queued in current check\n"},
    {111, "handle slot %d , subcmd id in runlist: %d\n"},
    {112, "update completion\n"},
    {113, "ATOMICS update read_idx by %d\n"},
    {114, "cache hit for address 0x%x:0x%x\n"},
    {115, "cache miss for address 0x%x:0x%x\n"},
    {116, "update read_idx by %d\n"},
    {117, "lead: handle packet. chain_flag: %d\n"},
    {118, "q new write index high %d : low %d\n"},
    {119, "q new read index high %d : low %d\n"},
    {120, "q old write index high %d : low %d\n"},
    {121, "q old read index high %d : low %d\n"},
    {122, "\n"}
  }
};

} // xrt_core

#endif
