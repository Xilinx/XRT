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

#ifndef xocl_core_context_h_
#define xocl_core_context_h_

#include "xocl/core/object.h"
#include "xocl/core/refcount.h"
#include "xocl/core/range.h"
#include "xocl/core/property.h"
#include <vector>
#include <functional>

namespace xocl {

class context : public refcount, public _cl_context
{
  using property_element_type = cl_context_properties;
  using property_list_type = property_list<cl_context_properties>;
  
  // The context shares ownership of a device
  using device_vector_type = std::vector<ptr<device>>;
  using device_iterator_type = ptr_iterator<device_vector_type::iterator>;

  // The context does not share ownership of the queue.
  // However, the queue shares ownership of this context.
  using queue_vector_type = std::vector<command_queue*>;
  using queue_iterator_type = queue_vector_type::iterator;

  // The context does not share ownership of the queue.
  // However, the queue shares ownership of this context.
  using program_vector_type = std::vector<program*>;
  using program_iterator_type = program_vector_type::iterator;

public:
  using notify_action = std::function<void(char*)>;

  context(const cl_context_properties* properties
          ,size_t num_devices, const cl_device_id* devices
          ,const notify_action& notify=notify_action());
  virtual ~context();

  unsigned int
  get_uid() const
  {
    return m_uid;
  }

  const property_list_type&
  get_properties() const
  {
    return m_props;
  }

  template <typename T>
  T
  get_property_as(property_element_type key) const
  {
    return m_props.get_value_as<T>(key);
  }

  range<device_iterator_type>
  get_device_range()
  {
    return range<device_iterator_type>(m_devices.begin(),m_devices.end());
  }

  device*
  get_device_if_one() const
  {
    return m_devices.size()==1
      ? (*(m_devices.begin())).get()
      : nullptr;
  }

  size_t
  num_devices() const
  {
    return m_devices.size();
  }

  bool
  has_device(const device* d)
  {
    return std::find(m_devices.begin(),m_devices.end(),d)!=m_devices.end();
  }

  range_lock<queue_iterator_type>
  get_queue_range()
  {
    std::unique_lock<std::mutex> lock(m_queue_mutex) ;
    return range_lock<queue_iterator_type>(m_queues.begin(),m_queues.end(),std::move(lock));
  }

  void
  add_queue(command_queue* q)
  {
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    m_queues.push_back(q);
  }
  
  void
  remove_queue(command_queue* q)
  {
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    auto itr = std::find(m_queues.begin(),m_queues.end(),q);
    if (itr != m_queues.end())
      m_queues.erase(itr);
  }

  range_lock<program_iterator_type>
  get_program_range()
  {
    std::unique_lock<std::mutex> lock(m_program_mutex) ;
    return range_lock<program_iterator_type>(m_programs.begin(),m_programs.end(),std::move(lock));
  }

  void
  add_program(program* p)
  {
    std::lock_guard<std::mutex> lock(m_program_mutex);
    m_programs.push_back(p);
  }
  
  void
  remove_program(program* p)
  {
    std::lock_guard<std::mutex> lock(m_program_mutex);
    auto itr = std::find(m_programs.begin(),m_programs.end(),p);
    if (itr != m_programs.end())
      m_programs.erase(itr);
  }

  platform*
  get_platform() const;

private:
  unsigned int m_uid = 0;
  property_list_type m_props;
  notify_action m_notify;

  platform* m_platform = nullptr;

  // The context owns 
  device_vector_type m_devices;

  // The context does not share ownership of the queue.
  // However, the queue shares ownership of this context.
  queue_vector_type m_queues;
  std::mutex m_queue_mutex;

  // The context does not share ownership of the program.
  // However, the program shares ownership of this context.
  program_vector_type m_programs;
  std::mutex m_program_mutex;
};

} // xocl

#endif


