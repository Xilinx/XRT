/* 
 * SPDX-License-Identifier: Apache-2.0
   Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
 */

#include "shim.h"
#include <algorithm>
//#define EM_DEBUG_KDS
#define PRINTSTARTFUNC
//#define PRINTSTARTFUNC std::cout <<"swscheduler: " <<__func__ << " begin " << std::endl;
namespace xclcpuemhal2 {

  xocl_cmd::xocl_cmd()
  {
    cu_idx = 0;
    slot_idx = 0;
    packet = NULL;
    state = ERT_CMD_STATE_NEW;
  }

  xocl_cmd::~xocl_cmd()
  {
    cu_idx = 0;
    slot_idx = 0;
    packet = NULL;
  }

  xocl_sched::xocl_sched (std::shared_ptr<SWScheduler> _sch)
  {
    bThreadCreated = false;
    error = 0;
    intc = 0;
    poll = 0;
    stop = false;
    pSch = std::weak_ptr{_sch} ;
  }

  xocl_sched::~xocl_sched()
  {
    bThreadCreated = false;
    error = 0;
    intc = 0;
    poll = 0;
    stop = false;
  }

  exec_core::exec_core()
  {
    base = 0;
    intr_base = 0;
    intr_num = 0;

    scheduler = NULL;

    num_slots = 0;
    num_cus = 0;
    num_cdma = 0;
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

    for (unsigned int i=0; i<MAX_CUS; ++i)
    {
      cu_addr_map[i] = 0;
      cu_usage[i] = 0;
      cus[i] = nullptr;
    }

    for (unsigned int i=0; i<MAX_U32_CU_MASKS; ++i)
      cu_status[i] = 0;

    ertfull = false;
    ertpoll = true;

    num_slot_masks = 1;

    sr0 = 0;
    sr1 = 0;
    sr2 = 0;
    sr3 = 0;
  }

  exec_core::~exec_core()
  {
  }

  xocl_cu::xocl_cu()
  {
    idx = 0;
    base = 0;
    dataflow = false;
    addr = 0;
    polladdr = 0;
    ctrlreg = 0;
    done_cnt = 0;
    run_cnt = 0;
  }

  xocl_cu::~xocl_cu()
  {
    idx = 0;
    base = 0;
    dataflow = false;
    addr = 0;
    polladdr = 0;
    ctrlreg = 0;
    done_cnt = 0;
    run_cnt = 0;
  }

  void SWScheduler::cu_continue(std::shared_ptr<xocl_cu>& xcu)
  {
    PRINTSTARTFUNC
    if (!xcu->dataflow)//TODO
      return;

    // acknowledge done directly to CU (xcu->addr)
    mParent.lock()->xclWrite(XCL_ADDR_KERNEL_CTRL, xcu->base + xcu->addr, (void*)&CpuemShim::CONTROL_AP_CONTINUE, 4);

    // in ert_poll mode acknowlegde done to ERT
    if (xcu->polladdr && xcu->run_cnt) {
      mParent.lock()->xclWrite(XCL_ADDR_KERNEL_CTRL,xcu->base + xcu->polladdr, (void*)&CpuemShim::CONTROL_AP_CONTINUE, 4);
    }
  }

  void SWScheduler::cu_poll(std::shared_ptr<xocl_cu>& xcu)
  {
    PRINTSTARTFUNC
    mParent.lock()->xclRead(XCL_ADDR_KERNEL_CTRL,xcu->base + xcu->addr,(void*)&(xcu->ctrlreg),4);
    if (xcu->run_cnt && (xcu->ctrlreg & (CpuemShim::CONTROL_AP_DONE | CpuemShim::CONTROL_AP_IDLE)))
    {
      ++xcu->done_cnt;
      --xcu->run_cnt;
      cu_continue(xcu);
    }
  }

  bool SWScheduler::cu_ready(std::shared_ptr<xocl_cu>& xcu)
  {
    PRINTSTARTFUNC
    if ((xcu->ctrlreg & CpuemShim::CONTROL_AP_START) || (!xcu->dataflow && xcu->run_cnt))
      cu_poll(xcu);

    bool bReady = xcu->dataflow ? !(xcu->ctrlreg & CpuemShim::CONTROL_AP_START) : xcu->run_cnt == 0;
    return bReady;
  }

  static inline uint32_t* cmd_regmap(std::shared_ptr<xocl_cmd>& xcmd)
  {
    PRINTSTARTFUNC
    struct ert_start_kernel_cmd *ecmd = (struct ert_start_kernel_cmd *)xcmd->packet;
    return ecmd->data + ecmd->extra_cu_masks;
  }

  void SWScheduler::cu_configure_ino(std::shared_ptr<xocl_cu>& xcu, std::shared_ptr<xocl_cmd>& xcmd)
  {
    PRINTSTARTFUNC
    unsigned int size = regmap_size(xcmd);
    uint32_t *regmap = cmd_regmap(xcmd);
    //unsigned int idx;
    regmap[0] = 0;
    mParent.lock()->xclWrite(XCL_ADDR_KERNEL_CTRL, xcu->base + xcu->addr , (void*)(regmap), size*4);
   /* for (idx = 4; idx < size; ++idx)
    {
      mParent->xclWrite(XCL_ADDR_KERNEL_CTRL, xcu->base + xcu->addr + (idx << 2), (void*)(regmap+idx) ,4);
    }*/
  }

  void SWScheduler::cu_configure_ooo(std::shared_ptr<xocl_cu>& xcu, std::shared_ptr<xocl_cmd>& xcmd)
  {
    PRINTSTARTFUNC
    unsigned int size = regmap_size(xcmd);
    uint32_t *regmap = cmd_regmap(xcmd);
    unsigned int idx;

    for (idx = 4; idx < size - 1; idx += 2)
    {
      uint32_t offset = *(regmap + idx);
      uint32_t val = *(regmap + idx + 1);
      mParent.lock()->xclWrite(XCL_ADDR_KERNEL_CTRL, xcu->base + offset , (void*)(&val) ,4);
    }
  }

  bool SWScheduler::cu_start(std::shared_ptr<xocl_cu>& xcu, std::shared_ptr<xocl_cmd>& xcmd)
  {
    PRINTSTARTFUNC
    /* write register map, starting at base + 0x10
     * 0x0 used for control register
     * 0x4, 0x8 used for interrupt, which is initialized in setup of ERT
     * 0xC used for interrupt status, which is set by hardware
     */
    if (opcode(xcmd) == ERT_EXEC_WRITE)
      cu_configure_ooo(xcu, xcmd);
    else
      cu_configure_ino(xcu, xcmd);

    // start cu.  update local state as we may not be polling prior
    // to next ready check.
    unsigned int size = regmap_size(xcmd);
    uint32_t *regmap = cmd_regmap(xcmd);

    xcu->ctrlreg |= CpuemShim::CONTROL_AP_START;
    const_cast<uint32_t*>(regmap)[0] = CpuemShim::CONTROL_AP_START;
    //mParent->xclWrite(XCL_ADDR_KERNEL_CTRL, xcu->base + xcu->addr,(void*)& CpuemShim::CONTROL_AP_START, 4);
    mParent.lock()->xclWrite(XCL_ADDR_KERNEL_CTRL, xcu->base + xcu->addr, (void*)(regmap), size*4);
    // in ert poll mode request ERT to poll CU
    if (xcu->polladdr) {
      mParent.lock()->xclWrite(XCL_ADDR_KERNEL_CTRL, xcu->base + xcu->polladdr, (void*)& CpuemShim::CONTROL_AP_START, 4);
    }

    ++xcu->run_cnt;

    return true;
  }


  std::shared_ptr<xocl_cmd> SWScheduler::cu_first_done(std::shared_ptr<xocl_cu>& xcu)
  {
    PRINTSTARTFUNC
    if (!xcu->done_cnt && xcu->run_cnt)
      cu_poll(xcu);

    return xcu->done_cnt ? (xcu->running_queue).front() : nullptr;
  }

  void SWScheduler::cu_pop_done(std::shared_ptr<xocl_cu>& xcu)
  {
    PRINTSTARTFUNC
    if (!xcu->done_cnt)
      return;

    //	struct xocl_cmd *xcmd;
    //xcmd = (xcu->running_queue).front();
    (xcu->running_queue).pop();
    --xcu->done_cnt;
  }

  unsigned int getFirstSetBitPos(int n)
  {
    PRINTSTARTFUNC
    if(!n)
      return -1;
    return log2(n & -n) ;
  }

  bool isKthBitSet(int n, int k)
  {
    PRINTSTARTFUNC
    if (n & (1 << (k)))
      return true;
    else
      return false;
  }

  bool SWScheduler::cmd_has_cu(std::shared_ptr<xocl_cmd>&  xcmd, uint32_t f_cu_idx)
  {
    PRINTSTARTFUNC
    uint32_t mask_idx = 0;
    uint32_t num_masks = cu_masks(xcmd);
    for (mask_idx=0; mask_idx<num_masks; ++mask_idx)
    {
      uint32_t cmd_mask = xcmd->packet->data[mask_idx]; /* skip header */
      uint32_t cu_idx = cu_idx_in_mask (f_cu_idx);
      if(cu_mask_idx(f_cu_idx) < mask_idx)
        return false;

      if( isKthBitSet(cmd_mask, cu_idx))
      {
        return true;
      }
    }
    return false;
  }

  void cu_reset(std::shared_ptr<xocl_cu>& xcu, unsigned int idx, uint32_t base, uint32_t addr, uint32_t polladdr)
  {
    PRINTSTARTFUNC
    xcu->idx = idx;
    xcu->base = base;
    xcu->dataflow = (addr & 0xFF) == AP_CTRL_CHAIN;
    xcu->addr = addr & ~(0xFF); // clear encoded handshake
    xcu->polladdr = polladdr;
    xcu->ctrlreg = 0;
    xcu->done_cnt = 0;
    xcu->run_cnt = 0;
  }


  SWScheduler::SWScheduler(std::shared_ptr<CpuemShim> _parent)
  {
    PRINTSTARTFUNC
    mParent = std::weak_ptr{_parent};
    num_pending = 0;
  }

  SWScheduler::~SWScheduler()
  {
    PRINTSTARTFUNC
    mScheduler.reset();
    num_pending = 0;
  }

//KDS FLOW STARTED...
  static uint32_t cu_idx_to_addr(std::shared_ptr<exec_core>& exec,unsigned int cu_idx)
  {
    return exec->cu_addr_map[cu_idx];
  }

  bool SWScheduler::cu_done(std::shared_ptr<exec_core>& exec, unsigned int cu_idx)
  {
    PRINTSTARTFUNC
    uint32_t cu_addr = cu_idx_to_addr(exec,cu_idx);

    uint32_t mask = 0;
    mParent.lock()->xclRead(XCL_ADDR_KERNEL_CTRL, exec->base + cu_addr, (void*)&mask, 4);
    /* done is indicated by AP_DONE(2) alone or by AP_DONE(2) | AP_IDLE(4)
     * but not by AP_IDLE itself.  Since 0x10 | (0x10 | 0x100) = 0x110
     * checking for 0x10 is sufficient. */
    if(mask & 2)
    {
      unsigned int mask_idx = cu_mask_idx(cu_idx);
      unsigned int pos = cu_idx_in_mask(cu_idx);
      exec->cu_status[mask_idx] ^= 1<<pos;
      return true;
    }
    return false;
  }
   int SWScheduler::acquire_slot(std::shared_ptr<xocl_cmd>&  xcmd)
  {
     PRINTSTARTFUNC
    if (type(xcmd)==ERT_CTRL)
      return 0;

    auto exec = xcmd->exec.lock();
    return acquire_slot_idx(exec);
  }

  int SWScheduler::get_free_cu(std::shared_ptr<xocl_cmd>& xcmd)
  {
    PRINTSTARTFUNC
    auto exec = xcmd->exec.lock();
    uint32_t mask_idx=0;
    uint32_t num_masks = cu_masks(xcmd);
    for (mask_idx=0; mask_idx<num_masks; ++mask_idx) {
      uint32_t cmd_mask = xcmd->packet->data[mask_idx]; /* skip header */
      uint32_t busy_mask = exec->cu_status[mask_idx];
      int cu_idx = getFirstSetBitPos((cmd_mask | busy_mask) ^ busy_mask);
      if (cu_idx>=0)
      {
        exec->cu_status[mask_idx] ^= 1<<cu_idx;
        return cu_idx_from_mask(cu_idx,mask_idx);
      }
    }
    return -1;
  }

  uint32_t SWScheduler::cu_masks(std::shared_ptr<xocl_cmd>& xcmd)
  {
    PRINTSTARTFUNC
    struct ert_start_kernel_cmd *sk;
    if (opcode(xcmd)!=ERT_START_KERNEL)
      return 0;
    sk = (struct ert_start_kernel_cmd *)xcmd->packet;
    return 1 + sk->extra_cu_masks;
  }

  uint32_t SWScheduler::regmap_size(std::shared_ptr<xocl_cmd>&  xcmd)
  {
    return payload_size(xcmd) - cu_masks(xcmd);
  }

  void SWScheduler::configure_cu(std::shared_ptr<xocl_cmd>& xcmd, int cu_idx)
  {
    PRINTSTARTFUNC
    auto exec = xcmd->exec.lock();
    uint32_t cu_addr = cu_idx_to_addr(exec,cu_idx);
    uint32_t size = regmap_size(xcmd);
    struct ert_start_kernel_cmd *ecmd = (struct ert_start_kernel_cmd *)xcmd->packet;

    /* write register map, but skip first word (AP_START) */
    /* can't get memcpy_toio to work */
    /* memcpy_toio(user_bar + cu_addr + 4,ecmd->data + ecmd->extra_cu_masks + 1,(size-1)*4); */

    mParent.lock()->xclWrite(XCL_ADDR_KERNEL_CTRL,exec->base + cu_addr + 4 , ecmd->data + ecmd->extra_cu_masks + 1 , size*4);

    /* start CU at base + 0x0 */
    int ap_start = 0x1;
    mParent.lock()->xclWrite(XCL_ADDR_KERNEL_CTRL,exec->base + cu_addr, (void*)&ap_start , 4 );
  }

//  static unsigned int get_cu_idx(struct exec_core *exec, unsigned int cmd_idx)
//  {
//    struct xocl_cmd *xcmd = exec->submitted_cmds[cmd_idx];
//    if(!xcmd)
//      return -1;
//    return xcmd->cu_idx;
//  }

  int SWScheduler::penguin_submit(std::shared_ptr<xocl_cmd>&  xcmd)
  {
    PRINTSTARTFUNC
    /* execution done by submit_cmds, ensure the cmd retired properly */
    if (opcode(xcmd)==ERT_CONFIGURE || type(xcmd)==ERT_KDS_LOCAL || type(xcmd)==ERT_CTRL) {
      xcmd->slot_idx = acquire_slot(xcmd);
      return true;
    }

    if (type(xcmd) != ERT_CU)
      return false;

    // Find a ready CU
    auto exec = xcmd->exec.lock();
    for (unsigned int cuidx = 0; cuidx < exec->num_cus; ++cuidx)
    {
      auto xcu = exec->cus[cuidx];

      if (cmd_has_cu(xcmd, cuidx) && cu_ready(xcu))
      {
        int l_slot_idx =  acquire_slot(xcmd);
        if(l_slot_idx < 0)
          return false;
        if(cu_start(xcu,xcmd))
        {
          xcmd->slot_idx = l_slot_idx;
          exec->submitted_cmds[xcmd->slot_idx] = NULL;
          //exec_release_slot(exec, xcmd);
          xcmd->cu_idx = cuidx;
          ++exec->cu_usage[xcmd->cu_idx];
          (xcu->running_queue).push(xcmd);
          return true;
        }
      }
    }
    return false;
  }

  void SWScheduler::penguin_query(std::shared_ptr<xocl_cmd>&  xcmd)
  {
    PRINTSTARTFUNC
    uint32_t cmd_opcode = opcode(xcmd);
    uint32_t cmd_type = type(xcmd);

    if (cmd_type==ERT_KDS_LOCAL || cmd_type==ERT_CTRL
        ||cmd_opcode==ERT_CONFIGURE)
    {
      mark_cmd_complete(xcmd);
    }
    else if (cmd_type==ERT_CU )
    {
      if(xcmd->cu_idx >= MAX_CUS)
      {
        return;
      }
      auto xcu = xcmd->exec.lock()->cus[xcmd->cu_idx];
      if (xcu && cu_first_done(xcu) == xcmd)
      {
        cu_pop_done(xcu);
        mark_cmd_complete(xcmd);
      }
    }
  }
//KDS FLOW ENDED...

  void SWScheduler::mb_query(std::shared_ptr<xocl_cmd>& xcmd)
  {
    PRINTSTARTFUNC
    if (type(xcmd) == ERT_KDS_LOCAL)
    {
      penguin_query(xcmd);
      return;
    }
    auto exec = xcmd->exec.lock();
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
        mParent.lock()->xclRead(XCL_ADDR_KERNEL_CTRL, exec->base + csr_addr, (void*)&mask, 4);
      }while(waitForResp && !mask);

      if (mask)
      {
#ifdef EM_DEBUG_KDS
        std::cout<<"Mask is non-zero. Mark respective command complete "<< mask << std::endl;
#endif
        mark_mask_complete(exec,mask,cmd_mask_idx);
      }
    }
  }

  int SWScheduler::acquire_slot_idx(std::shared_ptr<exec_core>& exec)
  {
    PRINTSTARTFUNC
    unsigned int mask_idx=0, slot_idx=-1;
    uint32_t mask;
    for (mask_idx=0; mask_idx<exec->num_slot_masks; ++mask_idx)
    {
      mask = exec->slot_status[mask_idx];
      slot_idx = ffz_or_neg_one(mask);
      if (slot_idx_from_mask_idx(slot_idx,mask_idx)>=exec->num_slots)
        continue;
      if(slot_idx > 31) //coverity slot_idx should be <=31
        return -1;
      exec->slot_status[mask_idx] ^= (1<<slot_idx);
      int rSlot = slot_idx_from_mask_idx(slot_idx,mask_idx);
      return rSlot;
    }
    return -1;
  }

  int SWScheduler::mb_submit(std::shared_ptr<xocl_cmd>& xcmd)
  {
    PRINTSTARTFUNC
    if (type(xcmd) == ERT_KDS_LOCAL)
      return penguin_submit(xcmd);

    uint32_t slot_addr;
    auto exec = xcmd->exec.lock();
    xcmd->slot_idx = acquire_slot_idx(exec);
#ifdef EM_DEBUG_KDS
    std::cout<<"Acquring slot index "<<xcmd->slot_idx<<" for CXMD: "<<xcmd<<" PACKET: "<<xcmd->packet<< " BO: "<< xcmd->bo << std::endl;
#endif
    if (xcmd->slot_idx<0) {
      return false;
    }

    slot_addr = ERT_CQ_BASE_ADDR + xcmd->slot_idx*slot_size(exec);

    /* TODO write packet minus header */
    mParent.lock()->xclWrite(XCL_ADDR_KERNEL_CTRL, exec->base + slot_addr + 4, xcmd->packet->data, (packet_size(xcmd)-1)*sizeof(uint32_t));
    //memcpy_toio(xcmd->exec->base + slot_addr + 4,xcmd->packet->data,(packet_size(xcmd)-1)*sizeof(uint32_t));

    /* TODO write header */
    mParent.lock()->xclWrite(XCL_ADDR_KERNEL_CTRL, exec->base + slot_addr, (void*)(&xcmd->packet->header), 4);
    //iowrite32(xcmd->packet->header,xcmd->exec->base + slot_addr);

    /* trigger interrupt to embedded scheduler if feature is enabled */
    if (exec->cq_interrupt) {
      uint32_t cq_int_addr = ERT_CQ_STATUS_REGISTER_ADDR + (slot_mask_idx(xcmd->slot_idx)<<2);
      uint32_t mask = 1<<slot_idx_in_mask(xcmd->slot_idx);
      //TODO
      mParent.lock()->xclWrite(XCL_ADDR_KERNEL_CTRL,exec->base + cq_int_addr, (void*)(&mask), 4);
        //iowrite32(mask,xcmd->exec->base + cq_int_addr);
    }
#ifdef EM_DEBUG_KDS
    std::cout<<"Submitted the command CXMD: "<<xcmd<<" PACKET: "<<xcmd->packet<< " BO: "<< xcmd->bo << std::endl <<std::endl;;
#endif

    return true;
  }

  int SWScheduler::ert_poll_submit_ctrl(std::shared_ptr<xocl_cmd>& xcmd)
  {
    PRINTSTARTFUNC
    if (opcode(xcmd) == ERT_CU_STAT)
      return penguin_submit(xcmd);

    return mb_submit(xcmd);
  }

  void SWScheduler::ert_poll_query_ctrl(std::shared_ptr<xocl_cmd>& xcmd)
  {
    PRINTSTARTFUNC
    if (opcode(xcmd) == ERT_CU_STAT)
      penguin_query( xcmd);
    else
      mb_query(xcmd);
  }


  int SWScheduler::ert_poll_submit(std::shared_ptr<xocl_cmd>& xcmd)
  {
    PRINTSTARTFUNC
    return penguin_submit(xcmd);
  }

  void SWScheduler::ert_poll_query(std::shared_ptr<xocl_cmd>& xcmd)
  {
    PRINTSTARTFUNC
    auto exec = xcmd->exec.lock();
    unsigned int cmd_mask_idx = slot_mask_idx((xcmd->cu_idx+1));

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
        mParent.lock()->xclRead(XCL_ADDR_KERNEL_CTRL, exec->base + csr_addr, (void*)&mask, 4);
      }while(waitForResp && !mask);

      if (mask)
      {
#ifdef EM_DEBUG_KDS
        std::cout<<"Mask is non-zero. Mark respective command complete "<< mask << std::endl;
#endif
        mark_mask_complete(exec,mask,cmd_mask_idx);
      }
    }
  }


  int SWScheduler::configure(std::shared_ptr<xocl_cmd>& xcmd)
  {
    PRINTSTARTFUNC
    auto exec=xcmd->exec.lock();
    struct ert_configure_cmd *cfg;

    cfg = (struct ert_configure_cmd *)(xcmd->packet);

    bool ert = false;//mParent->isMBSchedulerEnabled(); //TODO
    bool ert_poll = (ert && cfg->ert && cfg->dataflow);
    bool ert_full = (ert && cfg->ert && !cfg->dataflow);

    if (exec->configured==0)
    {
      exec->base = 0;
      exec->num_slot_masks = 1;
      exec->num_slots = ERT_CQ_SIZE / cfg->slot_size;
      exec->num_cus = cfg->num_cus;
      exec->cu_shift_offset = cfg->cu_shift;
      exec->cu_base_addr = cfg->cu_base_addr;
      exec->num_cu_masks = ((exec->num_cus-1)>>5) + 1;

      unsigned int cuidx = 0;
      for ( cuidx=0; cuidx<exec->num_cus; cuidx++)
      {
        exec->cu_addr_map[cuidx] = cfg->data[cuidx];
        auto nCu = std::make_shared<xocl_cu>();
        exec->cus[cuidx] =  nCu;
        uint32_t polladdr = (ert_poll) ? ERT_CQ_BASE_ADDR + (cuidx+1) * cfg->slot_size : 0;
        cu_reset(nCu, cuidx, exec->base, cfg->data[cuidx], polladdr);
      }

      bool cdmaEnabled = false;
      if(0) //if (mParent->isCdmaEnabled()) //TODO
      {
        uint32_t addr=0;
        for (unsigned int i = 0 ; i < 4; i++)
        { /* 4 is from xclfeatures.h */
          //addr = mParent->getCdmaBaseAddress(i);//TODO
          if (addr)
          {
            cdmaEnabled = true;
            ++exec->num_cus;
            ++exec->num_cdma;
            ++cfg->num_cus;
            ++cfg->count;
            cfg->data[cuidx] = addr;
            exec->cu_addr_map[cuidx] = cfg->data[cuidx];
            auto nCu = std::make_shared<xocl_cu>();
            exec->cus[cuidx] =  nCu;
            uint32_t polladdr = (ert_poll) ? ERT_CQ_BASE_ADDR + (cuidx+1) * cfg->slot_size : 0;
            cu_reset(nCu, cuidx, exec->base, cfg->data[cuidx], polladdr);
            ++cuidx;
          }
        }
      }

      if (ert_poll)
      {
        cfg->slot_size = ERT_CQ_SIZE / MAX_CUS;
        cfg->cu_isr = 0;
        cfg->cu_dma = 0;
        exec->ertpoll = true;
        exec->ertfull = false;
        exec->polling_mode = 1; //cfg->polling;
        exec->cq_interrupt = cfg->cq_int;
        cfg->cdma = cdmaEnabled ? 1 : 0;
      }
      else if(ert_full)
      {
        exec->ertfull = true;
        exec->ertpoll = false;
        exec->polling_mode = 1; //cfg->polling;
        exec->cq_interrupt = cfg->cq_int;
        cfg->cdma = cdmaEnabled ? 1 : 0;
      }
      else
      {
        exec->ertpoll = false;
        exec->ertfull = false;
        exec->polling_mode = 1; //cfg->polling;
      }
      return 0;
    }

    return 1;
  }

  void SWScheduler::release_slot_idx(std::shared_ptr<exec_core>& exec, unsigned int slot_idx)
  {
    PRINTSTARTFUNC
    unsigned int mask_idx = slot_mask_idx(slot_idx);
    unsigned int pos = slot_idx_in_mask(slot_idx);
    exec->slot_status[mask_idx] ^= (1<<pos);
  }

  void SWScheduler::notify_host(std::shared_ptr<xocl_cmd>& xcmd)
  {
    PRINTSTARTFUNC
    auto exec = xcmd->exec.lock();

    /* now for each client update the trigger counter in the context */
    for(auto& it: exec->ctx_list)
    {
      auto entry = it;
      entry->trigger++;
    }
  }

  void SWScheduler::mark_cmd_complete(std::shared_ptr<xocl_cmd>& xcmd)
  {
    PRINTSTARTFUNC
    auto exec = xcmd->exec.lock();
    exec->submitted_cmds[xcmd->slot_idx] = NULL;
    set_cmd_state(xcmd,ERT_CMD_STATE_COMPLETED);
    if (exec->polling_mode)
      mScheduler->poll--;
    release_slot_idx(exec,xcmd->slot_idx);
#ifdef EM_DEBUG_KDS
    std::cout<<"Marking command Complete XCMD: " <<xcmd<<" PACKET: "<<xcmd->packet<< " BO: "<< xcmd->bo << std::endl;
    std::cout<<"Releasing slot " << xcmd->slot_idx << std::endl<<std::endl;
#endif
    notify_host(xcmd);
  }

  void SWScheduler::mark_mask_complete(std::shared_ptr<exec_core>& exec, uint32_t mask, unsigned int mask_idx)
  {
    PRINTSTARTFUNC
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

  int SWScheduler::queued_to_running(std::shared_ptr<xocl_cmd>& xcmd)
  {
    PRINTSTARTFUNC
    int retval = false;
    bool bConfigure = false;
    if (opcode(xcmd)==ERT_CONFIGURE)
    {
#ifdef EM_DEBUG_KDS
    std::cout<<"Configure command has started. XCMD " <<xcmd<<" PACKET: "<<xcmd->packet<< " BO: "<< xcmd->bo << std::endl;
#endif
      configure(xcmd);
      bConfigure = true;
    }

    auto exec = xcmd->exec.lock();
    bool submitted  = false;
    if(exec->ertfull)
    {
      submitted = mb_submit(xcmd);
    }
    else if ( exec->ertpoll )
    {
      if(bConfigure)
        submitted = ert_poll_submit_ctrl(xcmd);
      else
        submitted = ert_poll_submit(xcmd);
    }
    else
      submitted = penguin_submit(xcmd);

    if ( submitted ) {
      set_cmd_state(xcmd,ERT_CMD_STATE_RUNNING);
      if (exec->polling_mode)
        mScheduler->poll++;
      exec->submitted_cmds[xcmd->slot_idx] = xcmd;
      retval = true;
    }

    return retval;
  }

  void SWScheduler::running_to_complete(std::shared_ptr<xocl_cmd>& xcmd)
  {
    PRINTSTARTFUNC
    auto exec = xcmd->exec.lock();

    bool bConfigure = false;
    if (opcode(xcmd)==ERT_CONFIGURE)
      bConfigure = true;

    if(exec->ertfull)
      mb_query(xcmd);
    else if (exec->ertpoll)
    {
      if(bConfigure)
        ert_poll_query_ctrl(xcmd);
      else
        penguin_query(xcmd);
    }
    else
      penguin_query(xcmd);
  }

  std::shared_ptr<xocl_cmd> SWScheduler::get_free_xocl_cmd(void)
  {
    PRINTSTARTFUNC
    auto cmd = std::make_shared<xocl_cmd>();
    return cmd;
  }

  int SWScheduler::convert_execbuf(std::shared_ptr<exec_core>& exec, std::shared_ptr<xclemulation::drm_xocl_bo>& xobj, std::shared_ptr<xocl_cmd>& xcmd)
  {
    PRINTSTARTFUNC
    size_t src_off;
    size_t dst_off;
    size_t sz;
    uint64_t src_addr = -1; //initializing to avoid ubuntu error
    uint64_t dst_addr = -1; //initializing to avoid ubuntu error
    struct ert_start_copybo_cmd *scmd = (struct ert_start_copybo_cmd *)xobj->buf;

    /* CU style commands must specify CU type */
    if (scmd->opcode == ERT_START_CU || scmd->opcode == ERT_EXEC_WRITE)
      scmd->type = ERT_CU;

    /* Only convert COPYBO cmd for now. */
    if (scmd->opcode != ERT_START_COPYBO)
      return 0;

    sz = ert_copybo_size(scmd);

    src_off = ert_copybo_src_offset(scmd);
    auto sBo = mParent.lock()->xclGetBoByHandle(scmd->src_bo_hdl);

    dst_off = ert_copybo_dst_offset(scmd);
    auto dBo = mParent.lock()->xclGetBoByHandle(scmd->dst_bo_hdl);

    if(!sBo && !dBo)
    {
      return -EINVAL;
    }

    if(sBo)
      src_addr = sBo->base;
    if(dBo)
      dst_addr = dBo->base;

    if (( !sBo || !dBo || mParent.lock()->isImported(scmd->src_bo_hdl) || mParent.lock()->isImported(scmd->dst_bo_hdl)) )
    {
      int ret =  mParent.lock()->xclCopyBO(scmd->dst_bo_hdl, scmd->src_bo_hdl , sz , dst_off, src_off);
      scmd->type = ERT_KDS_LOCAL;
      return ret;
    }

    /* Both BOs are local, copy via KDMA CU */
    if (exec->num_cdma == 0)
      return -EINVAL;

    if ((dst_addr + dst_off) % KDMA_BLOCK_SIZE ||
        (src_addr + src_off) % KDMA_BLOCK_SIZE ||
        sz % KDMA_BLOCK_SIZE)
      return -EINVAL;

    ert_fill_copybo_cmd(scmd, 0, 0, src_addr, dst_addr, sz / KDMA_BLOCK_SIZE);

    for (unsigned int i = exec->num_cus - exec->num_cdma; i < exec->num_cus; i++)
      scmd->cu_mask[i / 32] |= 1 << (i % 32);

    scmd->opcode = ERT_START_CU;
    scmd->type = ERT_CU;

    return 0;

  }

  int SWScheduler::add_cmd(std::shared_ptr<exec_core>& exec, std::shared_ptr<xclemulation::drm_xocl_bo>& bo)
  {
    PRINTSTARTFUNC
    std::lock_guard<std::mutex> lk(pending_cmds_mutex);
    auto xcmd = get_free_xocl_cmd();
    xcmd->packet = (struct ert_packet*)bo->buf;
    xcmd->bo=std::weak_ptr{bo};
    xcmd->exec=std::weak_ptr{exec};
    xcmd->cu_idx=-1;
    xcmd->slot_idx=-1;
    int ret = convert_execbuf(exec, bo , xcmd);
#ifdef EM_DEBUG_KDS
    std::cout<<"adding a command CMD: " <<xcmd<<" PACKET: "<<xcmd->packet<< " BO: "<< xcmd->bo <<" BASE: "<<xcmd->bo->base<< std::endl;
#endif

    set_cmd_state(xcmd,ERT_CMD_STATE_NEW);
    pending_cmds.push_back(xcmd);
    num_pending++;
    scheduler_wait_condition();
    return ret;
  }
  
  int SWScheduler::scheduler_wait_condition()
  {   
    PRINTSTARTFUNC
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
      //pthread_cond_signal(&mScheduler->state_cond);
      mScheduler->state_cond.notify_one();
      return 0;
    }
    return 1;
  }

  void SWScheduler::scheduler_queue_cmds()
  {
    //PRINTSTARTFUNC
    if(pending_cmds.empty())
      return;

#ifdef EM_DEBUG_KDS
    std::cout<<"Iterating on pending commands and adding to Scheduler command_queue  "<< std::endl;
#endif
    for(auto it: pending_cmds)
    {
      auto xcmd = it;

      /* CU style commands must specify CU type */
      if (opcode(xcmd) == ERT_START_CU || opcode(xcmd) == ERT_EXEC_WRITE)
        xcmd->packet->type = ERT_CU;

      mScheduler->command_queue.push_back(xcmd);
      xcmd->state = ERT_CMD_STATE_QUEUED;
#ifdef EM_DEBUG_KDS
    std::cout<<xcmd <<" ADDED to Scheduler command_queue  "<< std::endl;
#endif
      num_pending--;
    }
    pending_cmds.clear();
  }

  void SWScheduler::scheduler_iterate_cmds()
  {
    //PRINTSTARTFUNC
     auto end = mScheduler->command_queue.end();
#ifdef EM_DEBUG_KDS
     //if(mScheduler->command_queue.size() > 0)
     //  std::cout<<" command_queue size is "<<mScheduler->command_queue.size()<< std::endl;
#endif
     for (auto itr=mScheduler->command_queue.begin(); itr!=end; )
     {
       auto xcmd = *itr;
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

  void SWScheduler::scheduler_loop()
  {
    //PRINTSTARTFUNC
    std::lock_guard<std::mutex> lk(this->pending_cmds_mutex);

    if (mScheduler->error) { return; }

    /* queue new pending commands */
    this->scheduler_queue_cmds();

    /* iterate all commands */
    this->scheduler_iterate_cmds();
  }

  void SWScheduler::scheduler()
  {
    PRINTSTARTFUNC
    while (!mScheduler->stop && !mScheduler->error)
    {
      scheduler_loop();
      usleep(10);
    }
    
  }

  int SWScheduler::init_scheduler_thread(void)
  {
    mScheduler = std::make_shared<xocl_sched>(shared_from_this());
    PRINTSTARTFUNC
    if (mScheduler->bThreadCreated)
      return 0;

#ifdef EM_DEBUG_KDS
    std::cout<<"SWScheduler Thread started "<< std::endl;
#endif

    mScheduler->scheduler_thread = std::thread([&]
                                               { this->scheduler(); });
    mScheduler->bThreadCreated = true;

    return 0;
  }

  int SWScheduler::fini_scheduler_thread(void)
  {
    PRINTSTARTFUNC
    if (!mScheduler->bThreadCreated)
      return 0;

#ifdef EM_DEBUG_KDS
    std::cout<<"SWScheduler Thread ended "<< std::endl;
#endif

    mScheduler->stop= true;
    scheduler_wait_condition();
    mScheduler->bThreadCreated = false;
    
    //int retval = pthread_join(mScheduler->scheduler_thread,NULL);
    int retval = 0;
    
    if (mScheduler->scheduler_thread.joinable())
      mScheduler->scheduler_thread.join();
   
    pending_cmds.clear();
    mScheduler->command_queue.clear();
    free_cmds.clear();

    return retval;
  }

  int SWScheduler::add_exec_buffer(std::shared_ptr<exec_core> &exec, std::shared_ptr<xclemulation::drm_xocl_bo> &buf)
  {
    PRINTSTARTFUNC
    return add_cmd(exec, buf);
  }
}
