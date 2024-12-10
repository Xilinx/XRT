/* 
 * SPDX-License-Identifier: Apache-2.0
   Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
 */

#pragma once

#include <cstdint>
#include <list>
#include <queue>
#include <bitset>
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <boost/pool/object_pool.hpp>
#include "xrt/detail/ert.h"

#define MAX_CUS	  128
#define	MAX_SLOTS 128

namespace xclhwemhal2 {
    class HwEmShim;
}

namespace hwemu {

    static const unsigned AP_START = 1;
    static const unsigned AP_DONE  = 2;
    static const unsigned AP_IDLE  = 4;
    static const unsigned AP_READY = 0x8;
    static const unsigned AP_CONTINUE = 0x10;

    //! Forward declaration 
    class xocl_cmd;
    class exec_core;
    class exec_ops;
    class xocl_scheduler;

    /**
     * @opcode(): Returns the command opcode
     * @type():   Returnd the command type
     * @uid():    Unique ID of the command
     * @payload_size(): Payload size
     * @num_cumasks():  Number of cu masks
     * @regmap_size():  CU Register map size
     * @packet():   Returns the ert packet from bo object
     * @regmap(): start address of the register map 
     */

    class xocl_cmd 
    {
        public:
            xocl_cmd();

            uint32_t    opcode();
            uint32_t    type();
            uint64_t    cmd_uid();
            uint32_t    payload_size();
            uint32_t    packet_size();
            uint32_t    num_cumasks();
            uint32_t    regmap_size();

            ert_packet* packet();
            uint32_t*   regmap();

            void     set_int_state(enum ert_cmd_state state);
            void     set_state(enum ert_cmd_state state);
            void     bo_init(xclemulation::drm_xocl_bo *bo);

            bool     has_cu(uint32_t cuidx);
            uint32_t first_cu();
            uint32_t next_cu(uint32_t prev);
            void     set_cu(uint32_t cuidx);

            xclemulation::drm_xocl_bo *bo;
            enum ert_cmd_state state;
            union {
                struct ert_packet	         *ert_pkt;
                struct ert_configure_cmd     *ert_cfg;
                struct ert_start_kernel_cmd  *ert_cu;
                struct ert_start_copybo_cmd  *ert_cp;
            };

            uint64_t  uid;      // unique id for this command
            uint32_t  cu_idx;   // index of CU running this cmd
            uint32_t  slot_idx; // index in exec core running queue
            bool      aborted;

            std::bitset<MAX_CUS> cu_bitmap; //CU bitmap

            //! Static member varibale 
            //  to get the unique ID for each command 
            static uint64_t next_uid;
    };

    /**
     * class xocl_cu: Represents a compute unit in penguin or dataflow mode
     *
     */
    class xocl_cu 
    {
        public:
            xocl_cu(xclhwemhal2::HwEmShim* dev);
            void      cu_init(unsigned int idx, uint64_t base, uint64_t addr, uint64_t polladdr);
            uint64_t  cu_base_addr();
            uint64_t  cu_polladdr();
            bool      cu_dataflow();
            bool      cu_valid();

            void      cu_poll();
            void      cu_continue();
            uint32_t  cu_status();

            bool      cu_ready();
            void      cu_pop_done();
            void      cu_configure_ooo(xocl_cmd *xcmd);
            void      cu_configure_ino(xocl_cmd *xcmd);
            bool      cu_start(xocl_cmd *xcmd);
            xocl_cmd* cu_first_done();

            void      iowrite32(uint32_t data, uint64_t addr);
            uint32_t  ioread32(uint64_t addr);
            void      xocl_memcpy_toio(uint64_t addr, uint32_t* data, uint32_t len);
            void      xocl_memcpy_fromio(uint32_t* data, uint64_t addr, uint32_t len);

            uint32_t   idx;
            uint32_t   uid;
            uint32_t   control;
            bool       dataflow;

            uint64_t   base;
            uint64_t   addr;
            uint64_t   polladdr;

            uint32_t   ap_check;
            bool       error;

            uint32_t   ctrlreg;
            uint32_t   done_cnt;
            uint32_t   run_cnt;

            xclhwemhal2::HwEmShim*  xdevice;
            std::queue<xocl_cmd*>   cu_cmdq;
    };

    /**
     * class xocl_ert: Represents embedded scheduler in ert mode
     *
     */

    class xocl_ert 
    {
        public:
            xocl_ert(xclhwemhal2::HwEmShim* dev, uint64_t csr_base, uint64_t cq_base);
            ~xocl_ert();

            void      ert_cfg(uint32_t cq_size, uint32_t num_slots, bool cq_intr);
            void      ert_clear_csr();

            uint32_t  ert_acquire_slot_idx();
            uint32_t  ert_acquire_slot(xocl_cmd *xcmd);
            void      ert_release_slot_idx(uint32_t slot_idx);
            void      ert_release_slot(xocl_cmd *xcmd);
            xocl_cmd* ert_get_cmd(uint32_t slotidx);
            bool      ert_start_cmd(xocl_cmd *xcmd);

            void      ert_read_custat(xocl_cmd *xcmd, uint32_t num_cus);
            uint32_t  ert_version();
            uint32_t  ert_cu_usage( unsigned int cuidx);
            uint32_t  ert_cu_status(unsigned int cuidx);
            bool      ert_cq_slot_busy(unsigned int slotidx);
            uint32_t  ert_cq_slot_status(unsigned int slotidx);
            uint32_t  ert_cq_slot_usage(unsigned int slotidx);

            void      iowrite32(uint32_t data, uint64_t addr);
            uint32_t  ioread32(uint64_t addr);
            void      xocl_memcpy_toio(uint64_t addr, void* data, uint32_t len);
            void      xocl_memcpy_fromio(void* data, uint64_t addr, uint32_t len);

            uint64_t  csr_base;
            uint64_t  cq_base;
            uint32_t  uid;

            uint32_t  cq_size;
            uint32_t  num_slots;

            uint32_t  slot_size;
            bool      cq_intr;

            xocl_cmd * command_queue[MAX_SLOTS];

            // Bitmap tracks busy(1)/free(0) slots in command_queue
            std::bitset<MAX_SLOTS> slot_status;
            unsigned int	  ctrl_busy;

            // stats
            uint32_t  version;
            uint32_t  cu_usage[MAX_CUS];
            uint32_t  cu_status[MAX_CUS];
            uint32_t  cq_slot_status[MAX_SLOTS];
            uint32_t  cq_slot_usage[MAX_SLOTS];

            //! To acces device memory/CU's for Read/Write
            xclhwemhal2::HwEmShim* xdevice;
    };

    /**
     * exec_core: Core data structure for command execution on a device
     *
     * The execution core receives commands from scheduler when it transfers
     * execbuf command objects to execution cores where they are queued.  When the
     * scheduler services an execution core, the queued commands are submitted to
     * matching pending queue depending on command type.  A CU command is
     * submitted to the matching CU queue with fewest entries.  Pending CU
     * commands are started when the CU is available (kds mode) or when there is
     * room in the running command queue (ert mode).  When checking command
     * completion only the commands in the running queue need to be checked.
     */

    class exec_core 
    {
        public:
            exec_core(xclhwemhal2::HwEmShim* dev, xocl_scheduler* sched);

            int      exec_cfg_cmd(xocl_cmd *xcmd);
            bool     exec_is_ert();
            bool     exec_is_ert_poll();
            bool     exec_is_penguin();
            bool     exec_is_polling();
            bool     exec_is_flush();

            uint32_t exec_cu_base_addr(uint32_t cuidx);
            uint32_t exec_cu_usage(uint32_t cuidx);
            uint32_t exec_cu_status(uint32_t cuidx);

            uint32_t exec_num_running();
            uint32_t exec_num_pending();

            bool     exec_valid_cu(uint32_t cuidx);
            void     exec_cfg();

            //irqreturn_t versal_isr(int irq, void *arg);
            //irqreturn_t exec_isr(int irq, void *arg);

            xocl_scheduler* exec_scheduler();
            void   exec_update_custatus();
            int    exec_finish_cmd(xocl_cmd *xcmd);
            int    exec_execute_copybo_cmd(xocl_cmd *xcmd);

            void   exec_notify_host(xocl_cmd* xcmd);
            void   exec_mark_cmd_state(xocl_cmd *xcmd, enum ert_cmd_state state);
            void   exec_mark_cmd_complete(xocl_cmd *xcmd);
            void   exec_mark_cmd_error(xocl_cmd *xcmd);
            void   exec_process_cmd_mask(uint32_t mask, uint32_t mask_idx);
            void   exec_process_cu_mask(uint32_t mask, uint32_t mask_idx);

            bool   exec_penguin_start_cu_cmd(xocl_cmd *xcmd);
            bool   exec_penguin_start_ctrl_cmd(xocl_cmd *xcmd);
            void   exec_penguin_query_cmd(xocl_cmd *xcmd);
            bool   exec_ert_start_cmd(xocl_cmd *xcmd);
            bool   exec_ert_start_ctrl_cmd(xocl_cmd *xcmd);
            void   exec_ert_clear_csr();

            void   exec_ert_query_mailbox(xocl_cmd *xcmd);
            void   exec_ert_query_csr(xocl_cmd *xcmd, uint32_t mask_idx);
            void   exec_ert_query_cu(xocl_cmd *xcmd);
            void   exec_ert_query_cmd(xocl_cmd *xcmd);
            void   exec_query_cmd(xocl_cmd *xcmd);
            void   exec_cmd_free(xocl_cmd *xcmd);
            void   exec_abort_cmd(xocl_cmd *xcmd);

            bool   exec_start_cu_cmd(xocl_cmd *xcmd);
            bool   exec_start_ctrl_cmd(xocl_cmd *xcmd);
            bool   exec_start_kds_cmd(xocl_cmd *xcmd);

            int    exec_start_cu_range(uint32_t start, uint32_t end);
            int    exec_start_cus();
            int    exec_start_ctrl();
            int    exec_start_kds();
            int    exec_start_scu();

            bool   exec_submit_cu_cmd(xocl_cmd *xcmd);
            bool   exec_submit_ctrl_cmd(xocl_cmd *xcmd);
            bool   exec_submit_kds_cmd(xocl_cmd *xcmd);
            bool   exec_submit_scu_cmd(xocl_cmd *xcmd);
            bool   exec_submit_cmd(xocl_cmd *xcmd);

            void   exec_error_to_free(xocl_cmd *xcmd);
            void   exec_new_to_queued(xocl_cmd *xcmd);
            void   exec_queued_to_submitted();
            void   exec_submitted_to_running();
            void   exec_running_to_complete();

            //! Called from sceduler
            void   exec_service_cmds();

            void     iowrite32(uint32_t data, uint64_t addr);
            uint32_t ioread32(uint64_t addr);

            uint64_t   base;
            uint64_t   csr_base;
            uint64_t   cq_base;
            uint32_t   cq_size;

            uint32_t   intr_base;
            uint32_t   intr_num;

            uint32_t   uid;
            uint32_t   num_cus;
            uint32_t   num_cdma;

            bool	   polling_mode;
            bool	   cq_interrupt;
            bool	   configure_active;
            bool	   configured;
            bool	   stopped;
            bool	   flush;

            uint32_t   num_running_cmds;
            uint32_t   num_pending_cmds;
            uint32_t   cu_load_count[MAX_CUS];
            uint32_t   cu_usage[MAX_CUS];
            uint32_t   cu_status[MAX_CUS];

            xocl_cu*   cus[MAX_CUS];
            xocl_ert*  ert;
            exec_ops*  ops;
            xocl_scheduler*        scheduler;
            xclhwemhal2::HwEmShim* xdevice;

            // Status register pending complete.  Written by ISR, cleared by
            // scheduler
            std::atomic<int>  sr0;
            std::atomic<int>  sr1;
            std::atomic<int>  sr2;
            std::atomic<int>  sr3;

            std::queue<xocl_cmd*>   pending_ctrl_queue;
            std::queue<xocl_cmd*>   pending_kds_queue;
            std::queue<xocl_cmd*>   pending_scu_queue;
            std::queue<xocl_cmd*>   pending_cmd_queue;
            std::list<xocl_cmd*>    running_cmd_queue;
            std::queue<xocl_cmd*>   pending_cu_queue[MAX_CUS];
    };

    /**
     * class exec_ops: scheduler specific operations
     *
     * @start_cmd: start a command on a device
     * @start_ctrl: starts a control command
     * @query_cmd: check if a command has completed
     * @query_ctrl: check if a control command has completed
     * @process_mask: process command status register from ERT
     */

    class exec_ops
    {
        public:
            exec_ops(exec_core* core) { exec = core; }

            virtual bool start_cmd(xocl_cmd  *xcmd) = 0;
            virtual bool start_ctrl(xocl_cmd *xcmd) = 0;
            virtual void query_cmd(xocl_cmd  *xcmd) = 0;
            virtual void query_ctrl(xocl_cmd *xcmd) = 0;

            //Default implementation for penguin mode
            virtual void process_mask(uint32_t mask, uint32_t mask_idx) {}

            virtual bool is_ert()      { return false; }
            virtual bool is_ert_poll() { return false; }
            virtual bool is_penguin()  { return false; }

        protected:
            exec_core *exec;
    };

    /**
     * class ert_ops: ERT scheduling
     *
     * functions used in regular (no dataflow) ERT mode
     */

    class ert_ops : public exec_ops
    {
        public:
            ert_ops(exec_core* core): exec_ops(core) {}

            bool start_cmd(xocl_cmd  *xcmd) { return exec->exec_ert_start_cmd(xcmd); }
            bool start_ctrl(xocl_cmd *xcmd) { return exec->exec_ert_start_ctrl_cmd(xcmd); }
            void query_cmd(xocl_cmd  *xcmd) { exec->exec_ert_query_cmd(xcmd); }
            void query_ctrl(xocl_cmd *xcmd) { exec->exec_ert_query_cmd(xcmd); }

            void process_mask(uint32_t mask, uint32_t mask_idx) { 
                exec->exec_process_cmd_mask(mask, mask_idx); 
            }

            bool is_ert() { return true; }
    };

    /**
     * class ert_poll_ops: kernel mode scheduling with ert polling
     *
     * functions used in dataflow mode only when ERT is
     * assisting in polling for CU completion.
     */

    class ert_poll_ops : public exec_ops
    {
        public:
            ert_poll_ops(exec_core* core): exec_ops(core) {}

            bool start_cmd(xocl_cmd  *xcmd) { return exec->exec_penguin_start_cu_cmd(xcmd); }
            bool start_ctrl(xocl_cmd *xcmd) { return exec->exec_ert_start_ctrl_cmd(xcmd); }
            void query_cmd(xocl_cmd  *xcmd) { exec->exec_ert_query_cu(xcmd); }
            void query_ctrl(xocl_cmd *xcmd) { exec->exec_ert_query_cmd(xcmd); }

            void process_mask(uint32_t mask, uint32_t mask_idx) { 
                exec->exec_process_cu_mask(mask, mask_idx); 
            }

            bool is_ert_poll() { return true; }
    };

    /**
     * class penguin_ops: kernel mode scheduling (penguin)
     *
     * functions used in regular (no dataflow) penguin mode
     */

    class penguin_ops : public exec_ops
    {
        public:
            penguin_ops(exec_core* core): exec_ops(core) {}

            bool start_cmd(xocl_cmd  *xcmd) { return exec->exec_penguin_start_cu_cmd(xcmd); }
            bool start_ctrl(xocl_cmd *xcmd) { return exec->exec_penguin_start_ctrl_cmd(xcmd); }
            void query_cmd(xocl_cmd  *xcmd) { exec->exec_penguin_query_cmd(xcmd); }
            void query_ctrl(xocl_cmd *xcmd) { exec->exec_penguin_query_cmd(xcmd); }

            bool is_penguin()  { return true; }
    };

    /**
     * class xocl_sched: scheduler for xocl_cmd objects
     */

    class xocl_scheduler 
    {
        public:
            xocl_scheduler(xclhwemhal2::HwEmShim* dev);
            ~xocl_scheduler();

            void  scheduler_wake_up();
            void  scheduler_intr();

            void  scheduler_decr_poll();
            void  scheduler_incr_poll();

            int   scheduler_wait_condition();
            void  scheduler_wait();

            void  scheduler_queue_cmds();
            void  scheduler_service_cores();

            void  scheduler_loop();
            int   scheduler();

            int   add_xcmd(xocl_cmd *xcmd);
            int   convert_execbuf(xocl_cmd* xcmd);
            int   add_bo_cmd(xclemulation::drm_xocl_bo *buf);
            int   add_exec_buffer(xclemulation::drm_xocl_bo *buf);

            std::thread*             scheduler_thread;
            std::mutex               scheduler_mutex;
            std::condition_variable  wait_condition; //!< Condition variable to pause std::thread

            std::list<xocl_cmd*>     pending_cmds;
            std::mutex               pending_cmds_mutex;
            std::atomic<uint32_t>    num_pending;

            //! Boost object pool to manage the memory for xocl cmds
            boost::object_pool<xocl_cmd>  cmd_pool;

            //! exec_core
            exec_core*  exec;
            xclhwemhal2::HwEmShim*   device;

            bool        error;
            bool        stop;
            bool        reset;

            uint32_t	intc; /* pending intr shared with isr, word aligned atomic */
            uint32_t    poll; /* number of cmds to poll */
    };
}  //! namespace xclhwemhal2
