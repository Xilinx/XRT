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

#ifndef xrt_util_regmap_h_
#define xrt_util_regmap_h_

#include <cstddef>
#include <array>
#include <cstring>
#include <algorithm>
#include <stdexcept>

namespace xrt {

/**
 * Register map utility.
 *
 * Specify word type and max size of register map.  The actual
 * size of the register map is the max(idx) that was accessed
 * by the index operator and supposedly populated.
 *
 * The underlying array is aligned per align template param
 */
template <typename WordType, std::size_t MaxSize, std::size_t Align=alignof(std::max_align_t)>
class regmap
{
public:
  using regmap_t = std::array<WordType,MaxSize>;
  using word_type = WordType;
  using size_type = typename regmap_t::size_type;
  using reference = typename regmap_t::reference;
  using value_type = typename regmap_t::value_type;

private:
#ifdef __GNUC__
  __attribute__((aligned(Align))) regmap_t m_regmap {{0}};
#else
  // gcc supports upto 128 only with alignas
  alignas(Align) regmap_t m_regmap {{0}}; 
#endif
  std::size_t m_size {0};
public:
  /*constexpr*/ reference
  operator[] (size_type idx)
  {
    m_size = std::max(m_size,idx+1);
    return m_regmap.at(idx);
  }

  value_type
  operator[] (size_type idx) const
  {
    return m_regmap.at(idx);
  }

  bool 
  operator==(const regmap& rhs) const
  {
    if (m_size != rhs.m_size)
      return false;
    if (m_regmap != rhs.m_regmap)
      return false;
    return true;
  }

  bool
  operator!=(const regmap& rhs) const
  {
    return !(*this==rhs);
  }

  void
  push_back(word_type word)
  {
    m_regmap[m_size++] = word;
  }

  void
  append(const regmap& rhs)
  {
    for (std::size_t i=0;i<rhs.size();++i)
      push_back(rhs[i]);
  }

  void
  resize(size_type size)
  {
    if (size>MaxSize)
      throw std::runtime_error(std::to_string(size) + ">" + std::to_string(MaxSize));
    m_size=size;
  }

  std::size_t
  size() const
  {
    return m_size;
  }

  std::size_t
  bytes() const
  {
    return m_size * sizeof(WordType);
  }

  const WordType*
  data() const
  {
    return m_regmap.data();
  }
};

/**
 * Place the regmap in preallcoated storage.
 * Dangerous interface, experimental.
 */
template <typename WordType, std::size_t MaxSize>
class regmap_placed
{
public:
  using word_type = WordType;
  using size_type = std::size_t;
  using value_type = WordType;
  using reference = value_type&;

private:
  value_type* m_regmap = nullptr;
  std::size_t m_size {0};
public:
  regmap_placed(value_type* data)
    : m_regmap(data)
  {}

  regmap_placed(void* data)
    : regmap_placed(static_cast<value_type*>(data))
  {}

  /*constexpr*/ reference
  operator[] (size_type idx)
  {
    m_size = std::max(m_size,idx+1);
    return m_regmap[idx];
  }

  value_type
  operator[] (size_type idx) const
  {
    return m_regmap[idx];
  }

  bool 
  operator==(const regmap_placed& rhs) const
  {
    if (m_size != rhs.m_size)
      return false;
    if (std::memcmp(m_regmap,rhs.m_regmap,m_size)!=0)
      return false;
    return true;
  }

  bool
  operator!=(const regmap_placed& rhs) const
  {
    return !(*this==rhs);
  }

  void
  push_back(word_type word)
  {
    std::memcpy(&m_regmap[m_size++],&word,sizeof(WordType));
  }

  void
  append(const regmap_placed& rhs)
  {
    for (std::size_t i=0;i<rhs.size();++i)
      push_back(rhs[i]);
  }

  void
  resize(size_type size)
  {
    if (size>MaxSize)
      throw std::runtime_error(std::to_string(size) + ">" + std::to_string(MaxSize));
    m_size=size;
  }

  std::size_t
  size() const
  {
    return m_size;
  }

  void
  clear()
  {
    m_size = 0;
    std::memset(m_regmap,0,MaxSize*sizeof(WordType));
  }

  std::size_t
  bytes() const
  {
    return m_size * sizeof(WordType);
  }

  WordType*
  data()
  {
    return m_regmap;
  }

  const WordType*
  data() const
  {
    return m_regmap;
  }
};

} // xrt

#endif


