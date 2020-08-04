#include "plugin/xdp/hal_profile.h"
#include "plugin/xdp/hal_device_offload.h"
#include "plugin/xdp/power_profile.h"
#include "core/common/module_loader.h"
#include "core/common/utils.h"
#include "core/common/config_reader.h"
#include "core/common/message.h"
#include "core/common/dlfcn.h"

namespace bfs = boost::filesystem;

namespace xdphal {

std::function<void(unsigned, void*)> cb ;

static bool cb_valid() {
  return cb != nullptr ;
}

static bool hal_plugins_loaded = false ;

CallLogger::CallLogger(uint64_t id)
           : m_local_idcode(id)
{
  if (hal_plugins_loaded) return ;
  hal_plugins_loaded = true ;

  // This hook is responsible for loading all of the HAL level plugins
  if (xrt_core::config::get_xrt_profile())
  {
    load_xdp_plugin_library(nullptr) ;
  }
  if (xrt_core::config::get_data_transfer_trace() != "off")
  {
    xdphaldeviceoffload::load_xdp_hal_device_offload() ;
  }
  if (xrt_core::config::get_power_profile())
  {
    xdppowerprofile::load_xdp_power_plugin() ;
  }
}

CallLogger::~CallLogger()
{}

AllocBOCallLogger::AllocBOCallLogger(xclDeviceHandle handle /*, size_t size, int unused, unsigned flags*/) 
    : CallLogger()
{
    if (!cb_valid()) return;
    m_local_idcode = xrt_core::utils::issue_id() ;

    CBPayload payload = {m_local_idcode, handle};
    cb(HalCallbackType::ALLOC_BO_START, &payload);
}

AllocBOCallLogger::~AllocBOCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::ALLOC_BO_END, &payload);
}

AllocUserPtrBOCallLogger::AllocUserPtrBOCallLogger(xclDeviceHandle handle /*, void *userptr, size_t size, unsigned flags*/)
    : CallLogger()
{
    if (!cb_valid()) return;
    m_local_idcode = xrt_core::utils::issue_id() ;

    CBPayload payload = {m_local_idcode, handle};
    cb(HalCallbackType::ALLOC_USERPTR_BO_START, &payload);
}

AllocUserPtrBOCallLogger::~AllocUserPtrBOCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::ALLOC_USERPTR_BO_END, &payload);
}

FreeBOCallLogger::FreeBOCallLogger(xclDeviceHandle handle /*, unsigned int boHandle*/) 
    : CallLogger()
{
    if (!cb_valid()) return;
    m_local_idcode = xrt_core::utils::issue_id() ;

    CBPayload payload = {m_local_idcode, handle};
    cb(HalCallbackType::FREE_BO_START, &payload);
}

FreeBOCallLogger::~FreeBOCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::FREE_BO_END, &payload);
}

WriteBOCallLogger::WriteBOCallLogger(xclDeviceHandle handle, size_t size /*, unsigned int boHandle, const void *src, size_t seek*/) 
    : CallLogger()
      ,m_buffer_transfer_id(0)
{
    if (!cb_valid()) return;
    m_local_idcode = xrt_core::utils::issue_id() ;

    m_buffer_transfer_id = xrt_core::utils::issue_id();

    BOTransferCBPayload payload = {{m_local_idcode, handle}, m_buffer_transfer_id, size} ;
    cb(HalCallbackType::WRITE_BO_START, &payload);
}

WriteBOCallLogger::~WriteBOCallLogger() {
    if (!cb_valid()) return;

    BOTransferCBPayload payload = {{m_local_idcode, 0}, m_buffer_transfer_id, 0};
    cb(HalCallbackType::WRITE_BO_END, &payload);
}

ReadBOCallLogger::ReadBOCallLogger(xclDeviceHandle handle, size_t size /*, unsigned int boHandle, void *dst, size_t skip*/) 
    : CallLogger()
      ,m_buffer_transfer_id(0)
{
    if (!cb_valid()) return;
    m_local_idcode = xrt_core::utils::issue_id() ;

    m_buffer_transfer_id = xrt_core::utils::issue_id() ;
    
    BOTransferCBPayload payload = {{m_local_idcode, handle}, m_buffer_transfer_id, size} ;
    cb(HalCallbackType::READ_BO_START, &payload);
}

ReadBOCallLogger::~ReadBOCallLogger() {
    if (!cb_valid()) return;

    BOTransferCBPayload payload = {{m_local_idcode, 0}, m_buffer_transfer_id, 0};
    cb(HalCallbackType::READ_BO_END, &payload);
}  

MapBOCallLogger::MapBOCallLogger(xclDeviceHandle handle /*, unsigned int boHandle, bool write*/) 
    : CallLogger()
{
    if (!cb_valid()) return;
    m_local_idcode = xrt_core::utils::issue_id() ;

    CBPayload payload = {m_local_idcode, handle};
    cb(HalCallbackType::MAP_BO_START, &payload);
}

MapBOCallLogger::~MapBOCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::MAP_BO_END, &payload);
}

SyncBOCallLogger::SyncBOCallLogger(xclDeviceHandle handle, size_t size, xclBOSyncDirection dir /*, unsigned int boHandle, size_t offset*/) 
    : CallLogger()
      ,m_buffer_transfer_id(0)
      ,m_is_write_to_device((XCL_BO_SYNC_BO_TO_DEVICE == dir) ? true : false)
{
    if (!cb_valid()) return;
    m_local_idcode = xrt_core::utils::issue_id() ;

    m_buffer_transfer_id = xrt_core::utils::issue_id() ;

    SyncBOCBPayload payload = {{m_local_idcode, handle}, m_buffer_transfer_id, size, m_is_write_to_device};
    cb(HalCallbackType::SYNC_BO_START, &payload);
}

SyncBOCallLogger::~SyncBOCallLogger() {
    if (!cb_valid()) return;
    SyncBOCBPayload payload = {{m_local_idcode, 0}, m_buffer_transfer_id, 0, m_is_write_to_device};
    cb(HalCallbackType::SYNC_BO_END, &payload);
}

CopyBOCallLogger::CopyBOCallLogger(xclDeviceHandle handle /*, unsigned int dst_boHandle,
                                   unsigned int src_bohandle, size_t size, size_t dst_offset, size_t src_offset*/) 
    : CallLogger()
{
    if (!cb_valid()) return;
    m_local_idcode = xrt_core::utils::issue_id() ;

    CBPayload payload = {m_local_idcode, handle};
    cb(HalCallbackType::COPY_BO_START, &payload);
}

CopyBOCallLogger::~CopyBOCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::COPY_BO_END, &payload);
}

GetBOPropCallLogger::GetBOPropCallLogger(xclDeviceHandle handle)
    : CallLogger()
{
    if (!cb_valid()) return;
    m_local_idcode = xrt_core::utils::issue_id() ;

    CBPayload payload = {m_local_idcode, handle};
    cb(HalCallbackType::GET_BO_PROP_START, &payload);
}

GetBOPropCallLogger::~GetBOPropCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::GET_BO_PROP_END, &payload);
}

ExecBufCallLogger::ExecBufCallLogger(xclDeviceHandle handle)
    : CallLogger()
{
    if (!cb_valid()) return;
    m_local_idcode = xrt_core::utils::issue_id() ;

    CBPayload payload = {m_local_idcode, handle};
    cb(HalCallbackType::EXEC_BUF_START, &payload);
}

ExecBufCallLogger::~ExecBufCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::EXEC_BUF_END, &payload);
}

ExecWaitCallLogger::ExecWaitCallLogger(xclDeviceHandle handle)
    : CallLogger()
{
    if (!cb_valid()) return;
    m_local_idcode = xrt_core::utils::issue_id() ;

    CBPayload payload = {m_local_idcode, handle};
    cb(HalCallbackType::EXEC_WAIT_START, &payload);
}

ExecWaitCallLogger::~ExecWaitCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::EXEC_WAIT_END, &payload);
}

UnmgdPwriteCallLogger::UnmgdPwriteCallLogger(xclDeviceHandle handle, unsigned flags, const void *buf, size_t count, uint64_t offset) 
    : CallLogger()
{
    if (!cb_valid()) return;
    m_local_idcode = xrt_core::utils::issue_id() ;

    UnmgdPreadPwriteCBPayload payload = {{m_local_idcode, handle}, flags, count, offset};
    cb(HalCallbackType::UNMGD_WRITE_START, &payload);
}

UnmgdPwriteCallLogger::~UnmgdPwriteCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::UNMGD_WRITE_END, &payload);
}

UnmgdPreadCallLogger::UnmgdPreadCallLogger(xclDeviceHandle handle, unsigned flags, void *buf, size_t count, uint64_t offset) 
    : CallLogger()
{
    if (!cb_valid()) return;
    m_local_idcode = xrt_core::utils::issue_id() ;

    UnmgdPreadPwriteCBPayload payload = {{m_local_idcode, handle}, flags, count, offset};
    cb(HalCallbackType::UNMGD_READ_START, &payload);
}

UnmgdPreadCallLogger::~UnmgdPreadCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::UNMGD_READ_END, &payload);
}

ReadCallLogger::ReadCallLogger(xclDeviceHandle handle, size_t size /*, xclAddressSpace space, uint64_t offset, void *hostBuf */) 
    : CallLogger()
{
    if (!cb_valid()) return;
    m_local_idcode = xrt_core::utils::issue_id() ;

    ReadWriteCBPayload payload = {{m_local_idcode, handle}, size};
    cb(HalCallbackType::READ_START, &payload);
}

ReadCallLogger::~ReadCallLogger()
{
    if (!cb_valid()) return;

    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::READ_END, &payload);
}

WriteCallLogger::WriteCallLogger(xclDeviceHandle handle, size_t size /*, xclAddressSpace space, uint64_t offset, const void *hostBuf */) 
    : CallLogger()
{
    if (!cb_valid()) return;
    m_local_idcode = xrt_core::utils::issue_id() ;

    ReadWriteCBPayload payload = { {m_local_idcode, handle}, size};
    cb(HalCallbackType::WRITE_START, (void*)(&payload));
}

WriteCallLogger::~WriteCallLogger()
{
    if (!cb_valid()) return;

    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::WRITE_END, &payload);
}


RegReadCallLogger::RegReadCallLogger(xclDeviceHandle handle, uint32_t ipIndex, uint32_t offset) 
    : CallLogger()
{
    if (!cb_valid()) return;
    m_local_idcode = xrt_core::utils::issue_id() ;

    ReadWriteCBPayload payload = {{m_local_idcode, handle}, 0};
    cb(HalCallbackType::REG_READ_START, &payload);
}

RegReadCallLogger::~RegReadCallLogger()
{
    if (!cb_valid()) return;

    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::REG_READ_END, &payload);
}

RegWriteCallLogger::RegWriteCallLogger(xclDeviceHandle handle, uint32_t ipIndex, uint32_t offset) 
    : CallLogger()
{
    if (!cb_valid()) return;
    m_local_idcode = xrt_core::utils::issue_id() ;

    ReadWriteCBPayload payload = { {m_local_idcode, handle}, 0};
    cb(HalCallbackType::REG_WRITE_START, (void*)(&payload));
}

RegWriteCallLogger::~RegWriteCallLogger()
{
    if (!cb_valid()) return;

    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::REG_WRITE_END, &payload);
}


ProbeCallLogger::ProbeCallLogger() 
    : CallLogger()
{
    if (!cb_valid()) return;
    m_local_idcode = xrt_core::utils::issue_id() ;

    CBPayload payload = {m_local_idcode, nullptr};
    cb(HalCallbackType::PROBE_START, &payload);
}

ProbeCallLogger::~ProbeCallLogger()
{
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::PROBE_END, &payload);
}

LockDeviceCallLogger::LockDeviceCallLogger(xclDeviceHandle handle) 
    : CallLogger()
{
    if (!cb_valid()) return;
    m_local_idcode = xrt_core::utils::issue_id() ;

    CBPayload payload = {m_local_idcode, handle};
    cb(HalCallbackType::LOCK_DEVICE_START, &payload);
}

LockDeviceCallLogger::~LockDeviceCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::LOCK_DEVICE_END, &payload);
}

UnLockDeviceCallLogger::UnLockDeviceCallLogger(xclDeviceHandle handle) 
    : CallLogger()
{
    if (!cb_valid()) return;
    m_local_idcode = xrt_core::utils::issue_id() ;

    CBPayload payload = {m_local_idcode, handle};
    cb(HalCallbackType::UNLOCK_DEVICE_START, &payload);
}

UnLockDeviceCallLogger::~UnLockDeviceCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::UNLOCK_DEVICE_END, &payload);
}

OpenCallLogger::OpenCallLogger(/*unsigned deviceIndex*/)
    : CallLogger()
{
    if (!cb_valid()) return;
    m_local_idcode = xrt_core::utils::issue_id() ;

    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::OPEN_START, &payload);
}

OpenCallLogger::~OpenCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::OPEN_END, &payload);
}

CloseCallLogger::CloseCallLogger(xclDeviceHandle handle) 
    : CallLogger()
{
    if (!cb_valid()) return;
    m_local_idcode = xrt_core::utils::issue_id() ;

    CBPayload payload = {m_local_idcode, handle};
    cb(HalCallbackType::CLOSE_START, &payload);
}

CloseCallLogger::~CloseCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::CLOSE_END, &payload);
}

OpenContextCallLogger::OpenContextCallLogger(/*unsigned deviceIndex*/)
    : CallLogger()
{
    if (!cb_valid()) return;
    m_local_idcode = xrt_core::utils::issue_id() ;

    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::OPEN_CONTEXT_START, &payload);
}

OpenContextCallLogger::~OpenContextCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::OPEN_CONTEXT_END, &payload);
}

CloseContextCallLogger::CloseContextCallLogger(xclDeviceHandle handle) 
    : CallLogger()
{
    if (!cb_valid()) return;
    m_local_idcode = xrt_core::utils::issue_id() ;

    CBPayload payload = {m_local_idcode, handle};
    cb(HalCallbackType::CLOSE_CONTEXT_START, &payload);
}

CloseContextCallLogger::~CloseContextCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::CLOSE_CONTEXT_END, &payload);
}

LoadXclbinCallLogger::LoadXclbinCallLogger(xclDeviceHandle handle, const void* buffer) 
                    : CallLogger(),
                      h(handle), mBuffer(buffer)
{
  if (!cb_valid()) return ;
    m_local_idcode = xrt_core::utils::issue_id() ;

  XclbinCBPayload payload = { {m_local_idcode, handle}, buffer } ;
  cb(HalCallbackType::LOAD_XCLBIN_START, &payload) ;
}

LoadXclbinCallLogger::~LoadXclbinCallLogger()
{
  if (!cb_valid()) return ;
  XclbinCBPayload payload = { {m_local_idcode, h}, mBuffer } ;
  cb(HalCallbackType::LOAD_XCLBIN_END, &payload) ;
}

  // The registration function
  void register_hal_callbacks(void* handle)
  {
    typedef void(*ftype)(unsigned, void*) ;
    cb = (ftype)(xrt_core::dlsym(handle, "hal_level_xdp_cb_func")) ;
    if (xrt_core::dlerror() != NULL) cb = nullptr ;
  }

  // The warning function
  void warning_hal_callbacks()
  {
    if(xrt_core::config::get_profile()) {
      // "profile=true" is also set. This enables OpenCL based flow for profiling. 
      // Currently, mix of OpenCL and HAL based profiling is not supported.
      xrt_core::message::send(xrt_core::message::severity_level::XRT_WARNING, "XRT",
                std::string("Both profile=true and xrt_profile=true set in xrt.ini config. Currently, these flows are not supported to work together."));
      return;
    }
  }

void load_xdp_plugin_library(HalPluginConfig* )
{
  static xrt_core::module_loader xdp_hal_loader("xdp_hal_plugin",
						register_hal_callbacks,
						warning_hal_callbacks) ;
}

}
