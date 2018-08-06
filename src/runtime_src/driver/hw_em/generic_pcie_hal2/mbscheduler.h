#ifndef _MB_SCHEDULER_H_
#define _MB_SCHEDULER_H_

#include <list>
#include <mutex>
#include <math.h>
#include <stdint.h>
#include "ert.h"

#define XOCL_U32_MASK 0xFFFFFFFF

#define	MAX_SLOTS	128
#define MAX_CUS		128
#define MAX_U32_SLOT_MASKS (((MAX_SLOTS-1)>>5) + 1)
#define MAX_U32_CU_MASKS (((MAX_CUS-1)>>5) + 1)

namespace xclhwemhal2 {
  class HwEmShim;
  class xocl_cmd;
  class MBScheduler;
  class exec_core;

  struct client_ctx 
  {
    int		trigger;
    std::mutex mLock;
  };
  
  class xocl_sched
  {
    public:
      pthread_t                   scheduler_thread;
      pthread_mutex_t             state_lock;
      pthread_cond_t              state_cond;
      std::list<xocl_cmd*>        command_queue;
      bool                        bThreadCreated;
      unsigned int                error;
      int                         intc;
      int                         poll;
      bool                        stop;
      MBScheduler*              pSch;
      xocl_sched(MBScheduler*);
      ~xocl_sched();
  };
  
  class xocl_cmd
  {
    public:
      xclemulation::drm_xocl_bo *bo;
      exec_core *exec;
      enum ert_cmd_state state;
      int cu_idx;
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

    std::list<client_ctx*>           ctx_list;

    struct xocl_sched          *scheduler;

    xocl_cmd*            submitted_cmds[MAX_SLOTS];

    unsigned int               num_slots;
    unsigned int               num_cus;
    unsigned int               cu_shift_offset;
    uint32_t                   cu_base_addr;
    unsigned int               polling_mode;
    unsigned int               cq_interrupt;
    unsigned int               configured;

    /* Bitmap tracks busy(1)/free(0) slots in cmd_slots*/
    uint32_t                        slot_status[MAX_U32_SLOT_MASKS];
    unsigned int               num_slot_masks; /* ((num_slots-1)>>5)+1 */

    uint32_t                        cu_status[MAX_U32_CU_MASKS];
    unsigned int               num_cu_masks; /* ((num_cus-1)>>5+1 */

    /* Status register pending complete.  Written by ISR, cleared
       by scheduler */
    int                   sr0;
    int                   sr1;
    int                   sr2;
    int                   sr3;

  };

  class MBScheduler
  {
    public:
    void set_cmd_int_state(xocl_cmd* xcmd, enum ert_cmd_state state) { xcmd->state = state; }
    void set_cmd_state(xocl_cmd* xcmd, enum ert_cmd_state state) { xcmd->state = state; xcmd->packet->state = state; }
    bool is_ert(exec_core *exec) { return true; }
    int ffz(uint32_t mask) { return( log2( ~mask & (mask+1) )); }
    int ffz_or_neg_one(uint32_t mask){
      if (mask==XOCL_U32_MASK) return -1;
      return ffz(mask);
    }
    
    unsigned int slot_size(exec_core *exec)   { return ERT_CQ_SIZE / exec->num_slots; }
    unsigned int cu_mask_idx(unsigned int cu_idx)    { return cu_idx >> 5; /* 32 cus per mask */ }
    unsigned int cu_idx_in_mask(unsigned int cu_idx) { return cu_idx - (cu_mask_idx(cu_idx) << 5); }
    unsigned int cu_idx_from_mask(unsigned int cu_idx, unsigned int mask_idx) { return cu_idx + (mask_idx << 5); }
    unsigned int slot_mask_idx(unsigned int slot_idx) { return slot_idx >> 5; }
    unsigned int slot_idx_in_mask(unsigned int slot_idx) { return slot_idx - (slot_mask_idx(slot_idx) << 5); }
    unsigned int slot_idx_from_mask_idx(unsigned int slot_idx,unsigned int mask_idx) { return slot_idx + (mask_idx << 5); }
    uint32_t opcode(xocl_cmd* xcmd) { return xcmd->packet->opcode; }
    uint32_t payload_size(xocl_cmd *xcmd) { return xcmd->packet->count; }
    uint32_t packet_size(xocl_cmd *xcmd) { return payload_size(xcmd) + 1; }
    void mb_query(xocl_cmd *xcmd);
    int mb_submit(xocl_cmd *xcmd);
    int acquire_slot_idx(exec_core *exec);
    int configure(xocl_cmd *xcmd);
    void release_slot_idx(exec_core *exec, unsigned int slot_idx);
    void notify_host(xocl_cmd *xcmd);
    void mark_cmd_complete(xocl_cmd *xcmd);
    void mark_mask_complete(exec_core *exec, uint32_t mask, unsigned int mask_idx);
    int queued_to_running(xocl_cmd *xcmd) ;
    void running_to_complete(xocl_cmd *xcmd) ;
    void complete_to_free(xocl_cmd *xcmd) { }
    xocl_cmd* get_free_xocl_cmd(void) ; 
    int add_cmd(exec_core *exec, xclemulation::drm_xocl_bo* bo) ;
    int scheduler_wait_condition() ;
    void scheduler_queue_cmds();
    void scheduler_iterate_cmds();
    
    friend void scheduler_loop(xocl_sched *xs);
    friend void* scheduler(void* data) ;

    int init_scheduler_thread(void) ;
    int fini_scheduler_thread(void) ;
    int add_exec_buffer(exec_core *eCore , xclemulation::drm_xocl_bo *buf) ;

    xocl_sched* mScheduler;
    MBScheduler(HwEmShim* _parent);
    ~MBScheduler();
    HwEmShim* mParent;
    private:
    std::list<xocl_cmd*> free_cmds;
    std::mutex free_cmds_mutex;
    
    std::list<xocl_cmd*> pending_cmds;
    std::mutex pending_cmds_mutex;
    
    std::mutex m_add_cmd_mutex;
    int num_pending;
  };
}

#endif
