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

// Copyright 2014 Xilinx, Inc. All rights reserved.
//
// This file contains confidential and proprietary information
// of Xilinx, Inc. and is protected under U.S. and
// international copyright and other intellectual property
// laws.
//
// DISCLAIMER
// This disclaimer is not a license and does not grant any
// rights to the materials distributed herewith. Except as
// otherwise provided in a valid license issued to you by
// Xilinx, and to the maximum extent permitted by applicable
// law: (1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND
// WITH ALL FAULTS, AND XILINX HEREBY DISCLAIMS ALL WARRANTIES
// AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING
// BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, NON-
// INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE; and
// (2) Xilinx shall not be liable (whether in contract or tort,
// including negligence, or under any other theory of
// liability) for any loss or damage of any kind or nature
// related to, arising under or in connection with these
// materials, including for any direct, or any indirect,
// special, incidental, or consequential loss or damage
// (including loss of data, profits, goodwill, or any type of
// loss or damage suffered as a result of any action brought
// by a third party) even if such damage or loss was
// reasonably foreseeable or Xilinx had been advised of the
// possibility of the same.
//
// CRITICAL APPLICATIONS
// Xilinx products are not designed or intended to be fail-
// safe, or for use in any application requiring fail-safe
// performance, such as life-support or safety devices or
// systems, Class III medical devices, nuclear facilities,
// applications related to the deployment of airbags, or any
// other applications that could lead to death, personal
// injury, or severe property or environmental damage
// (individually and collectively, "Critical
// Applications"). Customer assumes the sole risk and
// liability of any use of Xilinx products in Critical
// Applications, subject only to applicable laws and
// regulations governing limitations on product liability.
//
// THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS
// PART OF THIS FILE AT ALL TIMES.

#ifndef _WINDOWS

#include "unix_socket.h"

unix_socket::unix_socket()
{
  server_started = false;
  fd = -1;
  std::string sock_id = "";
  if(getenv("USER") != NULL) {
    std::string user = getenv("USER");
    if(getenv("EMULATION_SOCKETID")) {
      sock_id = getenv("EMULATION_SOCKETID");
    } else {
      sock_id = "xcl_sock";
    }
    std::string pathname =  "/tmp/" + user;
    name = pathname + "/" + sock_id;
    systemUtil::makeSystemCall(pathname, systemUtil::systemOperation::CREATE);
  } else {
    name = "/tmp/xcl_socket";
  }
  start_server(name);
}

void unix_socket::start_server(const std::string sk_desc)
{
  int sock= -1;
  struct sockaddr_un server;

  //unlink(sk_desc);
  sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("opening stream socket");
    exit(1);
  }
  server.sun_family = AF_UNIX;
  //Coverity
  strncpy(server.sun_path, sk_desc.c_str(),STR_MAX_LEN);
  if (connect(sock, (struct sockaddr*)&server, sizeof(server)) >= 0){
    fd = sock;
    server_started = true;
    return;
  }
  unlink(server.sun_path);
  if (bind(sock, (struct sockaddr *) &server, sizeof(struct sockaddr_un))) {
    close(sock);
    perror("binding stream socket");
    exit(1);
  }
  int status = listen(sock, 5);
  (void) status; // For Coverity

  fd = accept(sock, 0, 0);
  close(sock);
  if (fd == -1){
    perror("socket acceptance failed");
    exit(1);
  } else {
    server_started = true;
  } 
  return;
}

size_t unix_socket::sk_write(const void *wbuf, size_t count)
{
  ssize_t r;
  ssize_t wlen = 0;
  const unsigned char *buf = (const unsigned char*)(wbuf);
  do {
    if ((r = write(fd, buf + wlen, count - wlen)) < 0) {
      if (errno == EINTR || errno == EAGAIN) {
        continue;
      } 
      return -1;
    }
    wlen += r;
  } while (wlen < static_cast<unsigned int>(count));
  return wlen;
}

size_t unix_socket::sk_read(void *rbuf, size_t count)
{
  ssize_t r;
  ssize_t rlen = 0;
  unsigned char *buf = (unsigned char*)(rbuf);

  do {
    if ((r = read(fd, buf + rlen, count - rlen)) < 0) {
      if (errno == EINTR || errno == EAGAIN)
        continue;
      return -1;
    }
    rlen += r;
  } while ((rlen < static_cast<unsigned int>(count)) );

  return rlen;
}

#endif


