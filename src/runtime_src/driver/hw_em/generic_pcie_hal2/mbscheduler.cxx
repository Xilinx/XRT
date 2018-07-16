#include "shim.h"
#include <algorithm>
//#define EM_DEBUG_KDS
namespace xclhwemhal2 {

  xocl_cmd::xocl_cmd()
  {
    bo = NULL;
    exec = NULL;
    cu_idx = 0;
    slot_idx = 0;
    packet = NULL;
    state = ERT_CMD_STATE_NEW;
  }

  xocl_cmd::~xocl_cmd()
  {
    bo = NULL;
    exec = NULL;
    cu_idx = 0;
    slot_idx = 0;
    packet = NULL;
  }

  xocl_sched::xocl_sched (MBScheduler* _sch)
  {
    bThreadCreated = false;
    error = 0;
    intc = 0;
    poll = 0;
    stop = false;
    pSch = _sch ;
    pthread_mutex_init(&state_lock,NULL);
    pthread_cond_init(&state_cond,NULL);
    scheduler_thread = 0;
  }

  xocl_sched::~xocl_sched()
  {
    bThreadCreated = false;
    error = 0;
    intc = 0;
    poll = 0;
    stop = false;
    pSch = NULL ;
    pthread_mutex_init(&state_lock,NULL);
    pthread_cond_init(&state_cond,NULL);
  }

  exec_core::exec_core()
  {
    base = 0;
    intr_base = 0;
    intr_num = 0;

    scheduler = NULL;

    num_slots = 0;
    num_cus = 0;
    cu_shift_offset = 0;
    cu_base_addr = 0;
    polling_mode = 1;
    cq_interrupt = 0;
    configured = 0;

    num_cu_masks = 0;
    for (unsigned i=0; i<MAX_U32_SLOT_MASKS; ++i)
      slot_status[i] = 0;

    for(unsigned i=0; i <MAX_SLOTS; ++i)
      submitted_cmds[i] = NULL;
      
    num_slot_masks = 1;

    sr0 = 0;
    sr1 = 0;
    sr2 = 0;
    sr3 = 0;
  }

  exec_core::~exec_core()
  {
  }

  MBScheduler::MBScheduler(HwEmShim* _parent)
  {
    mParent = _parent;
    mScheduler = new xocl_sched(this);
    num_pending = 0;
  }

  MBScheduler::~MBScheduler()
  {
    delete mScheduler;
    mScheduler = NULL;
    num_pending = 0;
  }

  void MBScheduler::mb_query(xocl_cmd *xcmd)
  {
    exec_core *exec = xcmd->exec;
    unsigned int cmd_mask_idx = slot_mask_idx(xcmd->slot_idx);


    if (exec->polling_mode
        || (cmd_mask_idx==0 && exec->sr0)
        || (cmd_mask_idx==1 && exec->sr1)
        || (cmd_mask_idx==2 && exec->sr2)
        || (cmd_mask_idx==3 && exec->sr3)) {
      uint32_t csr_addr = ERT_STATUS_REGISTER_ADDR + (cmd_mask_idx<<2);
      //TODO
      uint32_t mask = 0;
      bool waitForResp = false;
      if (opcode(xcmd)==ERT_CONFIGURE)
        waitForResp = true;
      do{
        mParent->xclRead(XCL_ADDR_KERNEL_CTRL, xcmd->exec->base + csr_addr, (void*)&mask, 4);
      }while(waitForResp && !mask);
      
      if (mask)
      {
#ifdef EM_DEBUG_KDS
        std::cout<<"Mask is non-zero. Mark respective command complete "<< mask << std::endl;
#endif
        mark_mask_complete(xcmd->exec,mask,cmd_mask_idx);
      }
    }
  }

  int MBScheduler::acquire_slot_idx(exec_core *exec)
  {
    unsigned int mask_idx=0, slot_idx=-1;
    uint32_t mask;
    for (mask_idx=0; mask_idx<exec->num_slot_masks; ++mask_idx) 
    {
      mask = exec->slot_status[mask_idx];
      slot_idx = ffz_or_neg_one(mask);
      if(slot_idx < 0)
        continue;
      if (slot_idx_from_mask_idx(slot_idx,mask_idx)>=exec->num_slots)
        continue;
      exec->slot_status[mask_idx] ^= (1<<slot_idx);
      int rSlot = slot_idx_from_mask_idx(slot_idx,mask_idx);
      return rSlot;
    }
    return -1;
  }

  int MBScheduler::mb_submit(xocl_cmd *xcmd)
  {
    uint32_t slot_addr;

    xcmd->slot_idx = acquire_slot_idx(xcmd->exec);
#ifdef EM_DEBUG_KDS
    std::cout<<"Acquring slot index "<<xcmd->slot_idx<<" for CXMD: "<<xcmd<<" PACKET: "<<xcmd->packet<< " BO: "<< xcmd->bo << std::endl;
#endif
    if (xcmd->slot_idx<0) {
      return false;
    }

    slot_addr = ERT_CQ_BASE_ADDR + xcmd->slot_idx*slot_size(xcmd->exec);

    /* TODO write packet minus header */
    mParent->xclWrite(XCL_ADDR_KERNEL_CTRL, xcmd->exec->base + slot_addr + 4, xcmd->packet->data,(packet_size(xcmd)-1)*sizeof(uint32_t)); 
    //memcpy_toio(xcmd->exec->base + slot_addr + 4,xcmd->packet->data,(packet_size(xcmd)-1)*sizeof(uint32_t));

    /* TODO write header */
    mParent->xclWrite(XCL_ADDR_KERNEL_CTRL, xcmd->exec->base + slot_addr, (void*)(&xcmd->packet->header) ,4); 
    //iowrite32(xcmd->packet->header,xcmd->exec->base + slot_addr);

    /* trigger interrupt to embedded scheduler if feature is enabled */
    if (xcmd->exec->cq_interrupt) {
      uint32_t cq_int_addr = ERT_CQ_STATUS_REGISTER_ADDR + (slot_mask_idx(xcmd->slot_idx)<<2);
      uint32_t mask = 1<<slot_idx_in_mask(xcmd->slot_idx);
      //TODO 
      mParent->xclWrite(XCL_ADDR_KERNEL_CTRL,xcmd->exec->base + cq_int_addr, (void*)(&mask) ,4);
        //iowrite32(mask,xcmd->exec->base + cq_int_addr);
    }
#ifdef EM_DEBUG_KDS
    std::cout<<"Submitted the command CXMD: "<<xcmd<<" PACKET: "<<xcmd->packet<< " BO: "<< xcmd->bo << std::endl <<std::endl;;
#endif

    return true;
  }

  int MBScheduler::configure(xocl_cmd *xcmd)
  {
    exec_core *exec=xcmd->exec;
    struct ert_configure_cmd *cfg;

    cfg = (struct ert_configure_cmd *)(xcmd->packet);

    if (exec->configured==0) 
    {
      exec->base = 0;
      exec->num_slot_masks = 1;
      exec->num_slots = ERT_CQ_SIZE / cfg->slot_size;
      exec->num_cus = cfg->num_cus;
      exec->cu_shift_offset = cfg->cu_shift;
      exec->cu_base_addr = cfg->cu_base_addr;
      exec->num_cu_masks = ((exec->num_cus-1)>>5) + 1;

      if (cfg->ert) 
      {
        exec->polling_mode = 1; //cfg->polling;
        exec->cq_interrupt = cfg->cq_int;

      }
      else 
      {
        std::cout<<"ERT not enabled "<<std::endl;
      }
      return 0;
    }

    return 1;
  }

  void MBScheduler::release_slot_idx(exec_core *exec, unsigned int slot_idx)
  {
    unsigned int mask_idx = slot_mask_idx(slot_idx);
    unsigned int pos = slot_idx_in_mask(slot_idx);
    exec->slot_status[mask_idx] ^= (1<<pos);
  }

  void MBScheduler::notify_host(xocl_cmd *xcmd)
  {
    exec_core *exec = xcmd->exec;

    /* now for each client update the trigger counter in the context */
    for(auto it: exec->ctx_list)
    {
      client_ctx* entry = it;
      entry->trigger++;
    }
  }

  void MBScheduler::mark_cmd_complete(xocl_cmd *xcmd)
  {
    xcmd->exec->submitted_cmds[xcmd->slot_idx] = NULL;
    set_cmd_state(xcmd,ERT_CMD_STATE_COMPLETED);
    if (xcmd->exec->polling_mode)
      mScheduler->poll--;
    release_slot_idx(xcmd->exec,xcmd->slot_idx);
#ifdef EM_DEBUG_KDS
    std::cout<<"Marking command Complete XCMD: " <<xcmd<<" PACKET: "<<xcmd->packet<< " BO: "<< xcmd->bo << std::endl;
    std::cout<<"Releasing slot " << xcmd->slot_idx << std::endl<<std::endl;
#endif
    notify_host(xcmd);
  }

  void MBScheduler::mark_mask_complete(exec_core *exec, uint32_t mask, unsigned int mask_idx)
  {
#ifdef EM_DEBUG_KDS
    std::cout<<"Marking some commands complete" << std::endl;
#endif
    int bit_idx=0,cmd_idx=0;
    if (!mask)
      return;
    for (bit_idx=0, cmd_idx=mask_idx<<5; bit_idx<32; mask>>=1,++bit_idx,++cmd_idx)
    {
      if (mask & 0x1)
      {
        if(exec->submitted_cmds[cmd_idx])
        {
          mark_cmd_complete(exec->submitted_cmds[cmd_idx]);
        }
      }
    }
  }

  int MBScheduler::queued_to_running(xocl_cmd *xcmd)
  {
    int retval = false;
    if (opcode(xcmd)==ERT_CONFIGURE)
    {
#ifdef EM_DEBUG_KDS
    std::cout<<"Configure command has started. XCMD " <<xcmd<<" PACKET: "<<xcmd->packet<< " BO: "<< xcmd->bo << std::endl;
#endif
      configure(xcmd);
    }

    if (mb_submit(xcmd)) {
      set_cmd_state(xcmd,ERT_CMD_STATE_RUNNING);
      if (xcmd->exec->polling_mode)
        mScheduler->poll++;
      xcmd->exec->submitted_cmds[xcmd->slot_idx] = xcmd;
      retval = true;
    }

    return retval;
  }

  void MBScheduler::running_to_complete(xocl_cmd *xcmd)
  {
    mb_query(xcmd);
  }

  xocl_cmd* MBScheduler::get_free_xocl_cmd(void)
  {
    xocl_cmd* cmd = new xocl_cmd;
    return cmd;
  } 
  
  int MBScheduler::add_cmd(exec_core *exec, xclemulation::drm_xocl_bo* bo)
  {
    std::lock_guard<std::mutex> lk(pending_cmds_mutex);
    xocl_cmd *xcmd = get_free_xocl_cmd();
    xcmd->packet = (struct ert_packet*)bo->buf;
    xcmd->bo=bo;
    xcmd->exec=exec;
    xcmd->cu_idx=-1;
    xcmd->slot_idx=-1;
#ifdef EM_DEBUG_KDS
    std::cout<<"adding a command CMD: " <<xcmd<<" PACKET: "<<xcmd->packet<< " BO: "<< xcmd->bo <<" BASE: "<<xcmd->bo->base<< std::endl;
#endif

    set_cmd_state(xcmd,ERT_CMD_STATE_NEW);
    pending_cmds.push_back(xcmd);
    num_pending++;
    scheduler_wait_condition();
    return 0;
  }

  
  
  int MBScheduler::scheduler_wait_condition()
  {
    bool bSchComeOutOfCond = false;
    if (mScheduler->stop || mScheduler->error) {
      bSchComeOutOfCond = true;
    }

    if (num_pending > 0) {
      bSchComeOutOfCond = true;
    }

    if (mScheduler->intc > 0) {
      mScheduler->intc = 0;
      bSchComeOutOfCond = true;
    }

    if (mScheduler->poll >0 ) {
      bSchComeOutOfCond = true;
    }
    if(bSchComeOutOfCond)
    {
      pthread_cond_signal(&mScheduler->state_cond);
      return 0;
    }
    return 1;
  }

  void MBScheduler::scheduler_queue_cmds()
  {
    if(pending_cmds.empty())
      return;

#ifdef EM_DEBUG_KDS
    std::cout<<"Iterating on pending commands and adding to Scheduler command_queue  "<< std::endl;
#endif
    for(auto it: pending_cmds)
    {
      xocl_cmd *xcmd = it;
      mScheduler->command_queue.push_back(xcmd);
      xcmd->state = ERT_CMD_STATE_QUEUED;
#ifdef EM_DEBUG_KDS
    std::cout<<xcmd <<" ADDED to Scheduler command_queue  "<< std::endl;
#endif
      num_pending--;
    }
    pending_cmds.clear();
  }

  void MBScheduler::scheduler_iterate_cmds()
  {
     auto end = mScheduler->command_queue.end();
#ifdef EM_DEBUG_KDS
     //if(mScheduler->command_queue.size() > 0)
     //  std::cout<<" command_queue size is "<<mScheduler->command_queue.size()<< std::endl;
#endif
     for (auto itr=mScheduler->command_queue.begin(); itr!=end; ) 
     {
       xocl_cmd *xcmd = *itr;
       if (xcmd->state == ERT_CMD_STATE_QUEUED)
       {
#ifdef EM_DEBUG_KDS
         std::cout<<xcmd << " is in QUEUED state  "<< std::endl;
#endif
         queued_to_running(xcmd);
       }
       if (xcmd->state == ERT_CMD_STATE_RUNNING)
       {
         running_to_complete(xcmd);
       }
       
       if (xcmd->state == ERT_CMD_STATE_COMPLETED)
       {
#ifdef EM_DEBUG_KDS
         std::cout<<xcmd << " is in COMPLETED state  "<< std::endl;
#endif
         complete_to_free(xcmd);
         itr = mScheduler->command_queue.erase(itr);
         end = mScheduler->command_queue.end();
       }
       else {
         ++itr;
       }
     }

  }

  void scheduler_loop(xocl_sched *xs)
  {
    MBScheduler* pSch = xs->pSch;
    std::lock_guard<std::mutex> lk(pSch->pending_cmds_mutex);

    if (xs->error) { return; }

    /* queue new pending commands */
    pSch->scheduler_queue_cmds();

    /* iterate all commands */
    pSch->scheduler_iterate_cmds();
  }

  void* scheduler(void* data)
  {
    xocl_sched *xs = (xocl_sched *)data;
    while (!xs->stop && !xs->error)
    {
      scheduler_loop(xs);
      usleep(10);
    }
    return NULL;
  }

  int MBScheduler::init_scheduler_thread(void)
  {

    if (mScheduler->bThreadCreated)
      return 0;

#ifdef EM_DEBUG_KDS
    std::cout<<"Scheduler Thread started "<< std::endl;
#endif

    int returnStatus  =  pthread_create(&(mScheduler->scheduler_thread) , NULL, scheduler, (void *)mScheduler);

    if (returnStatus != 0) 
    {
      std::cout << __func__ <<  " pthread_create failed " << " " << returnStatus<< std::endl;
      exit(1);
    }
    mScheduler->bThreadCreated = true;

    return 0;
  }
  
  int MBScheduler::fini_scheduler_thread(void)
  {
    if (!mScheduler->bThreadCreated)
      return 0;

#ifdef EM_DEBUG_KDS
    std::cout<<"Scheduler Thread ended "<< std::endl;
#endif

    mScheduler->stop= true;
    scheduler_wait_condition();
    mScheduler->bThreadCreated = false;

    int retval = pthread_join(mScheduler->scheduler_thread,NULL);

    pending_cmds.clear();
    mScheduler->command_queue.clear();
    free_cmds.clear();

    return retval;
  } 

  int MBScheduler::add_exec_buffer(exec_core* exec, xclemulation::drm_xocl_bo *buf)
  {
    return add_cmd(exec, buf);
  }
}
