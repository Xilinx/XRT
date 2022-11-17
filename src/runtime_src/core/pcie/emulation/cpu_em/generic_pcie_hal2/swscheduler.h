/* 
 * SPDX-License-Identifier: Apache-2.0
   Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
 */

#ifndef _SW_SCHEDULER_H_
#define _SW_SCHEDULER_H_

#include <list>
#include <mutex>
#include <cmath>
#include <cstdint>
#include <queue>
#include <thread>
#include <condition_variable>
#include "ert.h"

#define XOCL_U32_MASK 0xFFFFFFFF

#define	MAX_SLOTS	128
#define MAX_CUS		128
#define MAX_U32_SLOT_MASKS (((MAX_SLOTS-1)>>5) + 1)
#define MAX_U32_CU_MASKS (((MAX_CUS-1)>>5) + 1)

namespace xclcpuemhal2 {
  class CpuemShim;
  class xocl_cmd;
  class SWScheduler;
  class exec_core;

  struct client_ctx 
  {
    int		trigger;
    std::mutex mLock;
  };

  class xocl_sched
  {
    public:
      std::thread                 scheduler_thread;
      std::condition_variable_any    state_cond;
      std::list<std::shared_ptr<xocl_cmd>>   command_queue;
      bool                        bThreadCreated;
      unsigned int                error;
      int                         intc;
      int                         poll;
      bool                        stop;
      std::weak_ptr<SWScheduler>  pSch;
      xocl_sched(std::shared_ptr<SWScheduler>);
      ~xocl_sched();
  };

  class xocl_cu 
  {
    public:
      unsigned int       idx;
      bool               dataflow;
      uint32_t           base;
      uint32_t           addr;
      uint32_t           polladdr;
      uint32_t           ctrlreg;
      unsigned int       done_cnt;
      unsigned int       run_cnt;
      std::queue<std::shared_ptr<xocl_cmd>>         running_queue;
      xocl_cu();
      ~xocl_cu();
  };

  class xocl_cmd
  {
    public:
      std::weak_ptr<xclemulation::drm_xocl_bo> bo;
      std::weak_ptr<exec_core> exec;
      enum ert_cmd_state state;
      unsigned int cu_idx;
      int slot_idx;
      /* The actual cmd object representation */
      struct ert_packet *packet;
      xocl_cmd();
      ~xocl_cmd();
  };

  class exec_core 
  {
    public:
      exec_core();
      ~exec_core();
    uint64_t base;
    uint32_t			  intr_base;
    uint32_t			  intr_num;

    std::list<std::shared_ptr<client_ctx>>     ctx_list;
    std::shared_ptr<xocl_sched>          scheduler;
    std::shared_ptr<xocl_cmd>   submitted_cmds[MAX_SLOTS];

    unsigned int               num_slots;
    unsigned int               num_cus;
    unsigned int               num_cdma;
    unsigned int               cu_shift_offset;
    uint32_t                   cu_base_addr;
    unsigned int               polling_mode;
    unsigned int               cq_interrupt;
    unsigned int               configured;

    /* Bitmap tracks busy(1)/free(0) slots in cmd_slots*/
    uint32_t                   slot_status[MAX_U32_SLOT_MASKS];
    unsigned int               num_slot_masks; /* ((num_slots-1)>>5)+1 */

    uint32_t                   cu_status[MAX_U32_CU_MASKS];
    unsigned int               num_cu_masks; /* ((num_cus-1)>>5+1 */
    uint32_t                   cu_addr_map[MAX_CUS];
    std::shared_ptr<xocl_cu>   cus[MAX_CUS];
    uint32_t                   cu_usage[MAX_CUS];
    bool                       ertfull;
    bool                       ertpoll;

    /* Status register pending complete.  Written by ISR, cleared
       by scheduler */
    int                   sr0;
    int                   sr1;
    int                   sr2;
    int                   sr3;

  };

  class SWScheduler: public std::enable_shared_from_this<SWScheduler>
  {
    public:
    void set_cmd_int_state(std::shared_ptr<xocl_cmd>& xcmd, enum ert_cmd_state state) { xcmd->state = state; }
    void set_cmd_state(std::shared_ptr<xocl_cmd>& xcmd, enum ert_cmd_state state) { xcmd->state = state; xcmd->packet->state = state; }
    bool is_ert(std::shared_ptr<exec_core> exec) { return true; }
    int ffz(uint32_t mask) { return( log2( ~mask & (mask+1) )); }
    int ffz_or_neg_one(uint32_t mask){
      if (mask==XOCL_U32_MASK) return -1;
      return ffz(mask);
    }

    unsigned int slot_size(std::shared_ptr<exec_core>& exec)   { return ERT_CQ_SIZE / exec->num_slots; }
    unsigned int cu_mask_idx(unsigned int cu_idx)    { return cu_idx >> 5; /* 32 cus per mask */ }
    unsigned int cu_idx_in_mask(unsigned int cu_idx) { return cu_idx - (cu_mask_idx(cu_idx) << 5); }
    unsigned int cu_idx_from_mask(unsigned int cu_idx, unsigned int mask_idx) { return cu_idx + (mask_idx << 5); }
    unsigned int slot_mask_idx(unsigned int slot_idx) { return slot_idx >> 5; }
    unsigned int slot_idx_in_mask(unsigned int slot_idx) { return slot_idx - (slot_mask_idx(slot_idx) << 5); }
    unsigned int slot_idx_from_mask_idx(unsigned int slot_idx,unsigned int mask_idx) { return slot_idx + (mask_idx << 5); }
    uint32_t opcode(std::shared_ptr<xocl_cmd>& xcmd) { return xcmd->packet->opcode; }
    uint32_t payload_size(std::shared_ptr<xocl_cmd>& xcmd) { return xcmd->packet->count; }
    uint32_t packet_size(std::shared_ptr<xocl_cmd>& xcmd) { return payload_size(xcmd) + 1; }
    uint32_t type(struct std::shared_ptr<xocl_cmd>& xcmd) { return xcmd->packet->type; }

    void mb_query(std::shared_ptr<xocl_cmd>& xcmd);
    int mb_submit(std::shared_ptr<xocl_cmd>& xcmd);
    void penguin_query(std::shared_ptr<xocl_cmd>& xcmd);
    int penguin_submit(std::shared_ptr<xocl_cmd>& xcmd);
    void ert_poll_query(std::shared_ptr<xocl_cmd>& xcmd);
    int ert_poll_submit(std::shared_ptr<xocl_cmd>& xcmd);
    void ert_poll_query_ctrl(std::shared_ptr<xocl_cmd>& xcmd);
    int ert_poll_submit_ctrl(std::shared_ptr<xocl_cmd>& xcmd);
    int acquire_slot(struct std::shared_ptr<xocl_cmd>& xcmd);
    int acquire_slot_idx(std::shared_ptr<exec_core>& exec);
    int configure(std::shared_ptr<xocl_cmd>& xcmd);
    void release_slot_idx(std::shared_ptr<exec_core>& exec, unsigned int slot_idx);
    void notify_host(std::shared_ptr<xocl_cmd>& xcmd);
    void mark_cmd_complete(std::shared_ptr<xocl_cmd>& xcmd);
    void mark_mask_complete(std::shared_ptr<exec_core>& exec, uint32_t mask, unsigned int mask_idx);
    int queued_to_running(std::shared_ptr<xocl_cmd>& xcmd) ;
    void running_to_complete(std::shared_ptr<xocl_cmd>& xcmd) ;
    void complete_to_free(std::shared_ptr<xocl_cmd>& xcmd) { }
    std::shared_ptr<xocl_cmd> get_free_xocl_cmd(void);
    int add_cmd(std::shared_ptr<exec_core> &exec, std::shared_ptr<xclemulation::drm_xocl_bo> &bo);
    int scheduler_wait_condition();
    void scheduler_queue_cmds();
    void scheduler_iterate_cmds();
    int get_free_cu(std::shared_ptr<xocl_cmd>& xcmd);
    void configure_cu(std::shared_ptr<xocl_cmd>& xcmd, int cu_idx);
    bool cu_done(std::shared_ptr<exec_core>& exec, unsigned int cu_idx);
    uint32_t cu_masks(std::shared_ptr<xocl_cmd>& xcmd);
    uint32_t regmap_size(std::shared_ptr<xocl_cmd>& xcmd);
    bool cmd_has_cu(std::shared_ptr<xocl_cmd>& xcmd, uint32_t f_cu_idx);
    void cu_configure_ooo(std::shared_ptr<xocl_cu>& xcu, std::shared_ptr<xocl_cmd>& xcmd);
    void cu_configure_ino(std::shared_ptr<xocl_cu>& xcu, std::shared_ptr<xocl_cmd>& xcmd);
    std::shared_ptr<xocl_cmd> cu_first_done(std::shared_ptr<xocl_cu>& xcu);
    void cu_pop_done(std::shared_ptr<xocl_cu>& xcu);
    void cu_continue(std::shared_ptr<xocl_cu>& xcu);
    void cu_poll(std::shared_ptr<xocl_cu>& xcu);
    bool cu_ready(std::shared_ptr<xocl_cu>& xcu);
    bool cu_start(std::shared_ptr<xocl_cu>& xcu, std::shared_ptr<xocl_cmd>& xcmd);

    void scheduler_loop();
    void scheduler() ;

    int init_scheduler_thread(void) ;
    int fini_scheduler_thread(void) ;
    int add_exec_buffer(std::shared_ptr<exec_core> &exec, std::shared_ptr<xclemulation::drm_xocl_bo> &buf);//exec_core *eCore , xclemulation::drm_xocl_bo *buf) ;
    int convert_execbuf(std::shared_ptr<exec_core>& exec, std::shared_ptr<xclemulation::drm_xocl_bo>& xobj, std::shared_ptr<xocl_cmd>& xcmd);

    std::shared_ptr<xocl_sched> mScheduler;
    SWScheduler(std::shared_ptr<CpuemShim> _parent);
    ~SWScheduler();
    std::weak_ptr<CpuemShim> mParent;
    private:
    std::list<std::shared_ptr<xocl_cmd>> free_cmds;
    std::mutex free_cmds_mutex;

    std::list<std::shared_ptr<xocl_cmd>> pending_cmds;
    std::mutex pending_cmds_mutex;

    std::mutex m_add_cmd_mutex;
    int num_pending;
  };
}

#endif
