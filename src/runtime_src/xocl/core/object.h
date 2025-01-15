// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2016-2017 Xilinx, Inc
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef xocl_core_object_h_
#define xocl_core_object_h_

#include "xocl/api/icd/ocl_icd_bindings.h"
#include "xocl/config.h"

#include <CL/cl.h>
#include <type_traits>

#if defined(__GNUC__) && __GNUC__ >= 6
# pragma GCC diagnostic ignored "-Wignored-attributes"
#endif

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

// Base class for all CL API object types
template <typename XOCLTYPE, typename CLTYPE>
class object
{
  const cl_icd_dispatch* m_dispatch;


public:
  typedef XOCLTYPE xocl_type;
  typedef CLTYPE   cl_type;

  object() : m_dispatch(&cl_icd_dispatch_obj) {}
};

namespace detail {

template <typename CLTYPE>
struct cl_object_traits;

template <typename CLTYPE>
struct cl_object_traits<CLTYPE*>
{
  using xocl_type = typename CLTYPE::xocl_type;

  static xocl_type*
  get_xocl(CLTYPE* cl)
  {
    return static_cast<xocl_type*>(cl);
  }
};

}

// Get an xocl object from a CL API object
// Example:
//  cl_platform_id cp = ...;
//  xocl::platform* xp = obj(cp);
//
// This function simply does a static downcast of the API object.
// The static downcast is safe as long as the CL API object is a
// standard layout object.
template <typename CLTYPE>
typename detail::cl_object_traits<CLTYPE>::xocl_type*
xocl(CLTYPE c)
{
  return detail::cl_object_traits<CLTYPE>::get_xocl(c);
}

// Get an xocl object from a CL API object
// Provided as a work-around for GCC too aggressive optimization
// GCC Bugzilla – Bug 104475
template <typename CLTYPE>
typename detail::cl_object_traits<CLTYPE>::xocl_type*
xocl_or_error(CLTYPE c)
{
  auto x = detail::cl_object_traits<CLTYPE>::get_xocl(c);
#ifdef __GNUC__
  if (!x)
    __builtin_unreachable();
#endif
  return x;
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
// to xocl:: objects.
struct _cl_platform_id :   public xocl::object<xocl::platform,     _cl_platform_id> {};
struct _cl_device_id :     public xocl::object<xocl::device,       _cl_device_id> {};
struct _cl_context :       public xocl::object<xocl::context,      _cl_context> {};
struct _cl_event :         public xocl::object<xocl::event,        _cl_event> {};
struct _cl_command_queue : public xocl::object<xocl::command_queue,_cl_command_queue> {};
struct _cl_program :       public xocl::object<xocl::program,      _cl_program> {};
struct _cl_sampler :       public xocl::object<xocl::sampler,      _cl_sampler> {};
struct _cl_kernel :        public xocl::object<xocl::kernel,       _cl_kernel> {};
struct _cl_mem :           public xocl::object<xocl::memory,       _cl_mem> {};
struct _cl_stream :        public xocl::object<xocl::stream,       _cl_stream> {};
struct _cl_stream_mem :    public xocl::object<xocl::stream_mem,   _cl_stream_mem> {};

#endif
