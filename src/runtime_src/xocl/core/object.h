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

#ifndef xocl_core_object_h_
#define xocl_core_object_h_

#include "CL/cl.h"
#include "xocl/api/khronos/khrICD.h"

#include "xocl/config.h"

#include <type_traits>

#if defined(__GNUC__) && __GNUC__ >= 6
# pragma GCC diagnostic ignored "-Wignored-attributes"
#endif

struct _cl_icd_dispatch; // TBD

namespace xocl {

class platform;
class device;
class context;
class event;
class command_queue;
class program;
class kernel;
class sampler;
class memory;
class stream;
class stream_mem;

/**
 * Base class for all CL API object types
 */
template <typename XOCLTYPE, typename XCLTYPE, typename CLTYPE>
class object
{
  const KHRicdVendorDispatch* m_dispatch;

public:
  typedef XOCLTYPE xocl_type;
  typedef XCLTYPE  xcl_type;
  typedef CLTYPE   cl_type;

  object() : m_dispatch(&cl_khr_icd_dispatch) {}
};

namespace detail {

template <typename CLTYPE>
struct cl_object_traits;
  
template <typename CLTYPE>
struct cl_object_traits<CLTYPE*>
{
  using xocl_type = typename CLTYPE::xocl_type;
  using xcl_type = typename CLTYPE::xcl_type;

  static xocl_type*
  get_xocl(CLTYPE* cl)
  {
    return static_cast<xocl_type*>(cl);
  }

  static xcl_type*
  get_xcl(CLTYPE* cl)
  {
    return static_cast<xcl_type*>(cl);
  }
};

}

/**
 * Get an xocl object from a CL API object
 *
 * Example:
 *  cl_platform_id cp = ...;
 *  xocl::platform* xp = obj(cp);
 *
 * This function simply does a static downcast of the API object.
 * The static downcast is safe as long as the CL API object is a
 * standard layout object. 
 */
template <typename CLTYPE>
typename detail::cl_object_traits<CLTYPE>::xocl_type*
xocl(CLTYPE c)
{
  return detail::cl_object_traits<CLTYPE>::get_xocl(c);
}

template <typename CLTYPE>
typename detail::cl_object_traits<CLTYPE>::xcl_type*
xcl(CLTYPE c)
{
  return detail::cl_object_traits<CLTYPE>::get_xcl(c);
}

template <typename CLTYPE>
void
assign(CLTYPE* p, CLTYPE c)
{
  if (p) {
    xocl::xocl(c)->retain();
    *p = c;
  }
}

template <typename CLTYPE,typename XOCLTYPE>
void
assign(CLTYPE* p, XOCLTYPE c)
{
  if (p) {
    c->retain();
    *p = c;
  }
}

inline void
assign(cl_int* errorvar, cl_int errcode)
{
  if (errorvar)
    *errorvar = errcode;
}

inline void
assign(cl_int* errorvar, unsigned int errcode)
{
  if (errorvar)
    *errorvar = errcode;
}

inline void
assign(cl_uint* resultvar, cl_uint value)
{
  if (resultvar)
    *resultvar = value;
}

template <typename CLTYPE>
CLTYPE
retobj(CLTYPE c)
{
  if (c)
    c->retain();
  return c;
}

}

// Wire in old xilinxopencl classes until their content is moved
// to xocl:: objects. the XCLTYPE disappears once everything is 
// ported to new xocl objects.
class _xcl_platform_id;
class _xcl_device_id;
class _xcl_context;
class _xcl_event;
class _xcl_command_queue;
class _xcl_program;
class _xcl_kernel;
class _xcl_sampler;
class _xcl_mem;
class _xcl_stream;
class _xcl_stream_mem;

struct _cl_platform_id :   public xocl::object<xocl::platform,     _xcl_platform_id,  _cl_platform_id> {};
struct _cl_device_id :     public xocl::object<xocl::device,       _xcl_device_id,    _cl_device_id> {};
struct _cl_context :       public xocl::object<xocl::context,      _xcl_context,      _cl_context> {};
struct _cl_event :         public xocl::object<xocl::event,        _xcl_event,        _cl_event> {};
struct _cl_command_queue : public xocl::object<xocl::command_queue,_xcl_command_queue,_cl_command_queue> {};
struct _cl_program :       public xocl::object<xocl::program,      _xcl_program,      _cl_program> {};
struct _cl_sampler :       public xocl::object<xocl::sampler,      _xcl_sampler,      _cl_sampler> {};
struct _cl_kernel :        public xocl::object<xocl::kernel,       _xcl_kernel,       _cl_kernel> {};
struct _cl_mem :           public xocl::object<xocl::memory,       _xcl_mem,          _cl_mem> {};
struct _cl_stream :        public xocl::object<xocl::stream,       _xcl_stream,       _cl_stream> {};
struct _cl_stream_mem :    public xocl::object<xocl::stream_mem,   _xcl_stream_mem,   _cl_stream_mem> {};

#endif


