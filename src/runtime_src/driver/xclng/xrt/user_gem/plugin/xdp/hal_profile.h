#ifndef XDP_PROFILE_HAL_PLUGIN_H_
#define XDP_PROFILE_HAL_PLUGIN_H_

#include <functional>
#include <iostream>

enum class HalFuncType {
  PROBE,
  OPEN
};

namespace xdphal {

typedef void(*cb_probe_load_func_type)();

using cb_probe_func_type = std::function<void()>;
using cb_open_func_type = std::function<void()>;
using cb_close_func_type = std::function<void()>;
using cb_load_xclbin_func_type = std::function<void()>;
using cb_lock_device_func_type = std::function<void()>;
using cb_unlock_device_func_type = std::function<void()>;
using cb_open_context_func_type = std::function<void()>;
using cb_close_context_func_type = std::function<void()>;
using cb_alloc_bo_func_type = std::function<void()>;
using cb_alloc_user_ptr_bo_func_type = std::function<void()>;
using cb_free_bo_func_type = std::function<void()>;
using cb_write_bo_func_type = std::function<void()>;
using cb_read_bo_func_type = std::function<void()>;
using cb_map_bo_func_type = std::function<void()>;
using cb_sync_bo_func_type = std::function<void()>;
using cb_copy_bo_func_type = std::function<void()>;
using cb_unmgd_pread_func_type = std::function<void()>;
using cb_unmgd_pwrite_func_type = std::function<void()>;
using cb_reg_read_func_type = std::function<void()>;
using cb_reg_write_func_type = std::function<void()>;
using cb_exec_buf_func_type = std::function<void()>;
using cb_exec_buf_waitlist_func_type = std::function<void()>;
using cb_exec_wait_func_type = std::function<void()>;
using cb_create_write_queue_func_type = std::function<void()>;
using cb_create_read_queue_func_type = std::function<void()>;
using cb_alloc_qdma_func_type = std::function<void()>;
using cb_free_qdma_func_type = std::function<void()>;

class HalCallLogger {
public:
  HalCallLogger(HalFuncType funcType);
  ~HalCallLogger();
  static bool loaded;
};

void load_xdp_plugin_library();

} //  xdphal

/**
 * The declaration of the macros to be inserted into 
 * the shim implementations
 */
#define XDP_LOG_PROBE_CALL(x) xdphal::HalCallLogger hal_plugin_object(x);

#endif