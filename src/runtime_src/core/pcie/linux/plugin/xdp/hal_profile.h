#ifndef XDP_PROFILE_HAL_PLUGIN_H_
#define XDP_PROFILE_HAL_PLUGIN_H_

#include "core/include/xclperf.h"
#include "core/include/xclhal2.h"

#include "core/common/config_reader.h"

namespace xdp {
namespace hal {

class api_call_logger
{
 protected:
  uint64_t m_id ;
  const char* m_fullname = nullptr ;
 public:
  explicit api_call_logger(const char* function) ;
  virtual ~api_call_logger() = default ;
} ;

// Generic logger that just tracks start/stop of a function

class generic_api_call_logger : public api_call_logger
{
 private:
  generic_api_call_logger()                                 = delete ;
  generic_api_call_logger(const generic_api_call_logger& x) = delete ;
  generic_api_call_logger(generic_api_call_logger&& x)      = delete ;
  void operator=(const generic_api_call_logger& x)          = delete ;
  void operator=(generic_api_call_logger&& x)               = delete ;
 public:
  explicit generic_api_call_logger(const char* function) ;
  ~generic_api_call_logger() override ;
} ;

template <typename Callable, typename ...Args>
auto
profiling_wrapper(const char* function, Callable&& f, Args&&...args)
{
  if (xrt_core::config::get_xrt_trace()) {
    generic_api_call_logger log_object(function) ;
    return f(std::forward<Args>(args)...) ;
  }
  return f(std::forward<Args>(args)...) ;
}

// Specializations that track additional data

class buffer_transfer_logger : public api_call_logger
{
 private:
  uint64_t m_buffer_id ;
  uint64_t m_size ;
  bool m_is_write ;

  buffer_transfer_logger()                                = delete ;
  buffer_transfer_logger(const buffer_transfer_logger& x) = delete ;
  buffer_transfer_logger(buffer_transfer_logger&& x)      = delete ;
  void operator=(const buffer_transfer_logger& x)         = delete ;
  void operator=(buffer_transfer_logger&& x)              = delete ;
 public:
  explicit buffer_transfer_logger(const char* function, size_t size, bool isWrite) ;
  ~buffer_transfer_logger() override ;
} ;

template <typename Callable, typename ...Args>
auto
buffer_transfer_profiling_wrapper(const char* function, size_t size,
                                  bool isWrite, Callable&& f, Args&&...args)
{
  if (xrt_core::config::get_xrt_trace()) {
    buffer_transfer_logger log_object(function, size, isWrite) ;
    return f(std::forward<Args>(args)...) ;
  }
  return f(std::forward<Args>(args)...) ;
}

void load();
void register_callbacks(void* handle) ;
void warning_callbacks() ;
int error_function() ;

} // end namespace hal
} // end namespace xdp

#endif
