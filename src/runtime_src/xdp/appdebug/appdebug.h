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

#ifndef appdebug_h
#define appdebug_h
/**
 * This file contains the implementation of the application debug
 * It exposes a set of functions that are callable from debugger(GDB)
 * It defines data structures that provide the view of runtime data structures such as cl_event and cl_meme
 * It defines lambda functions that are attached as debug action with the cl_event
 */

#include "xocl/core/object.h"
#include "xocl/core/event.h"
#include "xocl/core/command_queue.h"
#include <utility>
#include <string>
#include <algorithm>

namespace appdebug {
class app_debug_view_base {
public:
  app_debug_view_base(bool aInValid = false, const std::string& aErrMsg = "") : m_invalid (aInValid), m_msg(aErrMsg) {}

  bool isInValid () const;
  std::string geterrmsg() const;
  void setInvalidMsg(bool aInValid, const std::string& aErrMsg) {
    m_invalid = aInValid;
    m_msg = aErrMsg;
  }

  virtual ~app_debug_view_base() {}
private:
  bool m_invalid;
  std::string m_msg;
};

template <typename T>
class app_debug_view : public app_debug_view_base {
public:
  app_debug_view (T* aData, std::function<void(void)>&& aDeleteAction, bool aInValid = false, const std::string& aErrMsg = "")
                 : app_debug_view_base(aInValid, aErrMsg), m_data(aData), m_delete_action(aDeleteAction) {}
  virtual ~app_debug_view() {
    if (m_delete_action)
       m_delete_action();
  }
  T* getdata() const;
private:
  T* m_data;
  std::function<void (void)> m_delete_action;
};

struct event_debug_view_base {
  unsigned int m_uid;
  cl_command_type m_cmd;
  const char* m_command_name;
  const char* m_status_name;
  std::string m_wait_list;
  cl_event m_event;

  event_debug_view_base (cl_event aEvent, unsigned int aUid, cl_command_type aCmd, const char* aCommandName, const char* aStatusName, std::string && aWaitList) :
        m_uid(aUid),
        m_cmd(aCmd),
        m_command_name(aCommandName),
        m_status_name(aStatusName),
        m_wait_list(aWaitList),
        m_event (aEvent) { }
  virtual ~event_debug_view_base() { }
  virtual std::string getstring(int aVerbose = 0, int aQuotes = 0);
};

struct clmem_debug_view {
  cl_mem m_mem;
  unsigned int m_uid;
  std::string m_bank;
  uint64_t m_device_addr;
  size_t m_size;
  void * m_host_addr;
  clmem_debug_view (cl_mem aMem, unsigned int aUid, const std::string& aBank, uint64_t aAddr, size_t aSize, void* hostAddr) :
              m_mem(aMem), m_uid(aUid), m_bank(aBank), m_device_addr(aAddr), m_size(aSize), m_host_addr(hostAddr) {};
  ~clmem_debug_view() {}
  std::string getstring(int aVerbose = 0, int aQuotes = 0);
};

struct kernel_debug_view {
  std::string m_kname;
  std::string m_status;
  size_t m_nworkgroups;
  uint32_t m_ncompleted;
  std::string m_args;
  kernel_debug_view (std::string aKname, std::string aStatus, size_t aNumWorkGroups, uint32_t aNumCompleted, std::string aArgs) :
             m_kname(aKname), m_status (aStatus), m_nworkgroups (aNumWorkGroups), m_ncompleted(aNumCompleted), m_args(aArgs) {};
  ~kernel_debug_view() {}
  std::string getstring(int aVerbose = 0, int aQuotes = 0);
};

struct event_debug_view_readwrite : public event_debug_view_base {
  cl_mem m_buffer;
  size_t m_offset;
  size_t m_size;
  const void* m_host_ptr;
  event_debug_view_readwrite (cl_event aEvent, unsigned int aUid, cl_command_type aCmd, const char* aCommandName, const char* aStatusName,
                      std::string && aWaitList, cl_mem aBuffer, size_t aOffset, size_t aSize, const void *aPtr) :
        event_debug_view_base(aEvent, aUid, aCmd, aCommandName, aStatusName, std::move(aWaitList)),
        m_buffer(aBuffer),
        m_offset(aOffset),
        m_size(aSize),
        m_host_ptr(aPtr) { }
  virtual ~event_debug_view_readwrite() { }
  virtual std::string getstring(int aVerbose = 0, int aQuotes = 0);
};


struct event_debug_view_copy : public event_debug_view_base {
  cl_mem m_src_buffer;
  size_t m_src_offset;
  cl_mem m_dst_buffer;
  size_t m_dst_offset;
  size_t m_size;
  event_debug_view_copy (cl_event aEvent, unsigned int aUid, cl_command_type aCmd, const char* aCommandName, const char* aStatusName,
                       std::string && aWaitList,cl_mem aSrcBuffer, size_t aSrcOffset, cl_mem aDstBuffer, size_t aDstOffset, size_t aSize) :
        event_debug_view_base(aEvent, aUid, aCmd, aCommandName, aStatusName, std::move(aWaitList)),
        m_src_buffer(aSrcBuffer),
        m_src_offset(aSrcOffset),
        m_dst_buffer(aDstBuffer),
        m_dst_offset(aDstOffset),
        m_size(aSize) { }
  virtual ~event_debug_view_copy() { }
  virtual std::string getstring(int aVerbose = 0, int aQuotes = 0);
};

struct event_debug_view_fill : public event_debug_view_base {
  cl_mem m_buffer;
  size_t m_offset;
  const void* m_pattern;
  size_t m_pattern_size;
  size_t m_size;
  event_debug_view_fill (cl_event aEvent, unsigned int aUid, cl_command_type aCmd, const char* aCommandName, const char* aStatusName,
                      std::string && aWaitList, cl_mem aBuffer, size_t aOffset, const void* aPattern, size_t aPatternSize, size_t aSize) :
        event_debug_view_base(aEvent, aUid, aCmd, aCommandName, aStatusName, std::move(aWaitList)),
        m_buffer(aBuffer),
        m_offset(aOffset),
        m_pattern(aPattern),
        m_pattern_size(aPatternSize),
        m_size(aSize) { }
  virtual ~event_debug_view_fill() { }
  virtual std::string getstring(int aVerbose = 0, int aQuotes = 0);
};

struct event_debug_view_map : public event_debug_view_base {
  cl_mem m_buffer;
  cl_mem_flags m_flags;
  event_debug_view_map (cl_event aEvent, unsigned int aUid, cl_command_type aCmd, const char* aCommandName,
                                const char* aStatusName, std::string && aWaitList, cl_mem aBuffer, size_t aFlags) :
        event_debug_view_base(aEvent, aUid, aCmd, aCommandName, aStatusName, std::move(aWaitList)),
        m_buffer(aBuffer),
        m_flags(aFlags) { }
  virtual ~event_debug_view_map() { }
  virtual std::string getstring(int aVerbose = 0, int aQuotes = 0);
};

struct event_debug_view_migrate : public event_debug_view_base {
  std::vector<cl_mem> m_mem_objects;
  cl_uint m_num_objects;
  bool m_kernel_args_migrate;
  cl_mem_migration_flags m_flags;
  std::string m_kname;

  event_debug_view_migrate (cl_event aEvent, unsigned int aUid, cl_command_type aCmd, const char* aCommandName, const char* aStatusName,
            std::string && aWaitList, const cl_mem *aBufferObjs, cl_uint aNumObjs, cl_mem_migration_flags aFlags) :
        event_debug_view_base(aEvent, aUid, aCmd, aCommandName, aStatusName, std::move(aWaitList)),
        m_mem_objects(aBufferObjs, aBufferObjs+aNumObjs),
        m_num_objects(aNumObjs),
        m_kernel_args_migrate(false),
        m_flags(aFlags) { }

  event_debug_view_migrate (cl_event aEvent, unsigned int aUid, cl_command_type aCmd, const char* aCommandName,
                                const char* aStatusName, std::string && aWaitList, std::string aKname) :
        event_debug_view_base(aEvent, aUid, aCmd, aCommandName, aStatusName, std::move(aWaitList)),
        m_num_objects (0),
        m_kernel_args_migrate(true),
        m_flags (0),
        m_kname (aKname) { }
  virtual ~event_debug_view_migrate() { }
  virtual std::string getstring(int aVerbose = 0, int aQuotes = 0);
};

struct event_debug_view_ndrange : public event_debug_view_base {
  std::string m_kname;
  size_t m_nworkgroups;
  uint32_t m_ncompleted;
  bool m_submitted;
  event_debug_view_ndrange (cl_event aEvent, unsigned int aUid, cl_command_type aCmd, const char* aCommandName, const char* aStatusName,
            std::string && aWaitList, std::string aKName, size_t aNumWorkGroups, uint32_t aNumCompleted, bool aSubmitted) :
        event_debug_view_base(aEvent, aUid, aCmd, aCommandName, aStatusName, std::move(aWaitList)),
        m_kname(aKName),
        m_nworkgroups(aNumWorkGroups),
        m_ncompleted(aNumCompleted),
        m_submitted(aSubmitted) { }
  virtual ~event_debug_view_ndrange() { }
  virtual std::string getstring(int aVerbose = 0, int aQuotes = 0);
};

struct event_debug_view_unmap : public event_debug_view_base {
  cl_mem m_buffer;

  event_debug_view_unmap (cl_event aEvent, unsigned int aUid, cl_command_type aCmd, const char* aCommandName, const char* aStatusName, std::string && aWaitList, cl_mem aBuffer) :
        event_debug_view_base(aEvent, aUid, aCmd, aCommandName, aStatusName, std::move(aWaitList)),
        m_buffer(aBuffer) { }
  virtual ~event_debug_view_unmap() { }
  virtual std::string getstring(int aVerbose = 0, int aQuotes = 0);
};

struct event_debug_view_barrier_marker : public event_debug_view_base {
  std::string m_event_wait_list;

  event_debug_view_barrier_marker (cl_event aEvent, unsigned int aUid, cl_command_type aCmd, const char* aCommandName, const char* aStatusName, std::string && aWaitList) :
        event_debug_view_base(aEvent, aUid, aCmd, aCommandName, aStatusName, std::move(aWaitList))
        /*m_event_wait_list(event_wait_list)*/ { }

  virtual ~event_debug_view_barrier_marker() { }
  virtual std::string getstring(int aVerbose = 0, int aQuotes = 0);
};

struct event_debug_view_readwrite_image : public event_debug_view_base {
  cl_mem m_image;
  size_t m_row_pitch;
  size_t m_slice_pitch;
  const void* m_host_ptr;
  size_t m_origin[3];
  size_t m_region[3];
  event_debug_view_readwrite_image (cl_event aEvent, unsigned int aUid, cl_command_type aCmd, const char* aCommandName, const char* aStatusName,
                      std::string && aWaitList, cl_mem aImage,
                      std::vector<size_t>&& aOrigin, std::vector<size_t>&& aRegion,
                      size_t aRowPitch, size_t aSlicePitch, const void *aPtr) :
        event_debug_view_base(aEvent, aUid, aCmd, aCommandName, aStatusName, std::move(aWaitList)),
        m_image(aImage),
        m_row_pitch(aRowPitch),
        m_slice_pitch(aSlicePitch),
        m_host_ptr(aPtr) {
          std::copy(aOrigin.begin(), aOrigin.end(), m_origin);
          std::copy(aRegion.begin(), aRegion.end(), m_region);
        }
  virtual ~event_debug_view_readwrite_image() { }
  virtual std::string getstring(int aVerbose = 0, int aQuotes = 0);
};


//using action_debug_type = std::function<appdebug::event_debug_view_base* (xocl::event*)>;
using action_debug_type = xocl::event::action_debug_type;

action_debug_type
action_readwrite(cl_mem buffer,size_t offset, size_t size, const void* ptr);

action_debug_type
action_copybuf(cl_mem src_buffer, cl_mem dst_buffer, size_t src_offset, size_t dst_offset, size_t size);

action_debug_type
action_fill_buffer(cl_mem buffer, const void* pattern, size_t pattern_size, size_t offset, size_t size);

action_debug_type
action_map(cl_mem buffer,cl_map_flags map_flags);

action_debug_type
action_migrate(cl_uint num_mem_objects, const cl_mem *mem_objects, cl_mem_migration_flags flags);

action_debug_type
action_ndrange_migrate(cl_event event, cl_kernel kernel);

action_debug_type
action_ndrange(cl_event event, cl_kernel kernel);

action_debug_type
action_unmap(cl_mem buffer);

action_debug_type
action_barrier_marker(int num_events_in_wait_list, const cl_event* event_wait_list);

action_debug_type
action_readwrite_image(cl_mem image,const size_t* origin,const size_t* region,
		                         size_t row_pitch,size_t slice_pitch,const void* ptr);


template <typename F, typename ...Args>
void
set_event_action(xocl::event* event, F&& f, Args&&... args)
{
  //Save on effort creating lambda if debug not enabled
  if (xrt::config::get_app_debug()) {
    event->set_debug_action(f(std::forward<Args>(args)...));
  }
}
//Debug functions
app_debug_view<std::pair<size_t,size_t>>*
clPrintCmdQOccupancy(cl_command_queue cq);

void
clFreeCmdQueueInfo(std::pair<size_t, size_t> *size_pair);

app_debug_view <std::vector<event_debug_view_base*> >*
clPrintCmdQQueued(cl_command_queue cq);

app_debug_view <std::vector<event_debug_view_base*> >*
clPrintCmdQSubmitted(cl_command_queue cq);

app_debug_view<std::vector<cl_command_queue> >*
clGetCmdQueues();

app_debug_view<std::vector<cl_mem> >*
clGetClMems();

bool
isAppdebugEnabled();

void
clFreeAppDebugView(app_debug_view_base* aView);

app_debug_view <clmem_debug_view>*
clGetMemInfo(cl_mem aMem);

app_debug_view <std::vector<kernel_debug_view*>>*
clGetKernelInfo();

app_debug_view<event_debug_view_base>*
clGetEventInfo(cl_event aEvent);

//spm_debug_view requires xcl_app_debug.h, so don't include here
//forward declare needed type
struct spm_debug_view;
app_debug_view<spm_debug_view>*
clGetDebugCounters();

struct lapc_debug_view;
app_debug_view<lapc_debug_view>*
clGetDebugCheckers();

} // appdebug
#endif


