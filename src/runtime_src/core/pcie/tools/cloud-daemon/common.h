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

/* Declaring interfaces to helper functions for all daemons. */

#ifndef	COMMON_H
#define	COMMON_H

#include <string>
#include "pciefunc.h"
#include "core/pcie/driver/linux/include/mailbox_proto.h"

int splitLine(std::string line, std::string& key, std::string& value);
sw_chan *allocmsg(pcieFunc& dev, size_t payloadSize);
void freemsg(sw_chan *msg);
size_t getSockMsgSize(pcieFunc& dev, int sockfd);
size_t getMailboxMsgSize(pcieFunc& dev, int mbxfd);
bool readMsg(pcieFunc& dev, int fd, sw_chan *sc);
bool sendMsg(pcieFunc& dev, int fd, sw_chan *sc);
int waitForMsg(pcieFunc& dev, int localfd, int remotefd, long interval);
int localToRemote(pcieFunc& dev, int localfd, int remotefd);
int remoteToLocal(pcieFunc& dev, int localfd, int remotefd);

#endif	// COMMON_H
