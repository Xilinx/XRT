/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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
#include "ert.h"
#include "ert_fa.h"

#include "xrt/scheduler/command.h"
#include "xrt/scheduler/scheduler.h"

#include "core/common/xclbin_parser.h"

#include <iostream>
#include <fstream>
#include <bitset>

#ifdef _WIN32
#pragma warning ( disable : 4996 4267 )
#endif

namespace {

const char*
value_or_empty(const char* value)
{
  return value ? value : "";
}

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
run_start_callbacks(const xrt_xocl::command* cmd, const execution_context* ctx)
{
  for (auto& cb : cmd_start_cb)
    cb(cmd,ctx);
}

inline void
run_done_callbacks(const xrt_xocl::command* cmd, const execution_context* ctx)
{
  for (auto& cb : cmd_done_cb)
    cb(cmd,ctx);
}

struct execution_context::start_kernel : xrt_xocl::command
{
public:
  start_kernel(xrt_xocl::device* xdevice, xocl::execution_context* ec, ert_cmd_opcode opcode)
    : xrt_xocl::command(xdevice,opcode), m_ec(ec)
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

static void
fill_regmap(execution_context::regmap_type& regmap, size_t offset,
            ert_cmd_opcode opcode, uint32_t ctrl,
            const void* data, const size_t size,
            const xocl::kernel::argument::arginfo_range_type& arginforange)
{
  using value_type = uint32_t;
  using pointer_type = const uint32_t*;

  const size_t wsize = sizeof(value_type);
  const char* host_data = reinterpret_cast<const char*>(data);
  auto bytes = size;

  // For each component of the argument
  for (auto arginfo : arginforange) {
    auto component = host_data + arginfo->hostoffset;
    auto word = reinterpret_cast<pointer_type>(component);

    // For each 32-bit word of the component
    for (size_t wi = 0, we = arginfo->size / wsize; wi < we; ++wi, ++word, bytes -= wsize) {
      value_type device_value = 0;
      if (bytes >= wsize)
        device_value = *word;
      else {
        auto cword = reinterpret_cast<const char*>(word);
        std::copy(cword,cword+bytes,reinterpret_cast<char*>(&device_value));
      }

      if (opcode == ERT_EXEC_WRITE) {
        // write addr value pair at current end of regmap
        auto idx = regmap.size();
        regmap[idx++] = (ctrl == ACCEL_ADAPTER) ? arginfo->offset : arginfo->offset + wi*wsize;
        regmap[idx++] = device_value;
        assert(idx == regmap.size());
      }
      else {
        // write relative to start of register map
        auto idx = offset + arginfo->offset / wsize + wi;
        regmap[idx] = device_value;
      }
    }
  }
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

  m_dataflow = xrt_core::xclbin::get_dataflow(device->get_axlf());
  XOCL_DEBUGF("execution_context(%d) has dataflow(%d)\n",m_uid,m_dataflow);
}

void
execution_context::
add_compute_units(device* device)
{
  // Collect kernel's filtered CUs
  std::bitset<128> kernel_cus;
  for (auto cu : m_kernel->get_cus())
    kernel_cus.set(cu->get_index());

  // Targeted device CUs matching kernel CUs
  for (auto& scu : device->get_cus()) {
    auto cu = scu.get();
    if (kernel_cus.test(cu->get_index())) {

      // Check context creation
      if (!device->acquire_context(cu))
        continue;

      XOCL_DEBUGF("execution_context(%d) added cu(%d)\n",m_uid,cu->get_uid());
      m_cus.push_back(cu);
    }
  }

  if (m_cus.empty())
    throw xrt_xocl::error(CL_INVALID_KERNEL,
                     "kernel '"
                     + m_kernel->get_name()
                     + "' has no compute units to execute job '"
                     + std::to_string(m_uid) + "'\n");
}

ert_cmd_opcode
execution_context::
get_opcode() const
{
  switch (m_cus.front()->get_control_type()) {
  case ACCEL_ADAPTER:
    return ERT_EXEC_WRITE;
  case FAST_ADAPTER:
    return ERT_START_FA;
  default:
    return ERT_START_KERNEL;
  }
}

uint32_t
execution_context::
cu_control_type() const
{
  return m_cus.front()->get_control_type();
}

bool
execution_context::
write(const command_type& cmd)
{
  auto& packet = cmd->get_packet();
  auto data_size = packet.size() - 1; // subtract header

  // Construct command header
  auto epacket = xrt_xocl::command_cast<ert_packet*>(cmd.get());
  epacket->count = data_size;
  epacket->type  = ERT_CU;

  // Max number size is 4KB
  auto size = packet.bytes();
  if (size > 0x1000) {
    throw xrt_xocl::error(CL_OUT_OF_RESOURCES
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

  xrt_xocl::scheduler::schedule(cmd);
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

size_t
execution_context::
fill_fa_desc(void* data)
{
  // Convert payload data to fa_descriptor
  auto desc = reinterpret_cast<ert_fa_descriptor*>(data);
  auto& symbol = m_kernel->get_symbol();

  // Initialize descriptor
  desc->status = ERT_FA_ISSUED;
  desc->num_input_entries = symbol.fa_num_inputs;
  desc->input_entry_bytes = symbol.fa_input_entry_bytes;
  desc->num_output_entries = symbol.fa_num_outputs;
  desc->output_entry_bytes = symbol.fa_output_entry_bytes;

  // Iterate kernel indexed args and populate descriptor entries
  for (auto& arg : m_kernel_args) {
    if (!arg->is_indexed())
      continue;

    // Components of the argument
    auto arginforange = arg->get_arginfo_range();
    if (arginforange.size() != 1)
      // e.g. int4 not supported
      throw std::runtime_error("Multi-component arguments are not supported for FA style kernels");

    // XML meta data for this argument.  The meta data has been
    // parsed for FA specific data
    auto arginfo = *(arginforange.begin());

    // Offset into descriptor entries at precomputed offset
    auto desc_entry = reinterpret_cast<ert_fa_desc_entry*>(desc->data + arginfo->fa_desc_offset / sizeof(uint32_t));
    desc_entry->arg_offset = arginfo->offset;
    desc_entry->arg_size = arginfo->size;

    switch (arg->get_address_space()) {
      // global
      case kernel::argument::addr_space_type::SPIR_ADDRSPACE_GLOBAL:
      case kernel::argument::addr_space_type::SPIR_ADDRSPACE_CONSTANT: {
        auto mem = arg->get_memory_object();
        auto boh = mem->get_buffer_object_or_error(m_device);
        uint64_t addr = m_device->get_boh_addr(boh);
        auto words = reinterpret_cast<uint32_t*>(&addr);
        auto count = arginfo->size / sizeof(uint32_t);
        assert(count == 2);
        std::copy(words, words + count, desc_entry->arg_value);
        break;
      }

      // scalar
      case kernel::argument::addr_space_type::SPIR_ADDRSPACE_PRIVATE: {
        const void* value = arg->get_value();
        auto words = reinterpret_cast<const uint32_t*>(value);
        auto count = arginfo->size / sizeof(uint32_t);
        std::copy(words, words + count, desc_entry->arg_value);
        break;
      }

      // not supported
      default:
        throw std::runtime_error("Unsupported argument type for Fast Adapter protocol");
        break;
    }
  }
  return symbol.fa_desc_bytes;
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

  // Construct command packet and send to hardware
  auto opcode = get_opcode();
  auto cmd = std::make_shared<start_kernel>(m_device->get_xdevice(),this,opcode);
  ++m_active;

  auto& packet = cmd->get_packet();
  // Encode CUs in cu bitmasks with bits in position according to the
  // CUs that can be used
  encode_compute_units(packet);

  // Start of the command payload for CU arguments
  auto offset = packet.size();  // start of regmap

  // Amendment for FA style kernels
  if (opcode == ERT_START_FA) {
    auto data = packet.data() + offset;
    auto desc_size = fill_fa_desc(data);
    // Ensure internal packet size is adjusted to descriptor
    packet.resize(offset + desc_size / sizeof(uint32_t));
    write(cmd);
    return;
  }

  // kernel control protocol 
  auto ctrl = cu_control_type();

  auto& regmap = packet;

  // Ensure that S_AXI_CONTROL is created even when kernel
  // has no arguments.
  packet[offset]   = 0;  // control signals
  packet[offset+1] = 0;  // gier
  packet[offset+2] = 0;  // ier
  packet[offset+3] = 0;  // isr

  if (opcode == ERT_EXEC_WRITE) {
    // scheduler relies on exec_write addr,value pair
    // starting at offset+6 (4 ctrl + 2 ctx)
    // this is a mess, need separate exec_write packet.
    packet[offset+4] = 0; // ctx-in
    packet[offset+5] = 0; // ctx-out
  }

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
    if (address_space == kernel::argument::addr_space_type::SPIR_ADDRSPACE_PRIVATE)
    {
      auto arginforange = arg->get_arginfo_range();
      fill_regmap(regmap,offset,opcode,ctrl,arg->get_value(),arg->get_size(),arginforange);
    } else if(address_space == kernel::argument::addr_space_type::SPIR_ADDRSPACE_PIPES) {
	//do nothing
    } else if (address_space == kernel::argument::addr_space_type::SPIR_ADDRSPACE_GLOBAL
            || address_space == kernel::argument::addr_space_type::SPIR_ADDRSPACE_CONSTANT)
    {
      uint64_t physaddr = 0;
      if (auto mem = arg->get_memory_object()) {
        auto boh = mem->get_buffer_object_or_error(m_device);
        physaddr = m_device->get_boh_addr(boh);
      }
      else if (auto svm = arg->get_svm_object()) {
        physaddr = reinterpret_cast<uint64_t>(svm);
      }
      auto arginforange = arg->get_arginfo_range();
      assert(arginforange.size()==1);
      fill_regmap(regmap,offset,opcode,ctrl,&physaddr, arg->get_size(), arginforange);
    }
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
    size_t local_buffer_size = lwsx * lwsy * lwsz * 2048 /*XCL::Printf::getWorkItemPrintfBufferSize()*/;
    size_t group_x_size = gwsx / lwsx;
    size_t group_y_size = gwsy / lwsy;
    size_t group_id = m_cu_group_id[0] +
                      group_x_size * m_cu_group_id[1] +
                      group_y_size * group_x_size * m_cu_group_id[2];
    auto printf_buffer_offset = group_id * local_buffer_size;
    auto boh = printf_buffer->get_buffer_object_or_error(m_device);
    auto printf_buffer_base_addr = m_device->get_boh_addr(boh);
    printf_buffer_addr = printf_buffer_base_addr + printf_buffer_offset;
  }

  // Push runtime args
  for (auto& arg : m_kernel->get_rtinfo_argument_range()) {
    auto nm = arg->get_name();
    XOCL_DEBUGF("execution_context(%d) sets rtinfo(%s)\n",get_uid(),nm.c_str());
    if (nm=="work_dim")
      fill_regmap(regmap,offset,opcode,ctrl,&m_dim,sizeof(cl_uint),arg->get_arginfo_range());
    else if (nm=="global_offset")
      fill_regmap(regmap,offset,opcode,ctrl,m_goffset.data(),3*sizeof(size_t),arg->get_arginfo_range());
    else if (nm=="global_size")
      fill_regmap(regmap,offset,opcode,ctrl,m_gsize.data(),3*sizeof(size_t),arg->get_arginfo_range());
    else if (nm=="local_size")
      fill_regmap(regmap,offset,opcode,ctrl,m_lsize.data(),3*sizeof(size_t),arg->get_arginfo_range());
    else if (nm=="num_groups")
      fill_regmap(regmap,offset,opcode,ctrl,num_workgroups.data(),3*sizeof(size_t),arg->get_arginfo_range());
    else if (nm=="global_id")
      fill_regmap(regmap,offset,opcode,ctrl,m_cu_global_id.data(),3*sizeof(size_t),arg->get_arginfo_range());
    else if (nm=="local_id")
      fill_regmap(regmap,offset,opcode,ctrl,local_id.data(),3*sizeof(size_t),arg->get_arginfo_range());
    else if (nm=="group_id")
      fill_regmap(regmap,offset,opcode,ctrl,m_cu_group_id.data(),3*sizeof(size_t),arg->get_arginfo_range());
    else if (nm=="printf_buffer")
      fill_regmap(regmap,offset,opcode,ctrl,&printf_buffer_addr,sizeof(printf_buffer_addr),arg->get_arginfo_range());
  }

  // send command to mbs
  write(cmd);
}

bool
execution_context::
done(const xrt_xocl::command*)
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
  auto limit = m_dataflow ? 20*m_cus.size() : 2*m_cus.size();
  for (size_t i=m_active; !m_done && i<limit; ++i) {
    start();
    update_work();
    XOCL_DEBUG(std::cout,"active=",m_active,"\n");
  }

  return m_done;
}

} // namespace sws
