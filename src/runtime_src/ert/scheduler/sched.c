/**
 * Copyright (C) 2022 Xilinx, Inc
 * Copyright (C) 2022 Advanced Micro Devices, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

/**
 * Embedded runtime scheduler
 */
#include "core/include/xrt/detail/ert.h"

#include <stdint.h>
// includes from bsp
#ifndef ERT_HW_EMU
#include <mb_interface.h>
#include <xparameters.h>
#endif

#include "sched_print.h"
#include "xgq_mb_plat.h"
#include "core/include/xgq_impl.h"
#include "sched_cmd.h"
#include "sched_cu.h"
#include "xgq_ctrl.h"
#include "xgq_cu.h"

#define ERT_UNUSED __attribute__((unused))

//#define DEBUG_SLOT_STATE
#define CU_STATUS_MASK_NUM          4

#define REG32_MASK_SHIFT            5
// Assert macro implementation

#define CTRL_XGQ_SLOT_SIZE          512

#define ENABLE_I2H                  (1<<13)
#define ENABLE_I2E                  (1<<14)
#define ENABLE_CUI                  (1<<15)
#define CMD_QUEUE_MODE              (1<<16)
#define SCRATCH_MODE                (1<<17)
#define ECHO_MODE                   (1<<18)
#define DMSG_ENABLE                 (1<<19)

inline void
exit(int32_t val)
{
  CTRL_DEBUGF("%s\r\n",__func__);
  for(;;) {
  }
}

ERT_UNUSED
static void
ert_assert(const char* file, long line, const char* function, const char* expr, const char* msg)
{
  CTRL_DEBUGF("Assert failed: %s:%ld:%s:%s %s\r\n",file,line,function,expr,msg);
  exit(1);
}
////////////////////////////////////////////////////////////////
// Convenience types for clarity
////////////////////////////////////////////////////////////////
typedef uint32_t size_type;
typedef uint32_t addr_type;
typedef uint32_t value_type;
typedef uint32_t bitmask_type;

#define MASK_BIT_32(n)                 (((n)==32) ? ((bitmask_type)(~0)) : ((((bitmask_type)(1))<<(n))-1))

#ifdef ERT_HW_EMU
extern value_type read_reg(addr_type addr);
extern void write_reg(addr_type addr, value_type val);
extern void microblaze_enable_interrupts();
extern void microblaze_disable_interrupts();
extern void reg_access_wait();
#endif

////////////////////////////////////////////////////////////////
// HLS AXI protocol (from xclbin.h)
////////////////////////////////////////////////////////////////
#define AP_CTRL_HS    0
#define AP_CTRL_CHAIN 1
#define AP_CTRL_NONE  2
#define AP_CTRL_ME    3
#define ACCEL_ADATER  4
#define FAST_ADATER   5

////////////////////////////////////////////////////////////////
// Extensions to core/include/ert.h
////////////////////////////////////////////////////////////////
addr_type STATUS_REGISTER_ADDR[4] = {0, 0, 0, 0};

// If this assert fails, then ert_parameters is out of sync with
// the board support package header files.
#ifndef ERT_HW_EMU
_Static_assert(ERT_INTC_ADDR==XPAR_INTC_SINGLE_BASEADDR,"update core/include/ert.h");
#endif

////////////////////////////////////////////////////////////////
// Configuarable constants
// Statically allcoated array size is reduced in debug otherwise
// there is not enough space for compiled firmware
////////////////////////////////////////////////////////////////
#ifdef ERT_VERBOSE
#define MAX_SLOTS                  32   // size of statically allocated array
#else
#define MAX_SLOTS                  128  // size of statically allocated array
#endif

// Max number of compute units
#ifdef ERT_VERBOSE
#define MAX_CUS                    32   // size of statically allocated array
#else
#define MAX_CUS                    128  // size of statically allocated array
#endif

static size_type num_cus         = 0;    // actual number of cus



#define MAX_XGQ_CU    32

#define MAJOR         1U
#define MINOR         0U
#define ERT_VER       ((MAJOR<<16) + MINOR)

#define CTRL_XGQ_SPACE  (0x800)

#define CU_ARG_OFFSET (0x10)

#define XGQ_CMD_FEATURE_OFFSET     0x8
#define XGQ_CU_CMD_LOW_ADDR        0x10
#define XGQ_CU_CMD_HIGH_ADDR       0x14
#define XGQ_CU_CMD_SLOT_SZ_OFFSET  0x18


#define XGQ_CU_IDX(features)    (features & MASK_BIT_32(12))
#define XGQ_IP_CTRL(features)   ((features & MASK_BIT_32(8)) >> 16)
#define XGQ_NUM_CUS(features)   ((features & MASK_BIT_32(13)))
#define XGQ_OFFSET(xgq)         (xgq->xq_header_addr-ERT_CQ_BASE_ADDR)

size_type ctrl_queue_size = CTRL_XGQ_SPACE;

// First four bytes is VERSION, ctrl xgq starts from 0x4
addr_type ctrl_queue_offset = 0x4;


addr_type ctrl_scratch_offset = 0x0;

addr_type user_queue_offset = CTRL_XGQ_SPACE;

struct xgq xgq_admin;

struct xgq_ctrl ctrl_xgq = {0};

// mode 1
struct xgq xgqs[MAX_XGQ_CU] = {0};

struct xgq_cu cu_xgqs[MAX_XGQ_CU] = {0};

size_type cu_slot_sizes[MAX_XGQ_CU] = {0};

struct sched_cu sched_cus[MAX_XGQ_CU] = {0};


static value_type i2h                    = 0;
static value_type i2e                    = 0;
static value_type cui                    = 0;

static value_type dmsg                   = 0;

/* Performance breakdown: if echo flag is set,
 * MB notifies host right away and does not touch
 * hardware(config CU)
 */
value_type echo                          = 0;

static value_type cmd_queue_mode         = 0;

static value_type scratch_mode           = 0;

static value_type flatten_queue          = 0;

static value_type cfg_complete           = 0;

static size_type cmd_queue_slot_size     = 0;

struct cu_info
{
  // Index of CU that is assigned to this command
  size_type cu_idx;

  value_type ip_ctrl;

  // Size of user slot (in 32 bit words)
  size_type slot_size;

};

static struct cu_info cu_cfg[MAX_CUS];

#ifndef ERT_HW_EMU
/**
 * Utility to read a 32 bit value from any axi-lite peripheral
 */
static inline value_type
read_reg(addr_type addr)
{
  volatile value_type *ptr = (addr_type *)(addr);
  return *ptr;
}

/**
 * Utility to write a 32 bit value from any axi-lite peripheral
 */
static inline void
write_reg(addr_type addr, value_type val)
{
  ERT_DEBUGF("write_reg addr(0x%x) val(0x%x)\r\n", addr, val);
  volatile value_type *ptr = (addr_type *)(addr);
  *ptr = val;
}
#endif

inline static value_type
read_clk_counter(void)
{
  return read_reg(ERT_CLK_COUNTER_ADDR);
}

static inline value_type
rw_count_addr(addr_type slot_addr)
{
  return slot_addr + 0x8;
}

static inline value_type
draft_addr(addr_type slot_addr)
{
  return slot_addr + 0xC;
}

inline static void
setup_ert_base_addr()
{
  //std::fill(cu_addr_map, cu_addr_map + max_cus, AP_CTRL_NONE);
  
  // In Subsytem 2.0 and 3.0, ERT MB now has to go around to access 3 peripherals
  // internal to ERT Subsystem i.e CQRAM Controller, Embedded scheduler HW
  // and KDMA. ERT MB needs to read the value in ERT_BASE_ADDR, add that
  // value to ERT Subsystem Base Address and Peripheral Address, new value
  // would be used by the ERT MB to access CQ and CSR.
  #if defined(ERT_BUILD_V30) || defined(ERT_BUILD_V20)
  ert_base_addr = read_reg(ERT_BASE_ADDR);
  #endif

  STATUS_REGISTER_ADDR[0] = ERT_STATUS_REGISTER_ADDR0;
  STATUS_REGISTER_ADDR[1] = ERT_STATUS_REGISTER_ADDR1;
  STATUS_REGISTER_ADDR[2] = ERT_STATUS_REGISTER_ADDR2;
  STATUS_REGISTER_ADDR[3] = ERT_STATUS_REGISTER_ADDR3;  
}

static int
setup_cu_queue()
{
  value_type cu_xgq_offset = user_queue_offset;
  value_type cu_xgq_range = ERT_CQ_SIZE-cu_xgq_offset;
  int ret = 0;
  size_type cu_idx;


  CTRL_DEBUGF(" cmd_queue_mode %d\r\n", cmd_queue_mode);
  CTRL_DEBUGF(" scratch_mode   %d\r\n", scratch_mode);
  CTRL_DEBUGF(" cu_xgq_offset  %x\r\n", cu_xgq_offset);
  CTRL_DEBUGF(" cu_xgq_range   %x\r\n", cu_xgq_range);
  CTRL_DEBUGF(" num_cus        %d\r\n", num_cus);
  CTRL_DEBUGF(" echo           %d\r\n", echo);

  if (!num_cus)
    return ret;

  if (!flatten_queue) {
    ret = xgq_group_alloc(xgqs, num_cus, XGQ_IN_MEM_PROD, 0,
                          ERT_CQ_BASE_ADDR+cu_xgq_offset, (size_t *)&cu_xgq_range,
                          cu_slot_sizes, MAX_SLOTS);

    CTRL_DEBUGF(" XGQ MODE! cu_xgq_range %x ret %d\r\n", cu_xgq_range, ret);
    if (!ret) {
      for (cu_idx=0; cu_idx<num_cus; ++cu_idx) {
         struct xgq_cu *cu_xgq = &cu_xgqs[cu_idx];
         struct xgq *xgq = &xgqs[cu_idx];
         struct sched_cu *cu = &sched_cus[cu_idx];

         cu_xgq->offset = XGQ_OFFSET(xgq);
         cu_xgq->xgq_id = cu_idx;
         cu_xgq->csr_reg = STATUS_REGISTER_ADDR[cu_idx>>5];

         xgq_cu_init(cu_xgq, xgq, cu);
      }
    }
  }

  return ret;
}
/**
 * Configure MB and peripherals
 *
 * Wait for XGQ_CMD_OP_CFG_START in ctrl xgq, then configure as
 * requested.
 *
 * This function is used in two different scenarios:
 *  1. MB reset/startup, in which case the XGQ_CMD_OP_CFG_START is guaranteed
 *     to be in a slot at default slot offset (4K), most likely slot 0.
 *  2. During regular scheduler loop, in which case the XGQ_CMD_OP_CFG_START
 *     packet is at an arbitrary slot location.   In this scenario, the
 *     function may return (false) without processing the command if
 *     other commands are currently executing; this is to avoid hardware
 *     lockup.
 *
 * @param cmd
 *   Cmd to be processed
 * @return
 *   0 if XGQ_CMD_OP_CFG_START packet was processed, < 0 otherwise
 */
static int32_t
configure_mb(struct sched_cmd *cmd)
{
  addr_type queue_addr = cmd->cc_addr;
  struct xgq_cmd_resp_config_start resp_cmd = {0};
  value_type features = read_reg(queue_addr+XGQ_CMD_FEATURE_OFFSET);
  num_cus = XGQ_NUM_CUS(features);
  int ret = 0;

  cfg_complete = 0;

  if (num_cus > MAX_XGQ_CU)
    flatten_queue = 0x1;
  else
    flatten_queue = 0x0;

  CTRL_DEBUGF(" features 0x%x\r\n",features);

  i2h = (features & ENABLE_I2H) != 0;

  i2e = (features & ENABLE_I2E) != 0;

  cui = (features & ENABLE_CUI) != 0;

  dmsg = (features & DMSG_ENABLE) != 0;

  echo = (features & ECHO_MODE) != 0;

  cmd_queue_mode = (features & CMD_QUEUE_MODE) != 0;

  scratch_mode = (features & SCRATCH_MODE) != 0;

  flatten_queue |= ((scratch_mode << 1) || cmd_queue_mode);

  if (flatten_queue)
    cmd_queue_slot_size = 0;

#ifdef XGQ_CMD_DEBUG
  resp_cmd.hdr.cid = cmd->cc_header.hdr.cid;
#endif
  resp_cmd.i2h = 1;
  resp_cmd.i2e = 0;
  resp_cmd.cui = 0;
  resp_cmd.ob = 0;

  resp_cmd.rcode = ret;
   
  xgq_ctrl_response(&ctrl_xgq, &resp_cmd, sizeof(struct xgq_cmd_resp_config_start));

  CTRL_DEBUGF("<------- configure_mb\r\n");
  return ret;
}

static int32_t
configure_mb_end(struct sched_cmd *cmd)
{
  struct xgq_com_queue_entry resp_cmd = {0};
  int ret = setup_cu_queue();

  CTRL_DEBUGF(" interrupt to host config as %d \r\n", i2h);
  write_reg(ERT_HOST_INTERRUPT_ENABLE_ADDR, i2h);
#ifdef XGQ_CMD_DEBUG
  resp_cmd.hdr.cid = cmd->cc_header.hdr.cid;
#endif
  resp_cmd.rcode = ret;

  if (!ret)
  	cfg_complete = 1;

  xgq_ctrl_response(&ctrl_xgq, &resp_cmd, sizeof(struct xgq_com_queue_entry));

  CTRL_DEBUGF("<------- configure_mb_end ret %d\r\n", ret);
  return ret;
}

static int32_t
save_cfg_cu(struct sched_cmd *cmd)
{
  int ret = 0;
  addr_type queue_addr = cmd->cc_addr;
  value_type features = read_reg(queue_addr+XGQ_CMD_FEATURE_OFFSET), cu_idx = XGQ_CU_IDX(features);
  value_type addr_hi, addr_lo;
  struct cu_info *cu = NULL;
  struct sched_cu *sched_cu = NULL;
  struct xgq_com_queue_entry resp_cmd = {0};

  // check every cu is configured sequentially
  if (cu_idx > MAX_CUS || cu_idx >= num_cus) {
    ret = -EINVAL;
    resp_cmd.rcode = ret;
    xgq_ctrl_response(&ctrl_xgq, &resp_cmd, sizeof(struct xgq_com_queue_entry));
    return ret;
  }

  cu = &cu_cfg[cu_idx];
  sched_cu = &sched_cus[cu_idx];

  cu->ip_ctrl = XGQ_IP_CTRL(features);
  addr_lo = read_reg(queue_addr+XGQ_CU_CMD_LOW_ADDR); // config_cu_cmd->laddr
  addr_hi = read_reg(queue_addr+XGQ_CU_CMD_HIGH_ADDR); // config_cu_cmd->haddr
  cu->slot_size = read_reg(queue_addr+XGQ_CU_CMD_SLOT_SZ_OFFSET); //config_cu_cmd->slot_size


  cu_set_addr(sched_cu, (((uint64_t)addr_hi) << 32) + addr_lo);

  CTRL_DEBUGF(" cu->ip_ctrl %d \r\n", cu->ip_ctrl);
  CTRL_DEBUGF(" cu->slot_size %d \r\n", cu->slot_size);
  CTRL_DEBUGF(" cu_addr 0x%x%x \r\n", addr_hi, addr_lo);
  cu_slot_sizes[cu_idx] = cu->slot_size;

  if (flatten_queue) {
    cmd_queue_slot_size = (cmd_queue_slot_size > cu->slot_size) ?
                                cmd_queue_slot_size : cu->slot_size;
  }

#ifdef XGQ_CMD_DEBUG
  resp_cmd.hdr.cid = cmd->cc_header.hdr.cid;
#endif
  resp_cmd.rcode = ret;

  xgq_ctrl_response(&ctrl_xgq, &resp_cmd, sizeof(struct xgq_com_queue_entry));

  CTRL_DEBUGF("<------- save_cfg_cu \r\n");
  return ret;
}

static int32_t
query_cu(struct sched_cmd *cmd)
{
  int ret = 0;
  addr_type queue_addr = cmd->cc_addr;
  value_type features = read_reg(queue_addr+XGQ_CMD_FEATURE_OFFSET), cu_idx = XGQ_CU_IDX(features);
  struct xgq_cmd_resp_query_cu resp_cmd = {0};

  if (cu_idx > MAX_CUS)
    ret = -EINVAL;

  if (!flatten_queue)
    resp_cmd.offset = cu_xgqs[cu_idx].offset;
  else
    resp_cmd.offset = user_queue_offset;

  resp_cmd.xgq_id = cu_idx;
  resp_cmd.type = flatten_queue;

#ifdef XGQ_CMD_DEBUG
  resp_cmd.hdr.cid = cmd->cc_header.hdr.cid;
#endif
  resp_cmd.rcode = ret;

  CTRL_DEBUGF("  cu_idx          %x\r\n", cu_idx);
  CTRL_DEBUGF("  xgq_id          %x\r\n", resp_cmd.xgq_id);
  CTRL_DEBUGF("  xgq_type        %x\r\n", resp_cmd.type);
  CTRL_DEBUGF("  resp_cmd.offset %x\r\n", resp_cmd.offset);
  CTRL_DEBUGF("  resp_ret        %x\r\n", resp_cmd.rcode);

  xgq_ctrl_response(&ctrl_xgq, &resp_cmd, sizeof(struct xgq_cmd_resp_query_cu));
  CTRL_DEBUGF("<------- query_cu \r\n");
  return ret;
}

static inline int32_t
get_clk_counter(struct sched_cmd *cmd)
{
  int ret = 0;
  struct xgq_cmd_resp_clock_calib resp_cmd = {0};

  resp_cmd.timestamp = read_clk_counter();
  
#ifdef XGQ_CMD_DEBUG
  resp_cmd.hdr.cid = cmd->cc_header.hdr.cid;
#endif
  resp_cmd.rcode = ret;

  xgq_ctrl_response(&ctrl_xgq, &resp_cmd, sizeof(struct xgq_cmd_resp_clock_calib));
  return ret;
}

static inline void 
repetition_write(addr_type addr, value_type loop_cnt)
{
  while (loop_cnt--)
    write_reg(addr, 0x0);
}

static inline void 
repetition_read(addr_type addr, value_type loop_cnt)
{
  while (loop_cnt--)
    read_reg(addr);
}

static int32_t
validate_mb(struct sched_cmd *cmd)
{
  int ret = 0;
  value_type start_t, end_t, cnt = 1024;
  struct xgq_cmd_resp_access_valid resp_cmd = {0};

  start_t = read_clk_counter();
  repetition_read(cmd->cc_addr, cnt);
  end_t = read_clk_counter();
  resp_cmd.cq_read_single = (end_t-start_t)/cnt;
   
  start_t = read_clk_counter();
  repetition_write(cmd->cc_addr, cnt);
  end_t = read_clk_counter();
  resp_cmd.cq_write_single = (end_t-start_t)/cnt;

  start_t = read_clk_counter();
  repetition_read(sched_cus[0].cu_addr, cnt);
  end_t = read_clk_counter();
  resp_cmd.cu_read_single = (end_t-start_t)/cnt;

  start_t = read_clk_counter();
  repetition_write(sched_cus[0].cu_addr, cnt);
  end_t = read_clk_counter();
  resp_cmd.cu_write_single = (end_t-start_t)/cnt; 
  
  CTRL_DEBUGF("resp_cmd.cq_read_single %d\r\n",resp_cmd.cq_read_single);
  CTRL_DEBUGF("resp_cmd.cq_write_single %d\r\n",resp_cmd.cq_write_single);
  CTRL_DEBUGF("resp_cmd.cu_read_single %d\r\n",resp_cmd.cu_read_single);
  CTRL_DEBUGF("resp_cmd.cu_write_single %d\r\n",resp_cmd.cu_write_single);    
#ifdef XGQ_CMD_DEBUG
  resp_cmd.hdr.cid = cmd->cc_header.hdr.cid;
#endif
  resp_cmd.rcode = ret;

  xgq_ctrl_response(&ctrl_xgq, &resp_cmd, sizeof(struct xgq_cmd_resp_access_valid));
  return ret;
}


static int32_t
data_integrity(struct sched_cmd *cmd)
{
  int ret = 0;
  addr_type queue_addr = cmd->cc_addr;
  struct xgq_cmd_resp_data_integrity resp_cmd = {0};
  value_type cnt = 0;


  // Read Write stress test
  resp_cmd.data_integrity = 1;
  while ((cnt = read_reg(rw_count_addr(queue_addr)))) {
    value_type pattern = read_reg(draft_addr(queue_addr));
    if (pattern != 0x0 && pattern != 0xFFFFFFFF) {
      CTRL_DEBUGF("read undefined value = 0x%x\r\n",pattern);
      resp_cmd.data_integrity = 0;
    }
  }

  resp_cmd.h2d_access = 1;
  resp_cmd.d2d_access = 1;
  for (size_type offset=sizeof(struct xgq_cmd_data_integrity); offset<CTRL_XGQ_SLOT_SIZE; offset+=4) {
    volatile value_type pattern = read_reg(queue_addr+offset);
    if (pattern != HOST_RW_PATTERN) {
      resp_cmd.h2d_access = 0;
      CTRL_DEBUGF("h2d_access failed, pattern = 0x%x slot.slot_addr 0x%x\r\n",pattern,queue_addr+offset);
      break;
    }

    write_reg(queue_addr+offset, DEVICE_RW_PATTERN);
    pattern = read_reg(queue_addr+offset);

    if (pattern != DEVICE_RW_PATTERN) {
      resp_cmd.d2d_access = 0;
      CTRL_DEBUGF("d2d_access failed, pattern = 0x%x slot.slot_addr 0x%x\r\n",pattern,queue_addr+offset);
      break;
    }
  }

  resp_cmd.d2cu_access = 1;
  for (size_type i=0; i<num_cus; ++i) {
    addr_type addr = sched_cus[i].cu_addr;
    value_type val = read_reg(addr);
    if (val != 0x4) {
      resp_cmd.d2cu_access = 0;
    }
  }
#ifdef XGQ_CMD_DEBUG
  resp_cmd.hdr.cid = cmd->cc_header.hdr.cid;
#endif
  resp_cmd.rcode = ret;

  xgq_ctrl_response(&ctrl_xgq, &resp_cmd, sizeof(struct xgq_cmd_resp_data_integrity));
  return ret;
}


static void
exit_mb(struct sched_cmd *cmd)
{
  struct xgq_com_queue_entry resp_cmd = {0};

#ifdef XGQ_CMD_DEBUG
  resp_cmd.hdr.cid = cmd->cc_header.hdr.cid;
#endif
  resp_cmd.rcode = 0;

  xgq_ctrl_response(&ctrl_xgq, &resp_cmd, sizeof(struct xgq_com_queue_entry));
  CTRL_DEBUGF("mb_sleep\r\n");
#ifndef ERT_HW_EMU
  mb_sleep();
#endif
  CTRL_DEBUGF("mb wakeup\r\n");
}

static int32_t
identify_xgq(struct sched_cmd *cmd)
{
  struct xgq_cmd_resp_identify resp_cmd = {0};

  resp_cmd.minor = MINOR;
  resp_cmd.major = MAJOR;

  resp_cmd.rcode = 0;

#ifdef XGQ_CMD_DEBUG
  resp_cmd.hdr.cid = cmd->cc_header.hdr.cid;
#endif
#ifdef ERT_BUILD_V30
  resp_cmd.resvd = read_clk_counter();
#endif
  xgq_ctrl_response(&ctrl_xgq, &resp_cmd, sizeof(struct xgq_cmd_resp_identify));

  return 0;
}

/**
 * Process special command.
 *
 * Special commands are not performace critical
 *
 * @return true
 *   If command was processed, false otherwise
 */
static inline int
process_ctrl_command()
{
  struct sched_cmd *cmd = xgq_ctrl_get_cmd(&ctrl_xgq);
  int ret = 0;
  value_type opcode = 0;

  if (!cmd)
    return -ENOENT;

#ifdef XGQ_CMD_DEBUG
  {
      /* CQ offset 0x610 is unused yet. */
      struct xgq_cmd_sq_hdr *log = (void *)(ERT_CQ_BASE_ADDR + 0x610);
      int i = 0;

      for (i = 0; i < 3; i++) {
          write_reg((uint32_t)(&log[i].header[0]), read_reg((uint32_t)(&log[i+1].header[0])));
          write_reg((uint32_t)(&log[i].header[1]), read_reg((uint32_t)(&log[i+1].header[1])));
      }
      write_reg((uint32_t)(&log[i].header[0]), cmd->cc_header.hdr.header[0]);
      write_reg((uint32_t)(&log[i].header[1]), cmd->cc_header.hdr.header[1]);
  }
#endif

  opcode = cmd_op_code(cmd);

  switch (opcode) {
  case XGQ_CMD_OP_CFG_START:
    ret = configure_mb(cmd);
    break;
  case XGQ_CMD_OP_CFG_END:
    ret = configure_mb_end(cmd);
    break;
  case XGQ_CMD_OP_CFG_CU:
    ret = save_cfg_cu(cmd);
    break;
  case XGQ_CMD_OP_QUERY_CU:
    ret = query_cu(cmd);
    break;
  case XGQ_CMD_OP_CLOCK_CALIB:
    ret = get_clk_counter(cmd);
    break;
  case XGQ_CMD_OP_ACCESS_VALID:
    ret = validate_mb(cmd);
    break;
  case XGQ_CMD_OP_DATA_INTEGRITY:
    ret = data_integrity(cmd);
    break;
  case XGQ_CMD_OP_EXIT:
    exit_mb(cmd);
    break;
  case XGQ_CMD_OP_IDENTIFY:
    ret = identify_xgq(cmd);
    break;
  default:
    ret = -ENOTTY;
    break;
  }

  return ret;
}

static void
setup_ctrl_queue()
{
  int ret = 0;
  uint32_t flag = XGQ_IN_MEM_PROD;

  // reset the ctrl_queue_size
  ctrl_queue_size = CTRL_XGQ_SPACE;

  ret = xgq_alloc(&xgq_admin, flag, 0, ERT_CQ_BASE_ADDR+ctrl_queue_offset,
                  (size_t *)&ctrl_queue_size, CTRL_XGQ_SLOT_SIZE, 0, 0);
  if (ret) {
    CTRL_DEBUGF("Failed to alloc XGQ, ret  %d\r\n", ret);
    CTRL_DEBUGF("Flag                    0x%x\r\n", flag);
    CTRL_DEBUGF("ctrl_queue_offset       0x%x\r\n", ERT_CQ_BASE_ADDR+ctrl_queue_offset);
    CTRL_DEBUGF("ctrl_queue_size         0x%x\r\n", ctrl_queue_size);
    CTRL_DEBUGF("CTRL XGQ SIZE           0x%x\r\n", CTRL_XGQ_SLOT_SIZE);
  }

  ctrl_scratch_offset = ctrl_queue_size;

  xgq_ctrl_init(&ctrl_xgq, &xgq_admin);
}
/**
 * Main routine executed by embedded scheduler loop
 *
 * _scheduler_loop() will keep polling each slot 
 * to see if there is any new command coming
 */
static void
_scheduler_loop()
{
  ERT_DEBUGF("ERT XGQ scheduler\r\n");
  size_type slot_idx = 0;

  // Set up ERT base address, this should only call once
  setup_ert_base_addr();

  write_reg(ERT_CQ_BASE_ADDR, ERT_VER);

  // Basic setup will be changed by configure_mb, but is necessary
  // for even configure_mb() to work.
  setup_ctrl_queue();

  setup_cu_queue();

  while (1) {
#ifdef ERT_HW_EMU
    reg_access_wait();
#endif
    while(!process_ctrl_command());
    if (cfg_complete) {
      for (slot_idx=0; slot_idx<num_cus; ++slot_idx) {
        while(!xgq_cu_process(&cu_xgqs[slot_idx])) {
          continue;
        }
      }
    }
  } // while
}

/**
 * CU interrupt service routine
 */
#ifndef ERT_HW_EMU
void _cu_interrupt_handler() __attribute__((interrupt_handler));
#endif
void
_cu_interrupt_handler()
{
  DMSGF("interrupt_handler\r\n");
  bitmask_type intc_mask = read_reg(ERT_INTC_IPR_ADDR);

  // Acknowledge interrupts
  write_reg(ERT_INTC_IAR_ADDR,intc_mask);
}

#ifdef ERT_HW_EMU
#ifdef __cplusplus
extern "C" {
#endif

#if defined(ERT_BUILD_V30)
void scheduler_v30_loop() {
    _scheduler_loop();
}
void cu_interrupt_handler_v30() {
    _cu_interrupt_handler();
}
#else
void scheduler_loop() {
    _scheduler_loop();
}
void cu_interrupt_handler() {
    _cu_interrupt_handler();
}
#endif //ERT_BUILD_V30

#ifdef __cplusplus
}
#endif

#else
int main()
{
  _scheduler_loop();
  return 0;
}
#endif
