/**
 * Copyright (C) 2016-2018 Xilinx, Inc
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

#ifndef _WINDOWS

#ifndef __XCLHOST_UNIXSOCKET__
#define __XCLHOST_UNIXSOCKET__
// Local/XRT headers
#include "em_defines.h"
#include "system_utils.h"
#include "xclhal2.h"
// c-style system headers
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
// C++ headers
#include <atomic>
#include <boost/algorithm/string.hpp>
#include <chrono>
#include <iostream>
#include <thread>

class unix_socket {
  private:
    int fd;                                     // A valid file descriptor - Client or Server
    std::string name;
    std::thread mthread;                        // Let's start socket monitor thread.
    struct pollfd mpoll_on_filedescriptor;      // Let's perform poll on Connected Client Socket only.
public:
    std::atomic<bool> server_started;           // Is Server Socket/Client Socket started?
    std::atomic<bool> m_is_socket_live;         // Is Server socket Live?
    std::atomic<bool> mNonBlocking;             // send/recv flags to set.
    void set_name(const std::string &sock_name) { name = sock_name;}
    std::string get_name() { return name;}
    unix_socket(const std::string& env = "EMULATION_SOCKETID", const std::string& sock_id="xcl_sock",double timeout_insec=300,bool fatal_error=true);
    ~unix_socket()
    {
      server_started = false;
      // Let's join the thread if spawned already.
      if ( mthread.joinable() )
        mthread.join();

      close(fd);
    }
    void start_server(double timeout_insec,bool fatal_error);
    ssize_t sk_write(const void *wbuf, size_t count);
    ssize_t sk_read(void *rbuf, size_t count);
    void monitor_socket();                    //API to shim layer that can requested to monitor the client socket fd.
    void monitor_socket_thread();             // A Thread where actual monitoring performed.

};


#endif

#endif
