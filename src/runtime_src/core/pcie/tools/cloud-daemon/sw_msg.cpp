/**
 * Copyright (C) 2019 Xilinx, Inc
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

#include <cstring>

#include "core/pcie/driver/linux/include/mailbox_proto.h"
#include "sw_msg.h"

sw_msg::~sw_msg()
{
}

sw_msg::sw_msg(const void *payload, size_t len, uint64_t id, uint64_t flags)
{
    buf = std::make_unique<std::vector<char>>(sizeof(sw_chan) + len, 0);
    sw_chan *sc = reinterpret_cast<sw_chan *>(buf->data());
    sc->sz = len;
    sc->flags = flags;
    sc->id = id;
    std::memcpy(sc->data, payload, len);
}

sw_msg::sw_msg(size_t len)
{
    buf = std::make_unique<std::vector<char>>(sizeof(sw_chan) + len, 0);
    sw_chan *sc = reinterpret_cast<sw_chan *>(buf->data());
    sc->sz = len;
}

size_t sw_msg::size()
{
    return buf->size();
}

size_t sw_msg::payloadSize()
{
    sw_chan *sc = reinterpret_cast<sw_chan *>(buf->data());
    return sc->sz;
}

bool sw_msg::valid()
{
    return (sizeof(sw_chan) + payloadSize() == size());
}

char *sw_msg::data()
{
    return buf->data();
}

char *sw_msg::payloadData()
{
    sw_chan *sc = reinterpret_cast<sw_chan *>(buf->data());
    return sc->data;
}

uint64_t sw_msg::id()
{
    sw_chan *sc = reinterpret_cast<sw_chan *>(buf->data());
    return sc->id;
}
