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

#ifndef _HWEM_MEMORY_MANAGER_H_
#define _HWEM_MEMORY_MANAGER_H_

#include <mutex>
#include <list>
#include <cassert>
#include <algorithm>

#include "em_defines.h"
#ifdef USE_HAL2
#include "xclhal2.h"
#else
#include "xclhal.h"
#endif

namespace xclemulation
{
    class MemoryManager 
    {
        std::mutex mMemManagerMutex;
        std::list<std::pair<uint64_t, uint64_t> > mFreeBufferList;
        std::list<std::pair<uint64_t, uint64_t> > mBusyBufferList;
        const uint64_t mSize;
        const uint64_t mStart;
        const uint64_t mAlignment;
        const unsigned mCoalesceThreshold;
        uint64_t mFreeSize;

        typedef std::list<std::pair<uint64_t, uint64_t> > PairList;

    public:
        static const uint64_t mNull = 0xffffffffffffffffull;

    public:
        MemoryManager(uint64_t size, uint64_t start, unsigned alignment);
        ~MemoryManager();
        uint64_t alloc(size_t& size,unsigned int paddingFactor = 0);
        void free(uint64_t buf);
        void reset();

        const uint64_t size()     { return mSize; }
        const uint64_t start()    { return mStart; }
        const uint64_t freeSize() { return mFreeSize; }
        static bool isNullAlloc(const std::pair<uint64_t, uint64_t>& buf) { return ((buf.first == mNull) || (buf.second == mNull)); }

        std::pair<uint64_t, uint64_t>lookup(uint64_t buf);

    private:
        void coalesce();
        PairList::iterator find(uint64_t buf);
    };
}

#endif


