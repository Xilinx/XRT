#include "command.h"
#include "xrt/device/device.h"
#include <mutex>
#include <map>
#include <vector>

namespace {

using buffer_type = xrt::device::ExecBufferObjectHandle;
static std::mutex s_mutex;

// Static destruction logic to prevent double purging. 

// Exec buffer objects must be purged before device is closed.  Static
// destruction calls platform dtor, which in turns calls purge
// commands, but static destruction could have deleted the static
// object in this file first.
static bool s_purged = false;

struct X {
  std::map<xrt::device*,std::vector<buffer_type>> freelist;
  X() {}
  ~X() { s_purged = true; }
};

static X sx;

}

namespace xrt {

command::
command(xrt::device* device, opcode_type opcode)
   : m_device(device)
    ,m_exec_bo(get_buffer(m_device,regmap_size*sizeof(value_type)))
    ,m_packet(m_device->map(m_exec_bo))
    ,m_header(m_packet[0])
{
  static unsigned int uid_count = 0;
  m_uid = uid_count++;

  // Clear in case packet was recycled
  m_packet.clear();

  ert_packet* epacket = get_ert_packet();
  epacket->state = ERT_CMD_STATE_NEW; // new command
  epacket->opcode = std::underlying_type<opcode_type>::type(opcode);

  XRT_DEBUG(std::cout,"xrt::command::command(",m_uid,")\n");
}

command::
~command()
{
  // consider reusing the exec_bos, e.g push on a free list
  if (m_exec_bo) {
    XRT_DEBUG(std::cout,"xrt::command::~command(",m_uid,")\n");
    m_device->unmap(m_exec_bo);
    free_buffer(m_device,m_exec_bo);
  }
}

command::
command(command&& rhs)
    : m_uid(rhs.m_uid), m_device(rhs.m_device)
    , m_exec_bo(std::move(rhs.m_exec_bo))
    , m_packet(std::move(rhs.m_packet))
    , m_header(m_packet[0])
{
  rhs.m_exec_bo = 0;
}


buffer_type
command::
get_buffer(xrt::device* device,size_t sz) 
{
  std::lock_guard<std::mutex> lk(s_mutex);

  auto itr = sx.freelist.find(device);
  if (itr != sx.freelist.end()) {
    auto& freelist = (*itr).second;
    if (!freelist.empty()) {
      auto buffer = freelist.back();
      freelist.pop_back();
      return buffer;
    }
  }

  return device->allocExecBuffer(sz); // not thread safe
}

void
command::
free_buffer(xrt::device* device,buffer_type bo)
{
  std::lock_guard<std::mutex> lk(s_mutex);
  sx.freelist[device].emplace_back(std::move(bo));
}


// Purge exec buffer freelist during static destruction.
// Not safe to call outside of static descruction, can't lock
// static mutex since it could have been descructed
void
purge_command_freelist()
{
  if (s_purged)
    return;

  for (auto& elem : sx.freelist)
    elem.second.clear();

  s_purged = true;
}

}
