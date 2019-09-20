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

/* Helper class representing SW channel msg buffer. */

#ifndef SW_MSG_H
#define SW_MSG_H

#include <vector>
#include <memory>

class sw_msg {
public:
    // Init a buffer ready to be sent out.
    sw_msg(const void *payload, size_t len, uint64_t id, uint64_t flags);
    // Init a buffer ready to receive data.
    sw_msg(size_t len);
    ~sw_msg();

    size_t size();
    char *data();
    bool valid();

    size_t payloadSize();
    char *payloadData();
    uint64_t id();

private:
    std::unique_ptr<std::vector<char>> buf;
};

#endif // SW_MSG_H
