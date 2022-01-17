/**
 * Copyright (C) 2016-2021 Xilinx, Inc
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

#include "core/include/ert.h"
#include <stdint.h>
// includes from bsp
#ifndef ERT_HW_EMU
#include <xil_printf.h>
#include <mb_interface.h>
#include <xparameters.h>
#else
#include <stdio.h>
#define xil_printf printf
#endif

// version is a git hash passed in from build script
// default for builds that bypass build script
#ifndef ERT_VERSION
# define ERT_VERSION 0
#endif
#ifndef ERT_SVERSION
# define ERT_SVERSION "0xdeadbeef"
#endif

// set local string that can be extracted
// from binary using 'strings sched.bin'
const char* ert_version = ERT_SVERSION;

#define ERT_UNUSED __attribute__((unused))

//#define ERT_VERBOSE
#define CTRL_VERBOSE
//#define DEBUG_SLOT_STATE
#define CU_STATUS_MASK_NUM          4

#define REG32_MASK_SHIFT            5
// Assert macro implementation


#define ERT_ENABLE                  (1<<0)
#define MB_HOST_INTR_ENABLE         (1<<1)
#define CDMA_ENABLE                 (1<<5)
#define DATAFLOW_ENABLE             (1<<6)
#define KDS_NEW                     (1<<8)
#define DMSG_ENABLE                 (1<<9)
#define ECHO_MODE                   (1<<10)

inline void
exit(int32_t val)
{
  xil_printf("%s\r\n",__func__);
  for(;;) {
  }
}


ERT_UNUSED
static void
ert_assert(const char* file, long line, const char* function, const char* expr, const char* msg)
{
  xil_printf("Assert failed: %s:%ld:%s:%s %s\r\n",file,line,function,expr,msg);
  exit(1);
}

#ifdef ERT_VERBOSE
# define ERT_PRINTF(format,...) xil_printf(format, ##__VA_ARGS__)
# define ERT_DEBUGF(format,...) xil_printf(format, ##__VA_ARGS__)
# define ERT_ASSERT(expr,msg) ((expr) ? ((void)0) : ert_assert(__FILE__,__LINE__,__FUNCTION__,#expr,msg))
#else
# define ERT_PRINTF(format,...) xil_printf(format, ##__VA_ARGS__)
# define ERT_DEBUGF(format,...)
# define ERT_ASSERT(expr,msg)
#endif

#ifdef CTRL_VERBOSE 
#if defined(ERT_BUILD_V30)
# define CTRL_DEBUG(msg) xil_printf(msg)
# define CTRL_DEBUGF(format,...) xil_printf(format, ##__VA_ARGS__)
# define DMSGF(format,...) if (dmsg) xil_printf(format, ##__VA_ARGS__)
#else
# define CTRL_DEBUG(msg)
# define CTRL_DEBUGF(format,...)
# define DMSGF(format,...)
#endif
#else
# define CTRL_DEBUG(msg)
# define CTRL_DEBUGF(format,...)
# define DMSGF(format,...)
#endif


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
// HLS AXI-lite
////////////////////////////////////////////////////////////////
#define AP_START    0x1
#define AP_DONE     0x2
#define AP_IDLE     0x4
#define AP_READY    0x8
#define AP_CONTINUE 0x10

////////////////////////////////////////////////////////////////
// HLS AXI protocol (from xclbin.h)
////////////////////////////////////////////////////////////////
#define	CU_ADDR_HANDSHAKE_MASK	(0xff)
#define	CU_HANDSHAKE(addr) ((addr) & CU_ADDR_HANDSHAKE_MASK)
#define	CU_ADDR(addr) ((addr) & ~CU_ADDR_HANDSHAKE_MASK)
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

// Marker for invalid index
const size_type no_index = (size_type)(-1);

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
static size_type num_slots                  = 16;   // actual number of slots

// Max number of compute units
#ifdef ERT_VERBOSE
#define MAX_CUS                    32   // size of statically allocated array
#else
#define MAX_CUS                    128  // size of statically allocated array
#endif
static size_type num_cus                    = 3;    // actual number of cus

// CU base address
static addr_type cu_base_address            = 0x0;

// CU address map
static addr_type cu_addr_map[MAX_CUS];

// CU offset (addone is 64k (1<<16), OCL is 4k (1<<12))
static size_type cu_offset                  = 16;

// Slot size
static size_type slot_size                  = 0x1000;


static value_type mb_host_interrupt_enabled = 0;

static value_type cdma_enabled              = 0;
static value_type dataflow_enabled          = 0;
static value_type kds_new                    = 0;
static value_type dmsg                      = 0;
/* Performance breakdown: if echo flag is set,
 * MB notifies host right away and does not touch
 * hardware(config CU)
 */
static value_type echo                      = 0;

//static size_type cu_status_mask_nums       = 4;
// Struct slot_info is per command slot in command queue
struct slot_info
{
  // Address of slot in command queue
  addr_type slot_addr;

  // Last command header read from slot in command queue
  // Last 4 bits of header are used for slot status per mb state
  // new     [0x1]: the command is in new state per host
  value_type header_value;

  // Cache opcode
  value_type opcode;

  // Index of CU that is assigned to this command
  size_type cu_idx;

  // Address of register map in command slot
  addr_type regmap_addr;

  // Size of register map in command slot (in 32 bit words)
  size_type regmap_size;
};

struct mb_validation
{
  value_type place_holder;

  value_type timestamp;

  value_type cq_read_single;

  value_type cq_write_single;

  value_type cu_read_single;

  value_type cu_write_single;
};

struct mb_access_test
{
  value_type place_holder;

  value_type h2h_access;

  value_type h2d_access;

  value_type d2h_access;

  value_type d2d_access;

  value_type d2cu_access;

  value_type wr_count;

  value_type wr_test;
};

static struct mb_validation mb_bist;

static struct mb_access_test mb_access;
// Fixed sized map from slot_idx -> slot info
static struct slot_info command_slots[MAX_SLOTS];

// Fixed sized map from cu_idx -> slot_idx
static size_type cu_slot_usage[MAX_CUS];

// Fixed sized map from cu_idx -> number of times executed
static size_type cu_usage[MAX_CUS];

// Cached cmd queue header to avoid expensive IO read
static value_type slot_cache[MAX_SLOTS];

// Bitmask indicating status of CUs. (0) idle, (1) running.
static value_type cu_status[CU_STATUS_MASK_NUM];

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


static inline void
ert_memcpy(addr_type dst, addr_type *src, value_type size)
{
  size_type offset;
  for (offset=0; offset<size; offset+=4) {
    volatile addr_type *ptr = (src+(offset/4));
    
    write_reg(dst+offset, *ptr);
  }

}

/**
 * Command opcode [27:23]
 */
static inline value_type
opcode(value_type header_value)
{
  return (header_value >> 23) & MASK_BIT_32(5);
}
  
/**
 * Command type [31:28]
 */
static inline value_type
cmd_type(value_type header_value)
{
  return (header_value >> 28) & MASK_BIT_32(4);
}

/**
 * Command header [22:12] is payload size
 */
static inline size_type
payload_size(value_type header_value)
{
  return (header_value >> 12) & MASK_BIT_32(11);
}

/**
 * Command header [11:10] is extra cu masks.
 */
static inline size_type
cu_masks(value_type header_value)
{
  return 1 + ((header_value >> 10) & MASK_BIT_32(2));
}

/**
 * CU section (where the cu bitmasks start)
 */
static inline addr_type
cu_section_addr(addr_type slot_addr)
{
  return slot_addr + sizeof(addr_type);
}

/**
 * Regmap section (where the cu regmap) is immediately
 * after cu section
 */
static inline addr_type
regmap_section_addr(value_type header_value, addr_type slot_addr)
{
  return cu_section_addr(slot_addr) + cu_masks(header_value)*sizeof(addr_type);
}

/**
 * Size of regmap is payload size (n) minus the number of cu_masks
 */
static inline size_type
regmap_size(value_type header_value)
{
  return payload_size(header_value) - cu_masks(header_value);
}

static inline addr_type
cu_idx_to_addr(size_type cu_idx)
{
  return CU_ADDR(cu_addr_map[cu_idx]);
}

static inline value_type
cu_idx_to_ctrl(size_type cu_idx)
{
  return CU_HANDSHAKE(cu_addr_map[cu_idx]);
}

static inline value_type
h2h_access_addr(addr_type slot_addr)
{
  return slot_addr + 0x4;
}

static inline value_type
wr_count_addr(addr_type slot_addr)
{
  return slot_addr + 0x18;
}

static inline value_type
wr_test_addr(addr_type slot_addr)
{
  return slot_addr + 0x1C;
}
/**
 * idx_in_mask() - Check if idx in in specified 32 bit mask
 *
 * @idx: Index to check
 * @mask_idx: Index of bit mask determines range of mask (1=>[63,32])
 * Return: @true of idx is in range of 32 bit mask, @false otherwise
 */
static inline bool
idx_in_mask(size_type idx, size_type mask_idx)
{
  return idx < ((mask_idx+1)<<REG32_MASK_SHIFT);
}

/**
 * idx_to_mask() - Return the bitmask corresponding to idx in mask with idx
 *
 * @idx: Index to map to specified mask
 * @mask_idx: Index of bit mask determines range of mask (1=>[63,32])
 * Return: 32 bit bitmask with position of translated idx set to '1', or 0x0
 *  if idx is not covered by range of mask
 */
static inline bitmask_type
idx_to_mask(size_type idx, size_type mask_idx)
{
  return idx_in_mask(idx,mask_idx)
    ? 1 << (idx-(mask_idx<<REG32_MASK_SHIFT))
    : 0;
}


static value_type
read_clk_counter(void)
{
  return read_reg(ERT_CLK_COUNTER_ADDR);
}


inline static void
setup_ert_base_addr()
{
  size_type i;
  // Prevent setup() from writing to unitialized bogus addresses
  for (i = 0; i < MAX_CUS; ++i)
    cu_addr_map[i] = AP_CTRL_NONE;
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

/**
 * MB configuration
 */
static void
setup()
{
  size_type i;
  CTRL_DEBUG("-> setup\r\n");

  // In dataflow number of slots is number of CUs plus ctrl slot (0),
  // otherwise its as many slots as possible per slot_size
  num_slots = ERT_CQ_SIZE / slot_size;

  CTRL_DEBUGF("slot_size=0x%x\r\n",slot_size);
  CTRL_DEBUGF("num_slots=%d\r\n",num_slots);
  CTRL_DEBUGF("num_cus=%d\r\n",num_cus);
  CTRL_DEBUGF("cu_offset=%d\r\n",cu_offset);
  CTRL_DEBUGF("cu_base_address=0x%x\r\n",cu_base_address);
  CTRL_DEBUGF("cdma_enabled=%d\n",cdma_enabled);
  CTRL_DEBUGF("mb_host_int_enabled=%d\r\n",mb_host_interrupt_enabled);
  CTRL_DEBUGF("dataflow_enabled=%d\r\n",dataflow_enabled);
  CTRL_DEBUGF("kds_new=%d\r\n",kds_new);
  CTRL_DEBUGF("dmsg=%d\r\n",dmsg);
  CTRL_DEBUGF("echo=%d\r\n",echo);

  // Initialize command slots
  for (i=0; i<num_slots; ++i) {
    struct slot_info *slot = &command_slots[i];
    slot->slot_addr = ERT_CQ_BASE_ADDR + (slot_size * i);
    slot->cu_idx = no_index;
    slot->regmap_addr = 0;
    slot->regmap_size = 0;

    // Clear command queue headers memory
    write_reg(slot->slot_addr,0x0);
    slot_cache[i] = 0;
  }

  // Clear CSR  (COR so read)
  /* To avoid unused variable compile warning
   * Add dummy operation
   */
  for (i=0; i<4; ++i) {
    volatile value_type csr_val = read_reg(STATUS_REGISTER_ADDR[i]);
    if (csr_val)
      csr_val = 0;
  }

  for (i=0; i<4; ++i)
    cu_status[i] = 0;

  // Initialize cu_slot_usage
  for (i=0; i<num_cus; ++i) {
    cu_slot_usage[i] = no_index;
    cu_usage[i] = 0;
  }

  // Set slot size (4K)
  write_reg(ERT_CQ_SLOT_SIZE_ADDR,slot_size/4);

  // CU offset in shift value

  write_reg(ERT_CU_OFFSET_ADDR,cu_offset);

  // Number of command slots
  write_reg(ERT_CQ_NUMBER_OF_SLOTS_ADDR,num_slots);

  // CU base address
  write_reg(ERT_CU_BASE_ADDRESS_ADDR,cu_base_address/4);

  // Command queue base address
  // The HW has changed so that the CQ is accessed at a different
  // address by the cudma_controller (which is internal to the ERT
  // subsystem) than the address it is accessed to my masters which
  // are external to the ERT subsystem e.g.XRT.
  //
  // So the cudma_controller has the CQ BRAM mapped at address
  // 0x0000_0000, but it will be at address ERT_CQ_BASE_ADDR
  // (i.e. 0x0034_0000) for XRT. And the code above was updated to
  // reflect this.
#if defined(ERT_BUILD_U50) || defined(ERT_BUILD_V20) || defined(ERT_BUILD_V30)
  write_reg(ERT_CQ_BASE_ADDRESS_ADDR,0x0);
#else
  write_reg(ERT_CQ_BASE_ADDRESS_ADDR,ERT_CQ_BASE_ADDR/4);
#endif

  // Number of CUs
  write_reg(ERT_NUMBER_OF_CU_ADDR,num_cus);

  write_reg(ERT_INTC_IER_ADDR,read_reg(ERT_INTC_IER_ADDR) & ~0x4);  // disable interrupts on bit 1 & 2 (0x2|0x4)
  write_reg(ERT_CU_ISR_HANDLER_ENABLE_ADDR,0); // disable CU ISR handler  
  CTRL_DEBUGF("cu interrupt mask : %s\r\n", "cannot convert"); // to_string throws

  write_reg(ERT_INTC_MER_ADDR,read_reg(ERT_INTC_MER_ADDR) & ~0x3);           // interrupt controller master enable
  microblaze_disable_interrupts();          // disable interrupts

  // Enable/disable mb->host interrupts
  write_reg(ERT_HOST_INTERRUPT_ENABLE_ADDR,mb_host_interrupt_enabled);
  CTRL_DEBUG("<- setup\r\n");
}

/**
 * Associate CUs with a command slot
 */
static inline void
set_cu_info(size_type cu_idx, size_type slot_idx)
{
  DMSGF("cu_slot_usage[%d]=%d\r\n",cu_idx,slot_idx);
  ERT_ASSERT(cu_slot_usage[cu_idx]==no_index,"cu already used");
  cu_slot_usage[cu_idx] = slot_idx;
  ++cu_usage[cu_idx];
}

/**
 * write_command_status
 */
static inline void
notify_host(size_type cmd_idx)
{
  // notify host (update host status register)
  DMSGF("notify_host(%d)\r\n",cmd_idx);
#if 0
  auto mask_idx = cmd_idx>>5;
  auto mask = idx_to_mask(cmd_idx,mask_idx);
  write_reg(STATUS_REGISTER_ADDR[mask_idx],mask);
#else
  // this relies on 1<<n == 1<<(n%32), safe?  no, not safe, to be fixed
  // once 128 slots work so I can check the actual behavior
  write_reg(STATUS_REGISTER_ADDR[cmd_idx>>REG32_MASK_SHIFT],1<<cmd_idx);
#endif
}

/**
 * Configure a CU at argument address
 *
 * Write register map to CU control register at address
 *
 * @param cu_addr
 *  Address of CU control register
 * @param regmap_addr
 *  The address of the register map to copy into the CU control
 *  register
 * @param regmap_size
 *  The size of register map in 32 bit words
 */
static inline void
configure_cu(addr_type cu_addr, addr_type regmap_addr, size_type regmap_size)
{
  // write register map, starting at base + 0x10
  // 0x4, 0x8, 0xc used for interrupt, which is initialized in setup
  size_type idx;

  for (idx = 4; idx < regmap_size; ++idx)
    write_reg(cu_addr + (idx << 2), read_reg(regmap_addr + (idx << 2)));

  /* We know for-loop is 2% slower than memcpy().
   * But unstable behavior are observed when using memcpy().
   * Sometimes, it does not fully configure all registers.
   * We failed to find a stable pattern to use memcpy().
   * Don't waste your life to it again.
   */
  // start kernel at base + 0x0
  write_reg(cu_addr, AP_START);
}

/**
 * Configure CU with address value pairs (out-of-order)
 */
static inline void
configure_cu_ooo(addr_type cu_addr, addr_type regmap_addr, size_type regmap_size)
{
  // write register map addr, value pairs starting 
  size_type idx;

  for (idx = 0; idx < regmap_size; idx += 2) {
    addr_type offset = read_reg(regmap_addr + (idx << 2));
    value_type value = read_reg(regmap_addr + ((idx + 1) << 2));
    write_reg(cu_addr + offset, value);
  }

  // start kernel at base + 0x0
  write_reg(cu_addr, AP_START);
}

/**
 * Check command status
 *
 * Called from CU interrupt service routine.  Argument CU is
 * complete, notify host and update state to indiciate slot
 * is now free.
 *
 * @param slot_idx
 *  Index of command slot that is associated with the CU
 * @param cu_idx
 *  Index of CU that just completed
 */
static inline void
check_command(size_type slot_idx, size_type cu_idx)
{
#ifdef ERT_VERBOSE
  struct slot_info *slot = &command_slots[slot_idx];
#endif
  DMSGF("cu_idx(%d) slot_idx(%d) \r\n",cu_idx,slot_idx);
  ERT_ASSERT(slot->cu_idx == cu_idx,"cu is not used by slot");
  notify_host(slot_idx);
  slot_cache[slot_idx] = 0;
  DMSGF("slot(%d) [running -> free]\r\n",slot_idx);

#ifdef DEBUG_SLOT_STATE
  write_reg(slot->slot_addr,slot->header_value);
#endif
}

static bool
data_integrity(value_type slot_idx)
{
  struct slot_info *slot = &command_slots[slot_idx];
  value_type cnt = 0;
  addr_type slot_addr = slot->slot_addr;
  size_type offset, i;

  while ((cnt = read_reg(wr_count_addr(slot_addr)))) {
    value_type pattern = read_reg(wr_test_addr(slot_addr));
    if (pattern != 0x0 && pattern != 0xFFFFFFFF) {
      CTRL_DEBUGF("read undefined value = 0x%x\r\n",pattern);
      mb_access.wr_test = 0;
    }
  }

  mb_access.h2h_access = read_reg(h2h_access_addr(slot_addr));
  for (offset=sizeof(struct mb_access_test); offset<slot_size; offset+=4) {
    volatile value_type pattern = read_reg(slot_addr+offset);
    if (pattern != HOST_RW_PATTERN) {
      mb_access.h2d_access = 0;
      CTRL_DEBUGF("h2d_access failed, pattern = 0x%x slot->slot_addr 0x%x\r\n",pattern,slot_addr+offset);
      break;
    }

    write_reg(slot_addr+offset, DEVICE_RW_PATTERN);
    pattern = read_reg(slot_addr+offset);

    if (pattern != DEVICE_RW_PATTERN) {
      mb_access.d2d_access = 0;
      CTRL_DEBUGF("d2d_access failed, pattern = 0x%x slot->slot_addr 0x%x\r\n",pattern,slot_addr+offset);
      break;
    }
  }

  for (i=0; i<num_cus; ++i) {
    addr_type addr = CU_ADDR(cu_addr_map[i]);
    value_type val = read_reg(addr);
    if (val != AP_IDLE) {
      mb_access.d2cu_access = 0;
      CTRL_DEBUGF("cu(%d) addr(0x%x) handshake(0x%x) encodedaddr(0x%x)\r\n", i, CU_ADDR(addr), CU_HANDSHAKE(addr), addr);
    }
  }

  ert_memcpy(slot_addr, (addr_type *)(&mb_access), sizeof(struct mb_access_test));
  notify_host(slot_idx);
  return true;
}

static inline bool
is_special_command(value_type opcode, size_type slot_idx)
{
  if (opcode==ERT_ACCESS_TEST)
    return data_integrity(slot_idx);
  return false;
}

static inline value_type
read_command_queue(addr_type slot_addr)
{
  value_type val = read_reg(slot_addr);
  /* TODO: Workaround for the BRAM read/write collision HW issue,
   * which will lead to ERT got incorrect command header.
   *
   * If command slot header is not zero, read command header again.
   * The second read will return the correct value.
   */
  if (val != 0)
      val = read_reg(slot_addr);

  return val;
}

static inline void
command_queue_fetch(size_type slot_idx)
{
  struct slot_info *slot = &command_slots[slot_idx];
  addr_type slot_addr = slot->slot_addr;
  value_type val = read_command_queue(slot_addr);

  if (val & AP_START) {
    DMSGF("slot idx 0x%x header 0x%x\r\n", slot_idx, val);
    write_reg(slot_addr,0x0);// clear command queue

    if (is_special_command(opcode(val), slot_idx))
      return;

    if (echo) {
      notify_host(slot_idx);
      return;
    }

    slot_cache[slot_idx] = val;
#if defined(ERT_BUILD_V30)
    addr_type addr = cu_section_addr(slot_addr);
    slot->cu_idx = read_reg(addr);
#else
    slot->cu_idx = slot_idx-1;
#endif
    slot->opcode = opcode(val);
    slot->header_value = val;
    slot->regmap_addr = regmap_section_addr(val,slot_addr);
    slot->regmap_size = regmap_size(val);
  }
}

/* cu_idx & MASK_BIT_32(5) is the upgrade version of cu_idx % 32
 */

static inline void
cu_used(value_type cu_idx)
{
  cu_status[cu_idx>>REG32_MASK_SHIFT] |= 1 << (cu_idx & MASK_BIT_32(5));
}

static inline void
cu_unused(value_type cu_idx)
{
  cu_status[cu_idx>>REG32_MASK_SHIFT] &= ~(1 << (cu_idx & MASK_BIT_32(5)));
}

static inline bool
cu_is_used(value_type cu_idx)
{
  return ((cu_status[cu_idx>>REG32_MASK_SHIFT] >> (cu_idx & MASK_BIT_32(5))) & 0x1);
}

static inline void
cu_state_check(size_type slot_idx)
{
  struct slot_info *slot = &command_slots[slot_idx];
  value_type cu_idx = slot->cu_idx;

  // check this CU if done
  if (cu_is_used(cu_idx)) {
    value_type cuvalue = read_reg(cu_idx_to_addr(cu_idx));
    if (cuvalue & (AP_DONE)) {
      value_type cu_slot = cu_slot_usage[cu_idx];
#ifndef ERT_HW_EMU
      write_reg(cu_idx_to_addr(cu_idx), AP_CONTINUE);
#endif
      notify_host(cu_slot);
      cu_unused(cu_idx);// now cu is available for next cmd
      slot_cache[cu_slot] = 0; // This slot have been submitted and completed, free it
    }
  }
}

static inline void
cu_execution(size_type slot_idx)
{
  struct slot_info *slot = &command_slots[slot_idx];
  value_type cu_idx = slot->cu_idx;

  if (!cu_is_used(cu_idx)) {
    if (slot_cache[slot_idx] & AP_START) {
      if (slot->opcode==ERT_EXEC_WRITE) // Out of order configuration
        configure_cu_ooo(cu_idx_to_addr(cu_idx),slot->regmap_addr,slot->regmap_size);
      else
        configure_cu(cu_idx_to_addr(cu_idx),slot->regmap_addr,slot->regmap_size);

      cu_used(cu_idx);
      set_cu_info(cu_idx,slot_idx); // record which slot cu associated with

    }
  }
}
/**
 * Configure MB and peripherals
 *
 * Wait for CONFIGURE_MB in specified slot, then configure as
 * requested.
 *
 * This function is used in two different scenarios:
 *  1. MB reset/startup, in which case the CONFIGURE_MB is guaranteed
 *     to be in a slot at default slot offset (4K), most likely slot 0.
 *  2. During regular scheduler loop, in which case the CONFIGURE_MB
 *     packet is at an arbitrary slot location.   In this scenario, the
 *     function may return (false) without processing the command if
 *     other commands are currently executing; this is to avoid hardware
 *     lockup.
 *
 * @param slot_idx
 *   The slot index with the CONFIGURE_MB command
 * @return
 *   True if CONFIGURE_MB packet was processed, false otherwise
 */
static bool
configure_mb(size_type slot_idx)
{
  CTRL_DEBUGF("-->configure_mb\r\n");
  struct slot_info *slot = &command_slots[slot_idx];
  addr_type slot_addr = slot->slot_addr;
  size_type i;

  CTRL_DEBUGF("configure cmd found in slot(%d)\r\n",slot_idx);
  slot_size=read_reg(slot_addr + 0x4);
  num_cus=read_reg(slot_addr + 0x8);
  cu_offset=read_reg(slot_addr + 0xC);
  cu_base_address=read_reg(slot_addr + 0x10);

  // Features
  value_type features = read_reg(slot_addr + 0x14);
  CTRL_DEBUGF("features=0x%04x\r\n",features);
  ERT_ASSERT(features & ERT_ENABLE,"ert is not enabled!!");
  mb_host_interrupt_enabled = (features & MB_HOST_INTR_ENABLE)==0;

  cdma_enabled = (features & CDMA_ENABLE)!=0;
  dataflow_enabled = (features & DATAFLOW_ENABLE)!=0;
  kds_new = (features & KDS_NEW)!=0;
  #ifndef ERT_HW_EMU
  ERT_ASSERT(kds_new,"Not NEW KDS!!");
  #endif
  dmsg = (features & DMSG_ENABLE)!=0;
  echo = (features & ECHO_MODE)!=0;

  // CU base address
  for (i=0; i<num_cus; ++i) {
    value_type addr = read_reg(slot_addr + 0x18 + (i<<2));
    cu_addr_map[i] = addr;  // encoded with handshake
    CTRL_DEBUGF("cu(%d) addr(0x%x) handshake(0x%x) encodedaddr(0x%x)\r\n", i, CU_ADDR(addr), CU_HANDSHAKE(addr), addr);
  }

  // (Re)initilize MB
  setup();

  // notify host
  notify_host(slot_idx);

  CTRL_DEBUGF("<--configure_mb\r\n");
  return true;
}

static bool
exit_mb(size_type slot_idx)
{
  struct slot_info *slot = &command_slots[slot_idx];
  CTRL_DEBUGF("exit_mb slot(%d) header=0x%x\r\n",slot_idx,slot->header_value);

  // Update registers so mgmt driver knows ERT has exited
  slot->header_value = (slot->header_value & ~0xF) | 0x4; // free
  write_reg(slot->slot_addr,slot->header_value); // acknowledge the completed control command
  CTRL_DEBUGF("scheduler loop exits slot(%d) header=0x%x\r\n",slot_idx,slot->header_value);
  notify_host(slot_idx);
#if defined(ERT_BUILD_V30)
#ifndef ERT_HW_EMU
  mb_sleep();
#endif
#else
  exit(0);
#endif
  return true;
}

// Gather ERT stats in ctrl command packet
// [1  ]      : header
// [1  ]      : custat version
// [1  ]      : ert version
// [1  ]      : number of cq slots
// [1  ]      : number of cus
// [#numcus]  : cu execution stats (number of executions)
// [#numcus]  : cu status (1: running, 0: idle)
// [#slots]   : command queue slot status
static bool
cu_stat(size_type slot_idx)
{
  struct slot_info *slot = &command_slots[slot_idx];
  addr_type slot_addr = slot->slot_addr;
  CTRL_DEBUGF("slot(%d) [new -> queued -> running]\r\n",slot_idx);
  CTRL_DEBUGF("cu_stat slot(%d) header=0x%x\r\n",slot_idx,slot->header_value);

  // write stats to command package after header
  size_type pkt_idx = 1; // after header
  size_type max_idx = slot_size >> 2; 
  size_type i;

  // custat version, update when changing layout of packet
  write_reg(slot_addr + (pkt_idx++ << 2),0x51a10000);

  // ert git version
  write_reg(slot_addr + (pkt_idx++ << 2),ERT_VERSION);

  // number of cq slots
  write_reg(slot_addr + (pkt_idx++ << 2),num_slots);

  // number of cus
  write_reg(slot_addr + (pkt_idx++ << 2),num_cus);
  
  // individual cu execution stat
  for (i=0; pkt_idx<max_idx && i<num_cus; ++i) {
    DMSGF("cu_usage[0x%x]=%d\r\n",cu_idx_to_addr(i),cu_usage[i]);
    write_reg(slot_addr + (pkt_idx++ << 2),cu_usage[i]);
  }

  // individual cu status
  for (i=0; pkt_idx<max_idx && i<num_cus; ++i) {
    DMSGF("cu_staus[0x%x]=%d\r\n",cu_idx_to_addr(i),(uint32_t)cu_is_used(i));
    write_reg(slot_addr + (pkt_idx++ << 2),cu_is_used(i));
  }

  // command slot status
  for (i=0; pkt_idx<max_idx && i<num_slots; ++i) {
    struct slot_info s = command_slots[i];
    DMSGF("slot_status[%d]=%d\r\n",i,s.header_value & MASK_BIT_32(4));
    write_reg(s.slot_addr + (pkt_idx++ << 2),s.header_value & MASK_BIT_32(4));
  }

#if 0
  // payload count
  auto mask = 0X7FF << 12;  // [22-12]
  slot->header_value = (slot->header_value & (~mask)) | (pkt_idx << 12);
  CTRL_DEBUGF("cu_stat new header=0x%x\r\n",slot->header_value);
  write_reg(slot->slot_addr, slot->header_value);
#endif

  // notify host
  notify_host(slot_idx);
  return true;
}

static bool
abort_mb(size_type slot_idx)
{
  CTRL_DEBUGF("abort cmd found in slot(%d)\r\n",slot_idx);

  struct slot_info *slot = &command_slots[slot_idx];
  size_type sidx = (slot->header_value >>  15) & MASK_BIT_32(8);
  struct slot_info *s = &command_slots[sidx];
  if (opcode(s->header_value)!=ERT_START_KERNEL)
    return true; // bail if not a start_kernel command
  /* If the target cu of the command is not running
   * Means we don't submit it yet, bail out 
   */
  if (!cu_is_used(s->cu_idx))
    return true; // 
  value_type cu_idx = s->cu_idx;
  check_command(sidx,cu_idx);
  cu_slot_usage[cu_idx] = no_index;
  cu_unused(cu_idx);
  notify_host(slot_idx);
  return true;
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

static bool
validate_mb(value_type slot_idx)
{
  struct slot_info *slot = &command_slots[slot_idx];
  value_type start_t, end_t, cnt = 1024;

  start_t = read_clk_counter();
  repetition_read(slot->slot_addr, cnt);
  end_t = read_clk_counter();
  mb_bist.cq_read_single = (end_t-start_t)/cnt;
   
  start_t = read_clk_counter();
  repetition_write(slot->slot_addr, cnt);
  end_t = read_clk_counter();
  mb_bist.cq_write_single = (end_t-start_t)/cnt;

  start_t = read_clk_counter();
  repetition_read(cu_idx_to_addr(0), cnt);
  end_t = read_clk_counter();
  mb_bist.cu_read_single = (end_t-start_t)/cnt;

  start_t = read_clk_counter();
  repetition_write(cu_idx_to_addr(0), cnt);
  end_t = read_clk_counter();
  mb_bist.cu_write_single = (end_t-start_t)/cnt; 

  ert_memcpy(slot->slot_addr, (addr_type *)(&mb_bist), sizeof(struct mb_validation));
  notify_host(slot_idx);
  return true;
}

static bool
clock_calib_mb(value_type slot_idx)
{
  struct slot_info *slot = &command_slots[slot_idx];

  mb_bist.timestamp = read_clk_counter();
  ert_memcpy(slot->slot_addr, (addr_type *)(&mb_bist), sizeof(struct mb_validation));
  notify_host(slot_idx);
  return true;
}


/**
 * Process special command.
 *
 * Special commands are not performace critical
 *
 * @return true
 *   If command was processed, false otherwise
 */
static bool
process_special_command(value_type opcode, size_type slot_idx)
{
  if (opcode==ERT_CONFIGURE) // CONFIGURE_MB
    return configure_mb(slot_idx);
  else if (opcode==ERT_CU_STAT)
    return cu_stat(slot_idx);
  else if (opcode==ERT_EXIT)
    return exit_mb(slot_idx);
  else if (opcode==ERT_ABORT)
    return abort_mb(slot_idx);
  else if (opcode==ERT_CLK_CALIB)
    return clock_calib_mb(slot_idx);
  else if (opcode==ERT_MB_VALIDATE)
    return validate_mb(slot_idx);
  else if (opcode==ERT_ACCESS_TEST_C) {
    mb_access.h2h_access  = 0;
    mb_access.h2d_access  = 1;
    mb_access.d2d_access  = 1;
    mb_access.d2h_access  = 1;
    mb_access.d2cu_access = 1;
    mb_access.wr_test     = 1;
    return data_integrity(slot_idx);
  }
  return false;
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
  ERT_DEBUGF("ERT scheduler\r\n");
  size_type slot_idx;

  // Set up ERT base address, this should only call once
  setup_ert_base_addr();

  // Basic setup will be changed by configure_mb, but is necessary
  // for even configure_mb() to work.
  setup();

  while (1) {
    for (slot_idx=0; slot_idx<num_slots; ++slot_idx) {
      struct slot_info *slot = &command_slots[slot_idx];

#ifdef ERT_HW_EMU
      reg_access_wait();
#endif
      if (slot_idx>0) {

        if (!slot_cache[slot_idx])
            command_queue_fetch(slot_idx);

        // we have nothing else to do
        if (!slot_cache[slot_idx])
          continue;

        cu_state_check(slot_idx);

        cu_execution(slot_idx);

      } else {
        value_type val = read_command_queue(slot->slot_addr);   

        if (val & AP_START) 
          process_special_command(opcode(val),slot_idx);
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
