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

#include "unix_socket.h"

unix_socket::unix_socket(const std::string& sock_id,double timeout_insec,bool fatal_error)
{
  std::string socket = sock_id;
  server_started = false;
  fd = -1;
  char* cUser = getenv("USER");
  if(cUser && !sock_id.compare("xcl_sock")) {
    std::string user = cUser;
    char* c_sock_id = getenv("EMULATION_SOCKETID"); 
    if(c_sock_id ) {
      socket = c_sock_id;
    }
    std::string pathname = "/tmp/" + std::string(cUser);
    name = pathname + "/" + socket;
    systemUtil::makeSystemCall(pathname, systemUtil::systemOperation::CREATE);
  } else {
      if(cUser) {
        std::string pathname = "/tmp/"+ std::string(cUser);
        name = pathname + "/" + socket;
        systemUtil::makeSystemCall(pathname, systemUtil::systemOperation::CREATE);
      } else {
        name = "/tmp/" + socket;
      }
  }
  start_server(timeout_insec,fatal_error);
}

void unix_socket::start_server(double timeout_insec,bool fatal_error)
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
  strncpy(server.sun_path, name.c_str(),STR_MAX_LEN);
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

  //wait for the timeout. Exit from the process if simulation process is not connected
  fd_set rfds;
  FD_ZERO(&rfds);
  FD_SET(sock,&rfds);
  struct timeval tv;
  tv.tv_sec = timeout_insec;
  tv.tv_usec = 0;
  int r = select(sock+1,&rfds, NULL, NULL, &tv);
  if(r <= 0 && fatal_error)
  {
    std::cout<<"ERROR: [SDx-EM 08-0] Failed to connect to device process"<<std::endl;
    exit(1);
  }
  if(r <=0 && !fatal_error) {
      close(sock);
      unlink(name.c_str());
      return;
  }

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

ssize_t unix_socket::sk_write(const void *wbuf, size_t count)
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

ssize_t unix_socket::sk_read(void *rbuf, size_t count)
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
