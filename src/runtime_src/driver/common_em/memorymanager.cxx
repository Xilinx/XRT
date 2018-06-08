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

/**
 * Copyright (C) 2015 Xilinx, Inc
 * Author: Sonal Santan
 * XDMA HAL Driver layered on top of XDMA kernel driver
 */

#include "memorymanager.h"
#include <cassert>
#include <algorithm>

/*
 * Define GCC version macro so we can use newer C++11 features
 * if possible
 */
#define GCC_VERSION (__GNUC__ * 10000 \
                     + __GNUC_MINOR__ * 100 \
                     + __GNUC_PATCHLEVEL__)


xclemulation::MemoryManager::MemoryManager(uint64_t size, uint64_t start,
                                      unsigned alignment) : mSize(size), mStart(start), mAlignment(alignment),
                                                            mCoalesceThreshold(4), mFreeSize(0)
{
    assert(start % alignment == 0);
    mFreeBufferList.push_back(std::make_pair(mStart, mSize));
    mFreeSize = mSize;
}

xclemulation::MemoryManager::~MemoryManager()
{


}

uint64_t
xclemulation::MemoryManager::alloc(size_t& origSize, unsigned int paddingFactor)
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

    for (PairList::iterator i = mFreeBufferList.begin(), e = mFreeBufferList.end(); i != e; ++i) {
        if (i->second < size)
            continue;
        result = i->first;
        if (i->second > size) {
            // Resize the existing entry in freelist
            i->first += size;
            i->second -= size;
        }
        else {
            // remove the exact match found
            mFreeBufferList.erase(i);
        }
        mBusyBufferList.push_back(std::make_pair(result, size));
        mFreeSize -= size;
        break;
    }
    return result;
}

void
xclemulation::MemoryManager::free(uint64_t buf)
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


void
xclemulation::MemoryManager::coalesce()
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


xclemulation::MemoryManager::PairList::iterator
xclemulation::MemoryManager::find(uint64_t buf)
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

void
xclemulation::MemoryManager::reset()
{
    std::lock_guard<std::mutex> lock(mMemManagerMutex);
    mFreeBufferList.clear();
    mBusyBufferList.clear();
    mFreeBufferList.push_back(std::make_pair(mStart, mSize));
    mFreeSize = 0;
}

std::pair<uint64_t, uint64_t>
xclemulation::MemoryManager::lookup(uint64_t buf)
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


