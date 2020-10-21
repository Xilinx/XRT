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

#include "memorymanager.h"

namespace xclemulation {
  MemoryManager::MemoryManager(uint64_t size, uint64_t start,
      unsigned alignment,std::string& tag ) : mSize(size), mStart(start), mAlignment(alignment), mTag(tag),
  mCoalesceThreshold(4), mFreeSize(0)
  {
    assert(start % alignment == 0);
    mFreeBufferList.push_back(std::make_pair(mStart, mSize));
    mFreeSize = mSize;
  }

  MemoryManager::~MemoryManager()
  {

  }

  uint64_t MemoryManager::alloc(size_t& origSize, unsigned int paddingFactor,std::map<uint64_t, uint64_t> &chunks )
  {
    if (origSize == 0)
      origSize = mAlignment;

    uint64_t result = mNull;
    const size_t mod_size = origSize % mAlignment;
    const size_t pad = (mod_size > 0) ? (mAlignment - mod_size) : 0;
    origSize += pad;
    size_t size = origSize;
    size = size +(2*paddingFactor*size);

    std::lock_guard<std::mutex> lock(mMemManagerMutex);
    if(mChildMemories.size())
    {
	    size_t remainingSize = size;
	    for (auto it:mChildMemories)
	    {
		    size_t sizeToBeAllocated = (it->freeSize() <= remainingSize) ? it->freeSize() : remainingSize;
		    uint64_t result_final = it->alloc(sizeToBeAllocated);

		    if(result_final != mNull)
			    chunks[result_final] = sizeToBeAllocated;

		    if(result == mNull)
			    result = result_final;

		    remainingSize = remainingSize - sizeToBeAllocated;
		    if(remainingSize == 0)
		    {
			    return result;
		    }
	    }
    }

    for (PairList::iterator i = mFreeBufferList.begin(), e = mFreeBufferList.end(); i != e; ++i) 
    {
      if (i->second < size)
        continue;
      result = i->first;
      if (i->second > size) 
      {
        // Resize the existing entry in freelist
        i->first += size;
        i->second -= size;
      }
      else 
      {
        // remove the exact match found
        mFreeBufferList.erase(i);
      }
      mBusyBufferList.push_back(std::make_pair(result, size));
      mFreeSize -= size;
      break;
    }
    return result;
  }

  void MemoryManager::free(uint64_t buf)
  {
    std::lock_guard<std::mutex> lock(mMemManagerMutex);
    PairList::iterator i = find(buf);
    if (i == mBusyBufferList.end())
      return;
    mFreeSize += i->second;
    mFreeBufferList.push_back(std::make_pair(i->first, i->second));
    mBusyBufferList.erase(i);
    if (mFreeBufferList.size() > mCoalesceThreshold) {
      coalesce();
    }
  }


  void MemoryManager::coalesce()
  {
    // First sort the free buffers and then attempt to coalesce the neighbors
    mFreeBufferList.sort();

    PairList::iterator curr = mFreeBufferList.begin();
    PairList::iterator next = curr;
    ++next;
    PairList::iterator last = mFreeBufferList.end();
    while (next != last) {
      if ((curr->first + curr->second) != next->first) {
        // Non contiguous blocks
        curr = next;
        ++next;
        continue;
      }
      // Coalesce curr and next
      curr->second += next->second;
      mFreeBufferList.erase(next);
      next = curr;
      ++next;
    }
  }


  MemoryManager::PairList::iterator MemoryManager::find(uint64_t buf)
  {
#if GCC_VERSION >= 40800
    PairList::iterator i = std::find_if(mBusyBufferList.begin(), mBusyBufferList.end(), [&] (const PairList::value_type& s)
        { return s.first == buf; });
#else
    PairList::iterator i = mBusyBufferList.begin();
    PairList::iterator last  = mBusyBufferList.end();
    while(i != last) {
      if (i->first == buf)
        break;
      ++i;
    }
#endif
    return i;
  }

  void MemoryManager::reset()
  {
    std::lock_guard<std::mutex> lock(mMemManagerMutex);
    mFreeBufferList.clear();
    mBusyBufferList.clear();
    mFreeBufferList.push_back(std::make_pair(mStart, mSize));
    mFreeSize = 0;
  }

  std::pair<uint64_t, uint64_t> MemoryManager::lookup(uint64_t buf)
  {
    std::lock_guard<std::mutex> lock(mMemManagerMutex);
    PairList::iterator i = find(buf);
    if (i != mBusyBufferList.end())
      return *i;
    // Compiler bug -- Some versions of GCC C++11 compiler do not
    // like mNull directly inside std::make_pair, so capture mNull
    // in a temporary 
    const uint64_t v = mNull;
    return std::make_pair(v, v);
  }
}

