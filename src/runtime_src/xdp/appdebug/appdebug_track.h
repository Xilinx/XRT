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

#ifndef appdebug_track_h
#define appdebug_track_h
/**
 * This file contains the implementation of the application debug
 * It declares data structures used to track the ocl objects
 */

#include "xocl/core/object.h"
#include "xocl/core/command_queue.h"
#include "xocl/core/execution_context.h"
#include "core/include/xrt/xrt_kernel.h"
#include <utility>
#include <string>
#include <set>
#include <map>
#include <mutex>

namespace appdebug {
void cb_scheduler_cmd_start (const xocl::execution_context*, const xrt::run&);
void cb_scheduler_cmd_done (const xocl::execution_context*, const xrt::run&);

template <typename T>
class app_debug_track {
public:
  ~app_debug_track() {
    m_set = false;
  }
  static app_debug_track* getInstance() {
    static app_debug_track singleton;
    return &singleton;
  }

  //Runtime calls these functions from constructor and destructor of opencl objects
  //these can suspend to get access to the data structure
  void add_object (T aObj) {
    if (m_set) {
      std::lock_guard<std::mutex> lk (m_mutex);
      m_objs.insert(aObj);
    }
  }
  void remove_object (T aObj) {
    if (m_set) {
      std::lock_guard<std::mutex> lk (m_mutex);
      m_objs.erase(aObj);
    }
  }

  //Following 2 function called during debug by user, this should never suspend
  void validate_object (T aObj) {
    if (m_set) {
      std::unique_lock<std::mutex> lk(m_mutex, std::defer_lock);
      if (!lk.try_lock())
        throw xocl::error(DBG_EXCEPT_LOCK_FAILED, "Failed to secure lock on data structure");
      if (m_objs.find(aObj) == m_objs.end() )
        throw xocl::error(DBG_EXCEPT_INVALID_OBJECT, "Unknown OpenCL object");
    }
    else {
      throw xocl::error(DBG_EXCEPT_INVALID_OBJECT, "Invalid object tracker");
    }
  }

  void for_each(std::function<void(T aObj)>&& fn) {
    if (m_set) {
      std::unique_lock<std::mutex> lk(m_mutex, std::defer_lock);
      if (!lk.try_lock())
        throw xocl::error(DBG_EXCEPT_LOCK_FAILED, "Failed to secure lock on data structure");
      std::for_each(m_objs.begin(), m_objs.end(), fn);
    }
    else {
      throw xocl::error(DBG_EXCEPT_INVALID_OBJECT, "Invalid object tracker");
    }
  }
  //When the program exits, the static singleton object could get deleted
  //before all the objects using it are released. So m_set is used to
  //disallow access to the data structure after the object is deleted
  static bool m_set;
private:
  std::set <T> m_objs;
  std::mutex m_mutex;
};

template <>
class app_debug_track <cl_event> {
public:
  struct event_data_t {
    bool m_start;
    uint32_t m_ncomplete;
  };
  static app_debug_track* getInstance() {
    static app_debug_track singleton;
    return &singleton;
  }
  app_debug_track () {
    //when appdebug tracker is created, install scheduler command start and end callbacks
    xocl::add_command_start_callback(appdebug::cb_scheduler_cmd_start);
    xocl::add_command_done_callback(appdebug::cb_scheduler_cmd_done);
  }
  ~app_debug_track() {
    m_set = false;
  }
  //Runtime calls these functions from constructor and destructor of opencl objects
  //these can suspend to get access to the data structure
  void add_object (cl_event aObj) {
    if (m_set) {
      std::lock_guard<std::mutex> lk (m_mutex);
      m_objs.insert(std::pair<cl_event, event_data_t>(aObj, event_data_t()));
    }
  }
  void remove_object (cl_event aObj) {
    if (m_set) {
      std::lock_guard<std::mutex> lk (m_mutex);
      m_objs.erase(aObj);
    }
  }

  //Suspends on lock, returns a reference and throws exception in cases that cannot be handled
  event_data_t& get_data (const cl_event aObj) {
    if (!m_set)
      throw xocl::error(DBG_EXCEPT_INVALID_OBJECT, "Appdebug singleton is deleted");

    std::lock_guard<std::mutex> lk (m_mutex);
    if (m_objs.find(aObj) == m_objs.end() )
      throw xocl::error(DBG_EXCEPT_INVALID_OBJECT, "Unknown OpenCL object");
    return m_objs[aObj];
  }

  //Following 2 function called during debug by user, this should never suspend
  void validate_object (cl_event aObj) {
    if (m_set) {
      std::unique_lock<std::mutex> lk(m_mutex, std::defer_lock);
      if (!lk.try_lock())
        throw xocl::error(DBG_EXCEPT_LOCK_FAILED, "Failed to secure lock on data structure");
      if (m_objs.find(aObj) == m_objs.end() )
        throw xocl::error(DBG_EXCEPT_INVALID_OBJECT, "Unknown OpenCL object");
    }
    else {
      throw xocl::error(DBG_EXCEPT_INVALID_OBJECT, "Invalid object tracker");
    }
  }

  void for_each(std::function<void(cl_event aObj)>&& fn) {
    if (m_set) {
      std::unique_lock<std::mutex> lk(m_mutex, std::defer_lock);
      if (!lk.try_lock())
        throw xocl::error(DBG_EXCEPT_LOCK_FAILED, "Failed to secure lock on data structure");
      //std::for_each(m_objs.begin(), m_objs.end(), fn);
      for (auto it = m_objs.begin(); it!=m_objs.end(); ++it) {
        fn(it->first);
      }
    }
    else {
      throw xocl::error(DBG_EXCEPT_INVALID_OBJECT, "Invalid object tracker");
    }
  }

  event_data_t& try_get_data (const cl_event aObj) {
    if (!m_set)
      throw xocl::error(DBG_EXCEPT_INVALID_OBJECT, "Appdebug singleton is deleted");

    std::unique_lock<std::mutex> lk(m_mutex, std::defer_lock);
    if (!lk.try_lock())
      throw xocl::error(DBG_EXCEPT_LOCK_FAILED, "Failed to secure lock on data structure");
    if (m_objs.find(aObj) == m_objs.end() )
      throw xocl::error(DBG_EXCEPT_INVALID_OBJECT, "Unknown OpenCL object");
    return m_objs[aObj];
  }

  //When the program exits, the static singleton object could get deleted
  //before all the objects using it are released. So m_set is used to
  //disallow access to the data structure after the object is deleted
  static bool m_set;
private:
  std::map<cl_event, event_data_t> m_objs;
  std::mutex m_mutex;
};
////////////////////////Command queue////////////////////
inline
void add_command_queue (xocl::command_queue* cq) {
  if (xrt_xocl::config::get_app_debug()) {
    //std::cout << "Adding queue xocl: " << std::hex << cq << " cl: " << (static_cast<cl_command_queue>(cq)) << std::endl;
    app_debug_track<cl_command_queue>::getInstance()->add_object(static_cast<cl_command_queue>(cq));
  }
}
inline
void remove_command_queue (xocl::command_queue* cq) {
  if (xrt_xocl::config::get_app_debug()) {
    //std::cout << "Removing queue xocl: " << std::hex << cq << " cl: " << (static_cast<cl_command_queue>(cq)) << std::endl;
    app_debug_track<cl_command_queue>::getInstance()->remove_object(static_cast<cl_command_queue>(cq));
  }
}
inline
void validate_command_queue (cl_command_queue cq) {
  if (xrt_xocl::config::get_app_debug()) {
    //std::cout << "validating queue cl: " << cq << std::endl;
    app_debug_track<cl_command_queue>::getInstance()->validate_object(cq);
  }
  else {
    throw xocl::error(DBG_EXCEPT_DBG_DISABLED, "Application debug not enabled");
  }
  return;
}

//////////////////////// Event ////////////////////
inline
void add_event (xocl::event* aEv) {
  if (xrt_xocl::config::get_app_debug()) {
    cl_event clEv = aEv;
    //std::cout << "Adding event xocl " << std::hex << aEv << " cl " << clEv << std::endl;
    //app_debug_track<cl_event>::getInstance()->add_object(static_cast<cl_event>(aEv));
    app_debug_track<cl_event>::getInstance()->add_object(clEv);
  }
}
inline
void remove_event (xocl::event* aEv) {
  if (xrt_xocl::config::get_app_debug()) {
    cl_event clEv = aEv;
    //std::cout << "Removing event: " << std::hex << aEv << std::endl;
    //std::cout << "Removing event xocl " << std::hex << aEv << " cl " << clEv << std::endl;
    //app_debug_track<cl_event>::getInstance()->remove_object(static_cast<cl_event>(aEv));
    app_debug_track<cl_event>::getInstance()->remove_object(clEv);
  }
}
inline
void validate_event (cl_event aEv) {
  if (xrt_xocl::config::get_app_debug()) {
    //std::cout << "Validating event cl " << std::hex << aEv  << std::endl;
    app_debug_track<cl_event>::getInstance()->validate_object(aEv);
  }
  else {
    throw xocl::error(DBG_EXCEPT_DBG_DISABLED, "Application debug not enabled");
  }
  return;
}

//////////////////////// cl_mem ////////////////////
inline
void add_clmem (cl_mem aMem) {
  if (xrt_xocl::config::get_app_debug()) {
    app_debug_track<cl_mem>::getInstance()->add_object(aMem);
  }
}
inline
void remove_clmem (cl_mem aMem) {
  if (xrt_xocl::config::get_app_debug()) {
    app_debug_track<cl_mem>::getInstance()->remove_object(aMem);
  }
}
inline
void validate_clmem (cl_mem aMem) {
  if (xrt_xocl::config::get_app_debug()) {
    app_debug_track<cl_mem>::getInstance()->validate_object(aMem);
  }
  else {
    throw xocl::error(DBG_EXCEPT_DBG_DISABLED, "Application debug not enabled");
  }
}
} // appdebug
#endif


