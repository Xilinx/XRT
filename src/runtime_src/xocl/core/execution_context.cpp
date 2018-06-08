/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

#include "execution_context.h"
#include "device.h"
#include "event.h"

#include "xrt/scheduler/command.h"
#include "xrt/scheduler/scheduler.h"

#include "xdp/debug/rt_printf_impl.h"
#include "impl/spir.h"

#include <iostream>
#include <fstream>

namespace {

const char* 
value_or_empty(const char* value) 
{ 
  return value ? value : ""; 
}

////////////////////////////////////////////////////////////////
// Conformance mode testing.
// Save currently executing contexts (s_active).
// Save context that want to use a diferent program (s_pending)
////////////////////////////////////////////////////////////////
namespace conformance {

// Global conformance mutex to ensure that exactly one thread
// at a time can do context switching (reconfig).  The recursive
// mutex allows conformance_done() to lock and still be able to call
// conformance_execute().  Note that conformance_execute() has two
// entry points, one from event trigger action and second from 
// conformance::try_pending() which is called from conformance_done().
static std::recursive_mutex s_mutex;

// Active contexts are those executing using currently loaded program
static std::vector<xocl::execution_context*> s_active;

// Pending contexts are those waiting to reconfigure the device
static std::vector<xocl::execution_context*> s_pending;

inline bool
on()
{
  static bool conf = std::getenv("XCL_CONFORMANCE")!=nullptr;
  return conf;
}

// Add context to pending list if and only if there are
// current active contexts.
// @return true if context is added as pending, false otherwise
static bool
pending(xocl::execution_context* ctx)
{
  if (s_active.empty())
    return false;
  
  s_pending.push_back(ctx);
  return true;
}

// Add context to active list of execution contexts
// @return true
static bool
active(xocl::execution_context* ctx)
{
  s_active.push_back(ctx);
  return true;
}

// Remove context from active list of execution contexts
// @return true
static bool
remove(xocl::execution_context* ctx)
{
  auto itr = std::find(s_active.begin(),s_active.end(),ctx);
  assert(itr!=s_active.end()); // return false;
  s_active.erase(itr);
  return true;
}

// Try execute pending contexts
// @return false if no pending contexts or there are active ones,
// true if execution was tried on all pending contexts
static bool
try_pending()
{
  std::vector<xocl::execution_context*> pending;
  if (!s_active.empty() || s_pending.empty())
    return false;
  pending = s_pending;
  s_pending.clear();
  for (auto ctx : pending)
    ctx->execute();
  return true;
}

} // conformance


} // namespace


namespace xocl {

static std::vector<command_callback_function_type> cmd_start_cb;
static std::vector<command_callback_function_type> cmd_done_cb;

void
add_command_start_callback(command_callback_function_type fcn)
{
  cmd_start_cb.emplace_back(std::move(fcn));
}

void 
add_command_done_callback(command_callback_function_type fcn)
{
  cmd_done_cb.emplace_back(std::move(fcn));
}


inline void
run_start_callbacks(const xrt::command* cmd, const execution_context* ctx)
{
  for (auto& cb : cmd_start_cb)
    cb(cmd,ctx);
}

inline void
run_done_callbacks(const xrt::command* cmd, const execution_context* ctx)
{
  for (auto& cb : cmd_done_cb)
    cb(cmd,ctx);
}

struct execution_context::start_kernel : xrt::command
{
public:
  start_kernel(xrt::device* xdevice, xocl::execution_context* ec)
    : xrt::command(xdevice,xrt::command::opcode_type::start_kernel), m_ec(ec)
  {}
  virtual void start() const
  {
    run_start_callbacks(this,m_ec);
  }
  virtual void done() const 
  { 
    run_done_callbacks(this,m_ec);
    m_ec->done(this); 
  }
  mutable xocl::execution_context* m_ec;
};

struct execution_context::start_kernel_conformance : start_kernel
{
  start_kernel_conformance(xrt::device* xdevice, xocl::execution_context* ec)
    : start_kernel(xdevice,ec)
  {}
  virtual void done() const 
  { 
    run_done_callbacks(this,m_ec);
    m_ec->conformance_done(this); 
  }
};

static int
fill_regmap(execution_context::regmap_type& regmap, size_t offset,
            const void* data, const size_t size, 
            const xocl::kernel::argument::arginfo_range_type& arginforange)
{
  // scale raw data input to specified size so that the
  // value when cast to uint32_t* doesn't carry junk in case
  // size is less than sizeof(uint32_t). Fill host_data 
  // conservative with an additional sizeof(uint32_t) bytes.
  const char* cdata = reinterpret_cast<const char*>(data);
  std::vector<char> host_data(cdata,cdata+size);
  host_data.resize(size+sizeof(uint32_t));

  // For each component of the argument
  for (auto arginfo : arginforange) {
    const char* component = host_data.data() + arginfo->hostoffset;
    const uint32_t* word = reinterpret_cast<const uint32_t*>(component);
    // For each 32-bit word of the component
    for (size_t wi=0, we=arginfo->size/sizeof(uint32_t); wi!=we; ++wi) {
      size_t device_offset = arginfo->offset + wi*sizeof(uint32_t);
      uint32_t device_value = *word;
      size_t register_offset = device_offset / sizeof(uint32_t);
      regmap[offset+register_offset] = device_value;
      //      std::cout << "regmap[" << register_offset << "]=" << device_value << "\n";
      ++word;
    }
  }
  return 0;
}


execution_context::
execution_context(device* device
                  ,kernel* kd
                  ,event* event
                  ,size_t work_dim
                  ,const size_t* global_work_offset
                  ,const size_t* global_work_size
                  ,const size_t* local_work_size)
  : m_dim(work_dim)
  , m_event(event)
  , m_kernel(kd)
  , m_device(device)
{
  static unsigned int count = 0;
  m_uid = count++;

  XOCL_DEBUGF("execution_context::execution_context(%d) for kernel(%s)\n",m_uid,m_kernel->get_name().c_str());
  std::copy(global_work_offset,global_work_offset+work_dim,m_goffset.begin());
  std::copy(global_work_size,global_work_size+work_dim,m_gsize.begin());
  std::copy(local_work_size,local_work_size+work_dim,m_lsize.begin());

  // Bind the kernel arguments to this context so that the same kernel
  // object can be reused while this context is executing
  for (auto& arg : m_kernel->get_argument_range())
    m_kernel_args.push_back(arg->clone());

  // Compute units to use
  add_compute_units(device);
}

void
execution_context::
add_compute_units(device* device)
{
  for (auto& scu : device->get_cus()) {
    auto cu = scu.get();
    // Check that the kernel symbol is the same between the CU kernel and
    // this context kernel.  There are test cases where two kernels share
    // the same name but have different symbol from xclbin.  This will go
    // away once we ensure that only one kernel per symbol is created in 
    // which case the kernel object address can be used from comparison.
    if(cu->get_symbol()->uid==m_kernel.get()->get_symbol_uid()) {
      XOCL_DEBUGF("execution_context(%d) adding cu(%d)\n",m_uid,cu->get_uid());
      m_cus.push_back(cu);
    }
  }
}

bool
execution_context::
write(const command_type& cmd)
{
  auto& packet = cmd->get_packet();
  auto data_size = packet.size() - 1; // subtract header

  // Construct command header
  auto epacket = cmd->get_ert_packet();
  epacket->count = data_size;

  // Max number size is 4KB
  auto size = packet.bytes();
  if (size > 0x1000) {
    throw xrt::error(CL_OUT_OF_RESOURCES
                     , std::string("control buffer size '")
                     + std::to_string(size/static_cast<double>(0x400))
                     + std::string("KB' exceeds maximum value of 4KB"));
  }

  static std::string debug_fnm = value_or_empty(std::getenv("MBS_PRINT_REGMAP"));
  if (!debug_fnm.empty()) {
    std::ofstream ostr(debug_fnm,std::ios::app);
    ostr << "# execution_context(" << get_uid() 
         << ") kernel(" << m_kernel->get_name() 
         << ") global_id(" << m_cu_global_id[0] << "," << m_cu_global_id[1] << "," << m_cu_global_id[2]
         << ") group_id(" << m_cu_group_id[0] << "," << m_cu_group_id[1] << "," << m_cu_group_id[2]
         << ")\n";
    for (size_t i=0; i<packet.size(); ++i)
      ostr << "0x" << std::uppercase << std::setfill('0') << std::setw(8) << std::hex << packet[i] << std::dec << "\n";
  }

  xrt::scheduler::schedule(cmd);
  return true;
}

void
execution_context::
encode_compute_units(packet_type& packet)
{
  // Encode CUs in a bitmask with bits in position according to the 
  // CU physical address.   The CU address is at 4k boundaries starting
  // at 0x0, so shift >> 12 to get CU index, the shift << idx
  word_type cu_bitmask[4] = {0};
  size_t no_of_masks = 0;
  for (auto cu : m_cus) {
    auto cu_idx = cu->get_index();
    auto mask_idx = cu_idx/32;
    auto cu_mask_idx = cu_idx - mask_idx*32;
    cu_bitmask[mask_idx] |= 1 << cu_mask_idx;
    no_of_masks = std::max(no_of_masks,mask_idx+1);
  }
  assert(no_of_masks >= 1);

  size_t offset = 1; // past header
  for (size_t i=0; i<no_of_masks; ++i)
    packet[offset++] = cu_bitmask[i];

  // write extra cu mask count to header [11:10]
  auto epacket = reinterpret_cast<ert_start_kernel_cmd*>(packet.data());
  epacket->extra_cu_masks = no_of_masks-1;
}

const compute_unit*
execution_context::
get_compute_unit(unsigned int cu_idx) const
{
  for (auto cu : m_cus)
    if (cu->get_index() == cu_idx)
      return cu;
  return nullptr;
}

void
execution_context::
update_work()
{
  for (unsigned int dim=0; dim<m_dim; ++dim) {

    if (m_cu_global_id[dim]+m_lsize[dim] < m_gsize[dim]) {
      m_cu_global_id[dim] += m_lsize[dim];
      ++m_cu_group_id[dim];
      return;
    }

    m_cu_global_id[dim] = m_goffset[dim];
    m_cu_group_id[dim] = 0;

  }

  m_done = true;
}

void
execution_context::
start()
{
  XOCL_DEBUGF("execution_context(%d) starting workgroup(%d,%d,%d)\n"
              ,get_uid(),m_cu_group_id[0],m_cu_group_id[1],m_cu_group_id[2]);

  // On first work load, transition event to CL_RUNNING
  if ( (m_cu_group_id[0]==0) && (m_cu_group_id[1]==0) && (m_cu_group_id[2]==0)) 
    m_event->set_status(CL_RUNNING);

  auto xdevice = m_device->get_xrt_device();

  // Construct command packet and send to hardware
  auto cmd = conformance::on()
    ? std::make_shared<start_kernel_conformance>(xdevice,this)
    : std::make_shared<start_kernel>(xdevice,this);
  ++m_active;
  auto& packet = cmd->get_packet();

  // Encode CUs in cu bitmasks with bits in position according to the
  // CUs that can be used
  encode_compute_units(packet);

  // Create the cu register map
  auto offset = packet.size();  // start of regmap
  auto& regmap = packet;

  size3 num_workgroups {0,0,0};
  for (auto d : {0,1,2}) {
    if (m_lsize[d]) // actually always true
      num_workgroups[d] = m_gsize[d]/m_lsize[d];
  }

  // Push kernel args
  xocl::memory* printf_buffer = nullptr;
  for (auto& arg : m_kernel_args) {
    if (arg->is_printf()) {
      printf_buffer = arg->get_memory_object();
      assert(printf_buffer);
      continue;
    }

    auto address_space = arg->get_address_space();
    if (address_space == SPIR_ADDRSPACE_PRIVATE) 
    {
      auto arginforange = arg->get_arginfo_range();
      fill_regmap(regmap,offset,arg->get_value(),arg->get_size(),arginforange);
    }
    else if (address_space==SPIR_ADDRSPACE_GLOBAL
             || address_space==SPIR_ADDRSPACE_CONSTANT
             || address_space==SPIR_ADDRSPACE_PIPES) 
    {
      if (address_space==SPIR_ADDRSPACE_PIPES)
        throw std::runtime_error("cu_ffa.cpp internal error, unexpected address space (pipes)");

      uint64_t physaddr = 0;
      if (auto mem = arg->get_memory_object()) {
        auto boh = xocl::xocl(mem)->get_buffer_object_or_error(m_device);
        physaddr = xdevice->getDeviceAddr(boh);
      }
      else if (auto svm = arg->get_svm_object()) {
        physaddr = reinterpret_cast<uint64_t>(svm);
      }
      auto arginforange = arg->get_arginfo_range();
      assert(arginforange.size()==1);
      fill_regmap(regmap,offset,&physaddr, arg->get_size(), arginforange);
    }
  }
  
  for (auto& arg : m_kernel->get_progvar_argument_range()) {
    uint64_t physaddr = 0;
    if (auto mem = arg->get_memory_object()) {
      auto boh = xocl::xocl(mem)->get_buffer_object_or_error(m_device);
      physaddr = xdevice->getDeviceAddr(boh);
    }
    assert(arg->get_arginfo_range().size()==1);
    fill_regmap(regmap,offset,&physaddr,arg->get_size(),arg->get_arginfo_range());
  }

  // Set runtime arguments as required
  size3 local_id {0,0,0};
  uint64_t printf_buffer_addr = 0;
  if ( printf_buffer ) {
    // This computes the offset that gets added to a physical printf buffer
    // address for a given workgroup. Necessary so we have a different
    // segment to hold each workgroup in the overall buffer.
    size_t lwsx = m_lsize[0];
    size_t lwsy = m_lsize[1];
    size_t lwsz = m_lsize[2];
    size_t gwsx = m_gsize[0];
    size_t gwsy = m_gsize[1];
    size_t local_buffer_size = lwsx * lwsy * lwsz * XCL::Printf::getWorkItemPrintfBufferSize();
    size_t group_x_size = gwsx / lwsx;
    size_t group_y_size = gwsy / lwsy;
    size_t group_id = m_cu_group_id[0] +
                      group_x_size * m_cu_group_id[1] +
                      group_y_size * group_x_size * m_cu_group_id[2];
    auto printf_buffer_offset = group_id * local_buffer_size;
    auto boh = printf_buffer->get_buffer_object_or_error(m_device);
    auto printf_buffer_base_addr = static_cast<uint64_t>(xdevice->getDeviceAddr(boh));
    printf_buffer_addr = printf_buffer_base_addr + printf_buffer_offset;
  }

  // Push runtime args
  for (auto& arg : m_kernel->get_rtinfo_argument_range()) {
    auto nm = arg->get_name();
    XOCL_DEBUGF("execution_context(%d) sets rtinfo(%s)\n",get_uid(),nm.c_str());
    if (nm=="work_dim")
      fill_regmap(regmap,offset,&m_dim,sizeof(cl_uint),arg->get_arginfo_range());
    else if (nm=="global_offset")
      fill_regmap(regmap,offset,m_goffset.data(),3*sizeof(size_t),arg->get_arginfo_range());
    else if (nm=="global_size")
      fill_regmap(regmap,offset,m_gsize.data(),3*sizeof(size_t),arg->get_arginfo_range());
    else if (nm=="local_size")
      fill_regmap(regmap,offset,m_lsize.data(),3*sizeof(size_t),arg->get_arginfo_range());
    else if (nm=="num_groups")
      fill_regmap(regmap,offset,num_workgroups.data(),3*sizeof(size_t),arg->get_arginfo_range());
    else if (nm=="global_id")
      fill_regmap(regmap,offset,m_cu_global_id.data(),3*sizeof(size_t),arg->get_arginfo_range());
    else if (nm=="local_id")
      fill_regmap(regmap,offset,local_id.data(),3*sizeof(size_t),arg->get_arginfo_range());
    else if (nm=="group_id")
      fill_regmap(regmap,offset,m_cu_group_id.data(),3*sizeof(size_t),arg->get_arginfo_range());
    else if (nm=="printf_buffer")
      fill_regmap(regmap,offset,&printf_buffer_addr,sizeof(printf_buffer_addr),arg->get_arginfo_range());
  }

  // send command to mbs
  write(cmd);
}

bool
execution_context::
done(const xrt::command*)
{
  // Care must be taken not to mark event complete and later reference
  // any data members of context which is owned (and deleted) with event
  bool ctx_done = false;
  {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (--m_active==0 && m_done)
      ctx_done=true;
  }

  // Only one thread will be able to set local ctx_done to true, so it's
  // safe to proceed without exclusive lock (mutex is a data member)
  if (ctx_done) {
    m_event->set_status(CL_COMPLETE);
    return true;
  }

  // execute more workgroups if necessary
  execute();
  return false;
}

bool
execution_context::
execute()
{
  // Mutual exclusion as multiple start_kernel commands could call execute.
  std::lock_guard<std::mutex> lk(m_mutex);

  // Conformance mode hook
  if (conformance::on()) {
    conformance_execute();
    assert(m_done);
  }

  if (m_done)
    return true;

  // Schedule workgroups.  But don't blindly schedule all workgroups
  // because that would fill the command queue with commands that
  // compete for same CUs and block (CQ full) other kernel calls that
  // may want to use other CUs.
  // 
  // In order to keep scheduler busy, we need more than just one
  // workgroup at a time, so here we try to ensure that the scheduled
  // commands at any given time is twice the number of available CUs.
  auto limit = 2*m_cus.size();
  for (size_t i=m_active; !m_done && i<limit; ++i) {
    start();
    update_work();
  }

  return m_done;
}

////////////////////////////////////////////////////////////////
// Conformance mode
////////////////////////////////////////////////////////////////
bool
execution_context::
conformance_done(const xrt::command*)
{
  // Global conformance lock
  std::lock_guard<std::recursive_mutex> lk(conformance::s_mutex);

  // Care must be taken not to mark event complete and later reference
  // any data members of context which is owned (and deleted) with event
  bool ctx_done = false;
  {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (--m_active==0) {
      assert(m_done);
      conformance::remove(this);
      ctx_done = true;
    }
  }

  // Only one thread will be able to set local ctx_done to true, so it's
  // safe to proceed without exclusive lock
  if (ctx_done) {
    m_event->set_status(CL_COMPLETE);
    conformance::try_pending(); // if no active, then try execute all pending
  }

  return true;
}

bool
execution_context::
conformance_execute()
{
  // Global conformance lock
  std::lock_guard<std::recursive_mutex> lk(conformance::s_mutex);

  bool same = m_kernel->get_program()==m_device->get_program();
  if (!same && conformance::pending(this))
    return false;

  // Either same program or no current active running contexts

  // Reeconfigure if different program
  if (!same) {
    XOCL_DEBUG(std::cout,"conformance mode reconfiguration for event(",m_event->get_uid(),") ec(",get_uid(),")\n");
    
    // Remove current CUs if any
    m_cus.clear();
    
    // reload new program and add new CUs
    m_device->load_program(m_kernel->get_program());
    add_compute_units(m_device);
  }

  // Run
  conformance::active(this);
  // Schedule all workgroups
  for (size_t i=0; !m_done; ++i) {
    start();
    update_work();
  }

  return true;
}

} // namespace sws
