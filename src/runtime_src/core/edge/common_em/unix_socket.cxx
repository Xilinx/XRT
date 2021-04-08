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

unix_socket::unix_socket(bool bStart)
{
  server_started = false;
  fd = -1;
  std::string sock_id = "";
  char* cUser = getenv("USER");
  if(cUser) {
    std::string user = cUser;
    char* c_sock_id = getenv("EMULATION_SOCKETID"); 
    if(c_sock_id) {
      sock_id = c_sock_id;
    } else {
      sock_id = "xcl_sock";
    }
    std::string pathname =  "/tmp/" + user;
    name = pathname + "/" + sock_id;
    systemUtil::makeSystemCall(pathname, systemUtil::systemOperation::CREATE);
  } else {
    name = "/tmp/xcl_socket";
  }

  if (bStart) {
      start_inet_server(300, true);
  }
  else {
    start_server(name);
  }
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

  //wait for the timeout. Exit from the process if simulation process is not connected
  fd_set rfds;
  FD_ZERO(&rfds);
  FD_SET(sock,&rfds);
  struct timeval tv;
  tv.tv_sec = 300;
  tv.tv_usec = 0;
  int r = select(sock+1,&rfds, NULL, NULL, &tv);
  if(r <= 0)
  {
    std::cout<<"ERROR: [SDx-EM 08-0] Failed to connect to device process"<<std::endl;
    exit(1);
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

//start inet server
void unix_socket::start_inet_server(double timeout_insec, bool fatal_error)
{
  int sock = -1;
  int portno = 1560;// Fixed Qemu port
  socklen_t clilen;
  struct sockaddr_in server, cli_addr;
  // create a socket
  // socket(int domain, int type, int protocol)
  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("opening stream socket");
    exit(1);
  }
  // clear address structure
  bzero((char *)&server, sizeof(server));
  /* setup the host_addr structure for use in bind call */
  // server byte order
  server.sin_family = AF_INET;
  // automatically be filled with current host's IP address
  server.sin_addr.s_addr = INADDR_ANY;
  // convert short integer value for port must be converted into network byte order
  server.sin_port = htons(portno);
  // bind(int fd, struct sockaddr *local_addr, socklen_t addr_length)
  // bind() passes file descriptor, the address structure, 
  // and the length of the address structure
  // This bind() call will bind  the socket to the current IP address on port, portno
  if (bind(sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
    close(sock);
    perror("binding stream socket");
    exit(1);
  }
  //printf("server :Bind completed\n");
  // This listen() call tells the socket to listen to the incoming connections.
  // The listen() function places all incoming connection into a backlog queue
  // until accept() call accepts the connection.
  // Here, we set the maximum size for the backlog queue to 5.

  int status = listen(sock, 5);
  (void)status; // For Coverity
  //printf("server :listen completed\n");

  //wait for the timeout. Exit from the process if simulation process is not connected
  fd_set rfds;
  FD_ZERO(&rfds);
  FD_SET(sock, &rfds);
  struct timeval tv;
  tv.tv_sec = timeout_insec;
  tv.tv_usec = 0;
  int r = select(sock + 1, &rfds, NULL, NULL, &tv);
  if (r <= 0 && fatal_error)
  {
    std::cout << "ERROR: [SDx-EM 08-0] Failed to connect to device process" << std::endl;
    exit(1);
  }
  if (r <= 0 && !fatal_error) {
    close(sock);
    unlink(name.c_str());
    return;
  }

  // The accept() call actually accepts an incoming connection
  clilen = sizeof(cli_addr);
  // This accept() function will write the connecting client's address info 
  // into the the address structure and the size of that structure is clilen.
  // The accept() returns a new socket file descriptor for the accepted connection.
  // So, the original socket file descriptor can continue to be used 
  // for accepting new connections while the new socker file descriptor is used for
  // communicating with the connected client.
  fd = accept(sock, (struct sockaddr *) &cli_addr, &clilen);
  close(sock);
  if (fd == -1) {
    perror("socket acceptance failed");
    exit(1);
  }
  else {
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
