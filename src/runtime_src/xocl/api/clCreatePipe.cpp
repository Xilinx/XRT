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

// Copyright 2017 Xilinx, Inc. All rights reserved.

#include <CL/opencl.h>
#include "xocl/config.h"
#include "xocl/core/context.h"
#include "xocl/core/device.h"
#include "xocl/core/memory.h"
#include "core/common/memalign.h"

#include "detail/context.h"
#include "detail/memory.h"

#include "api.h"
#include "plugin/xdp/profile.h"

#include <cstdlib>
#include <mutex>
#include <deque>

// Unused code, but left as reference to be reimplemented in needed

namespace {

struct cpu_pipe_reserve_id_t {
  std::size_t head;
  std::size_t tail;
  std::size_t next;
  unsigned int size;
  unsigned int ref;
};

struct cpu_pipe_t {
  std::mutex rd_mutex;
  std::mutex wr_mutex;
  std::size_t pkt_size;
  std::size_t pipe_size;
  std::size_t head;
  std::size_t tail;

  std::deque<cpu_pipe_reserve_id_t*> rd_rids;
  std::deque<cpu_pipe_reserve_id_t*> wr_rids;

  char buf[0];
};

/*
 * 6.13.16.2 - work-item builtins, non-reservation, non-locking
 */

XOCL_UNUSED
static int
cpu_write_pipe_nolock(void *v, void *e)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;

#ifdef PIPE_VERBOSE
  printf("cpu_write_pipe_nolock %p %p\n", v, e);
#endif

  std::size_t head = p->head;
  std::size_t next = (head + p->pkt_size) % p->pipe_size;

  while (next == p->tail);

  std::memcpy(&p->buf[head], e, p->pkt_size);
  p->head = next;

  return 0;
}

XOCL_UNUSED
static int
cpu_write_pipe_nb_nolock(void *v, void *e)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;

#ifdef PIPE_VERBOSE
  printf("cpu_write_pipe_nb_nolock %p %p\n", v, e);
#endif

  std::size_t head = p->head;
  std::size_t next = (head + p->pkt_size) % p->pipe_size;

  if (next == p->tail) {
    return -1;
  }

  std::memcpy(&p->buf[head], e, p->pkt_size);
  p->head = next;
  return 0;
}

XOCL_UNUSED
static int
cpu_read_pipe_nolock(void *v, void *e)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;

#ifdef PIPE_VERBOSE
  printf("cpu_read_pipe_nolock %p %p\n", v, e);
#endif

  std::size_t tail = p->tail;

  while (p->head == tail);

  std::memcpy(e, &p->buf[tail], p->pkt_size);
  p->tail = (tail + p->pkt_size) % p->pipe_size;
  return 0;
}

XOCL_UNUSED
static int
cpu_read_pipe_nb_nolock(void *v, void *e)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;

#ifdef PIPE_VERBOSE
  printf("cpu_read_pipe_nb_nolock %p %p\n", v, e);
#endif

  std::size_t tail = p->tail;

  if (p->head == tail) {
    return -1;
  }

  std::memcpy(e, &p->buf[tail], p->pkt_size);
  p->tail = (tail + p->pkt_size) % p->pipe_size;

  return 0;
}


XOCL_UNUSED
static int
cpu_peek_pipe_nb_nolock(void *v, void *e)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;

#ifdef PIPE_VERBOSE
  printf("cpu_peek_pipe_nb_nolock %p %p\n", v, e);
#endif

  std::size_t tail = p->tail;

  if (p->head == tail) {
    return -1;
  }
  std::memcpy(e, &p->buf[tail], p->pkt_size);
  return 0;
}


/*
 * 6.13.16.2 - work-item builtins, non-reservation, locking
 */

XOCL_UNUSED
static int
cpu_write_pipe(void *v, void *e)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;
  std::lock_guard<std::mutex> lk(p->wr_mutex);
  int ret = cpu_write_pipe_nolock(v, e);
  return ret;
}

XOCL_UNUSED
static int
cpu_write_pipe_nb(void *v, void *e)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;
  std::lock_guard<std::mutex> lk(p->wr_mutex);
  int ret = cpu_write_pipe_nb_nolock(v, e);
  return ret;
}

XOCL_UNUSED
static int
cpu_read_pipe(void *v, void *e)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;
  std::lock_guard<std::mutex> lk(p->rd_mutex);
  int ret = cpu_read_pipe_nolock(v, e);
  return ret;
}

XOCL_UNUSED
static int
cpu_read_pipe_nb(void *v, void *e)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;
  std::lock_guard<std::mutex> lk(p->rd_mutex);
  int ret = cpu_read_pipe_nb_nolock(v, e);
  return ret;
}

XOCL_UNUSED
static int
cpu_peek_pipe_nb(void *v, void *e)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;
  std::lock_guard<std::mutex> lk(p->rd_mutex);
  int ret = cpu_peek_pipe_nb_nolock(v, e);
  return ret;
}

/*
 * 6.13.16.2 - work-item builtins, reservation, locking
 */

XOCL_UNUSED
static void *
cpu_reserve_read_pipe(void *v, unsigned n)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;
  if (!v) return 0;

  std::lock_guard<std::mutex> lk(p->rd_mutex);

  std::size_t tail;
  if (p->rd_rids.size()) {
    cpu_pipe_reserve_id_t *id = p->rd_rids.back();
    tail = id->tail;
  }
  else {
    tail = p->tail;
  }

  int space = p->head - tail;
  if (space < 0)
    space += p->pipe_size;

  cpu_pipe_reserve_id_t *rid = 0;
  if ((int)n <= space) {
    rid = (cpu_pipe_reserve_id_t*)malloc(sizeof(cpu_pipe_reserve_id_t));
    if (rid) {
      // success
      rid->tail = tail;
      rid->next = (tail + (p->pkt_size*n)) % p->pipe_size;
      rid->size = n*p->pkt_size;
      rid->ref = 1;
      p->rd_rids.push_back(rid);
    }
  }

  return rid;
}

XOCL_UNUSED
static void
cpu_commit_read_pipe(void *v, void *r)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;
  cpu_pipe_reserve_id_t *rid = (cpu_pipe_reserve_id_t *)r;
  std::lock_guard<std::mutex> lk(p->rd_mutex);

  rid->ref--;
  assert(rid->ref == 0 && "bad commit on read pipe");

  while (p->rd_rids.size() && !p->rd_rids.front()->ref) {
    cpu_pipe_reserve_id_t *front = p->rd_rids.front();
    p->tail = front->next;
    p->rd_rids.pop_front();
    free(front);
  }
}

XOCL_UNUSED
static int
cpu_read_pipe_reserve(void *v, void *r, unsigned idx, void *e)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;
  cpu_pipe_reserve_id_t *rid = (cpu_pipe_reserve_id_t *)r;
  if (!p || !rid)
    return -1;

  std::size_t offset = idx*p->pkt_size;
  if (offset > (rid->size+p->pkt_size))
    return -1;

  offset = (rid->tail + offset) % p->pipe_size;
  std::memcpy(e, &p->buf[offset], p->pkt_size);

  return 0;
}

XOCL_UNUSED
static void *
cpu_reserve_write_pipe(void *v, unsigned n)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;
  if (!v) return 0;

  std::lock_guard<std::mutex> lk(p->wr_mutex);

  std::size_t head;
  if (p->wr_rids.size()) {
    cpu_pipe_reserve_id_t *id = p->wr_rids.back();
    head = id->head;
  }
  else {
    head = p->head;
  }

  int next = (head + p->pkt_size) % p->pipe_size;
  int space = p->tail - next;
  if (space < 0)
    space += p->pipe_size;

  cpu_pipe_reserve_id_t *rid = 0;
  if ((int)n <= space) {
    rid = (cpu_pipe_reserve_id_t*)malloc(sizeof(cpu_pipe_reserve_id_t));
    if (rid) {
      // success
      rid->head = head;
      rid->next = (head + (p->pkt_size*n)) % p->pipe_size;
      rid->size = n*p->pkt_size;
      rid->ref = 1;
      p->wr_rids.push_back(rid);
    }
  }

  return rid;
}

XOCL_UNUSED
static void
cpu_commit_write_pipe(void *v, void *r)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;
  cpu_pipe_reserve_id_t *rid = (cpu_pipe_reserve_id_t *)r;
  std::lock_guard<std::mutex> lk(p->wr_mutex);

  rid->ref--;
  assert(rid->ref == 0 && "bad commit on write pipe");

  while (p->wr_rids.size() && !p->wr_rids.front()->ref) {
    cpu_pipe_reserve_id_t *front = p->wr_rids.front();
    p->head = front->next;
    p->wr_rids.pop_front();
    free(front);
  }
}

XOCL_UNUSED
static int
cpu_write_pipe_reserve(void *v, void *r, unsigned idx, void *e)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;
  cpu_pipe_reserve_id_t *rid = (cpu_pipe_reserve_id_t *)r;
  if (!p || !rid)
    return -1;

  std::size_t offset = idx*p->pkt_size;
  if (offset > (rid->size+p->pkt_size))
    return -1;

  offset = (rid->head + offset) % p->pipe_size;
  std::memcpy(&p->buf[offset], e, p->pkt_size);

  return 0;
}


/*
 * 6.13.16.3 work-group builtins
 */

XOCL_UNUSED
static void *
cpu_work_group_reserve_read_pipe(void *v, unsigned n)
{
  return cpu_reserve_read_pipe(v,n);
}

XOCL_UNUSED
static void *
cpu_work_group_reserve_write_pipe(void *v, unsigned n)
{
  return cpu_reserve_write_pipe(v,n);
}

XOCL_UNUSED
static void
cpu_work_group_commit_read_pipe(void *v, void *r)
{
  cpu_commit_read_pipe(v,r);
}

XOCL_UNUSED
static void
cpu_work_group_commit_write_pipe(void *v, void *r)
{
  cpu_commit_write_pipe(v,r);
}

/*
 * 6.13.16.4 pipe query functions
 */

XOCL_UNUSED
static unsigned int
cpu_get_pipe_num_packets(void *v)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;

  std::lock_guard<std::mutex> lk(p->rd_mutex);
  std::size_t head = p->head;
  std::size_t tail;
  if (p->rd_rids.size()) {
    cpu_pipe_reserve_id_t *id = p->rd_rids.back();
    tail = id->tail;
  }
  else {
    tail = p->tail;
  }

  int space = head - tail;
  if (space < 0)
    space += p->pipe_size;

  return (unsigned int)(space / p->pkt_size);
}

XOCL_UNUSED
static unsigned int
cpu_get_pipe_max_packets(void *v)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;
  return (p->pipe_size / p->pkt_size) - 8;
}
  
}

namespace xocl {

static cl_uint
getDevicePipeMaxPacketSize(cl_device_id device)
{
  cl_uint size = 0;
  api::clGetDeviceInfo(device,CL_DEVICE_PIPE_MAX_PACKET_SIZE,sizeof(cl_uint),&size,nullptr);
  return size;
}

static void
validOrError(cl_context                context,
             cl_mem_flags              flags,
             cl_uint                   pipe_packet_size,
             cl_uint                   pipe_max_packets,
             const cl_pipe_properties *properties,
             cl_int *                 errcode_ret)
{
  if( !xocl::config::api_checks())
    return;

  // CL_INVALID_CONTEXT if context is not a valid context.
  detail::context::validOrError(context);

  // CL_INVALID_VALUE if values specified in flags are not as defined
  // above
  detail::memory::validOrError(flags);

  // CL_INVALID_VALUE if properties is not NULL.
  if (properties)
    throw error(CL_INVALID_VALUE,"properties must be nullptr");

  // CL_INVALID_PIPE_SIZE if pipe_packet_size is 0 or the
  // pipe_packet_size exceeds CL_DEVICE_PIPE_MAX_PACKET_SIZE value
  // specified in table 4.3 (see clGetDeviceInfo) for all devices in
  // context or if pipe_max_packets is 0.
  if (!pipe_packet_size)
    //throw error(CL_INVALID_PIPE_SIZE,"pipe_packet_size must be > 0");
    throw error(CL_INVALID_VALUE,"pipe_packet_size must be > 0");
  auto dr = xocl(context)->get_device_range();
  if (std::any_of(dr.begin(),dr.end(),
       [pipe_packet_size](device* d)
       {return pipe_packet_size > getDevicePipeMaxPacketSize(d); }))
    //throw error(CL_INVALID_PIPE_SIZE,"pipe_packet_size must be <= max packet size for all devices");
    throw error(CL_INVALID_VALUE,"pipe_packet_size must be <= max packet size for all devices");

  // CL_MEM_OBJECT_ALLOCATION_FAILURE if there is a failure to
  // allocate memory for the pipe object.

  // CL_OUT_OF_RESOURCES if there is a failure to allocate resources
  // required by the OpenCL implementation on the device.

  // CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources
  // required by the OpenCL implementation on the host.
}

static cl_mem
clCreatePipe(cl_context                context,
             cl_mem_flags              flags,
             cl_uint                   pipe_packet_size,
             cl_uint                   pipe_max_packets,
             const cl_pipe_properties *properties,
             cl_int *                  errcode_ret)
{
  validOrError(context,flags,pipe_packet_size,pipe_max_packets,properties,errcode_ret);

  auto upipe = std::make_unique<xocl::pipe>(xocl::xocl(context),flags,pipe_packet_size,pipe_max_packets);

  // TODO: here we allocate a pipe even if it isn't a memory mapped pipe,
  // it would be nice to not allocate the pipe if it's a hardware pipe.
  size_t nbytes = upipe->get_pipe_packet_size() * (upipe->get_pipe_max_packets()+8);
  void* user_ptr=nullptr;
  int status = xrt_core::posix_memalign(&user_ptr, 128, (sizeof(cpu_pipe_t)+nbytes));
  if (status)
    throw xocl::error(CL_MEM_OBJECT_ALLOCATION_FAILURE);
  upipe->set_pipe_host_ptr(user_ptr);

  xocl::assign(errcode_ret,CL_SUCCESS);
  return upipe.release();
}

} // xocl

cl_mem
clCreatePipe(cl_context                context,
             cl_mem_flags              flags,
             cl_uint                   pipe_packet_size,
             cl_uint                   pipe_max_packets,
             const cl_pipe_properties *properties,
             cl_int *                  errcode_ret)
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    return xocl::clCreatePipe
      (context,flags,pipe_packet_size,pipe_max_packets,properties,errcode_ret);
  }
  catch (const xrt::error& ex) {
    xocl::send_exception_message(ex.what());
    xocl::assign(errcode_ret,ex.get_code());
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    xocl::assign(errcode_ret,CL_OUT_OF_HOST_MEMORY);
  }
  return nullptr;
}
