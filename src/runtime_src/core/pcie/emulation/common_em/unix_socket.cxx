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

#include <fcntl.h>

#include "config.h"
#include "unix_socket.h"


unix_socket::unix_socket(const std::string& env, const std::string& sock_id, double timeout_insec, bool fatal_error)
{
  std::string socket = sock_id;
  server_started.store(false);
  fd = -1;
  char* cUser = getenv("USER");

  if(cUser && !sock_id.compare("xcl_sock")) {
    std::string user = cUser;
    char* c_sock_id = getenv(env.c_str()); 
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
  //by default, all calls should be blocking only.
  mNonBlocking = false;               
  start_server(timeout_insec,fatal_error);
  mpoll_on_filedescriptor = {fd, POLLERR, 0};   // CID-271874 This will be affective for monitor_socket_status callers only.
}

void unix_socket::start_server(double timeout_insec, bool fatal_error)
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
    // So I am a client now.
    fd = sock;
    std::cout<<"\n server socket name is\t"<<name<<"\n";
    server_started.store(true);
    m_is_socket_live.store(true);
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
  if(r <= 0 && fatal_error) {
    std::cout<<"ERROR: [SDx-EM 08-0] Failed to connect to device process"<<std::endl;
    exit(1);
  }
  if(r <=0 && !fatal_error) {
      close(sock);
      unlink(name.c_str());
      return;
  }
  // I am waiting for a client connection now.
  fd = accept(sock, 0, 0);
  close(sock);
  if (fd == -1){
    perror("socket acceptance failed");
    exit(1);
  } else {
    server_started.store(true);
    m_is_socket_live.store(true);    // server started so socket liveness present.
  }
  return;
}

//....................................................................................................//
//....................................READ ME..........................................................//
//....................................................................................................//
// The socket write/read system API's are replaced with send/recv system API
// The benifit out it, is able to provide more flags to API's
// to get the much needed feature. Here, non-blocking with timedout.
//....................................................................................................//



/************************************************************************
 * sk_write - A block/non-block send call on the client file descriptor
 *  
 ************************************************************************/
ssize_t unix_socket::sk_write(const void *wbuf, size_t count)
{
  if (false == server_started.load())
    return -1;

  ssize_t r;
  ssize_t wlen = 0;
  auto buf = reinterpret_cast<const unsigned char *>(wbuf);
  int flags = MSG_WAITALL; // wait for whole message.

  // make fd as non-blocking if and only if it is asked for.
  if (mNonBlocking)
  {
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 10000; // 10 ms
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    // Let's wait for a specific time on the file descriptor fd for a state change.
    if (select(fd + 1, &rfds, NULL, NULL, &timeout) < 0)
    {
      std::cerr << "\n failed to set select timedout for sk_write \n";
    }
    flags = MSG_DONTWAIT; // for non-blocking calls, DO NOT WAIT for more than timedout period.
  }

  do
  {
    // send() system API is a non-blocking call based on flags set.
    if ((r = send(fd, buf + wlen, count - wlen, flags)) < 0)
    {
      // I did not get data, alright Let me recall.
      if (errno == EAGAIN || errno == EWOULDBLOCK)
      {
        // Data might not be received by timedout period, so Let's try
        // If socket is not live then monitor_socket_thread will detect and while condition fails.
        continue;
      }
      // something fishy! so let's return.
      if (EBADF == errno)
        std::cerr << "\n file descriptor pointing to invalid file.\n";

      return -1;
    }
    wlen += r;
  } while ((server_started == true) && (wlen < static_cast<unsigned int>(count)));
  return wlen;
}

/************************************************************************
 * sk_read - A block/non-block recv call on the client file descriptor
 *  
 ************************************************************************/
ssize_t unix_socket::sk_read(void *rbuf, size_t count)
{
  if (false == server_started.load())
    return -1;

  ssize_t r;
  ssize_t rlen = 0;
  auto buf = reinterpret_cast<unsigned char *>(rbuf);
  int flags = MSG_WAITALL; // wait for whole message.

  // make fd as non-blocking if and only if it is asked for.
  if (mNonBlocking)
  {
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 10000; // 10ms
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    // Let's wait for a specific time on the file descriptor fd for a state change.
    if (select(fd + 1, &rfds, NULL, NULL, &timeout) < 0)
    {
      std::cerr << "\n failed to set select timedout for read socket\n";
    }
    flags = MSG_DONTWAIT; // for non-blocking calls, DO NOT WAIT for more than timedout period.
  }

  do
  {
    // recv() system API is a non-blocking call based on flags set.
    if ((r = recv(fd, buf + rlen, count - rlen, flags)) < 0)
    {
      // Alright! I did not get any data in the timedout period, so Let me retry again!!!
      if (errno == EAGAIN || errno == EWOULDBLOCK)
      {
        // Data might not be received by timedout period, so Let's try
        // If socket is not live then monitor_socket_thread will detect and while condition fails.
        continue; // OMG! data is not received, timedout? ok, Let's try
      }

      return -1; // other failures, then let me stop calling recv.
    }
    rlen += r;
  } while ((server_started == true) && (rlen < static_cast<unsigned int>(count)));

  return rlen;
}

/************************************************************************
 * monitor_socket - Public function available to shim layer if 
 *                  it wants to monitor the status.
 *                  Q2h_sock is not interested but sock is interested 
 *                  from shim class.
 *  
 ************************************************************************/
void unix_socket::monitor_socket()
{
  // Let's make non-blocking calls.
  int status = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);

  if (status == -1)
    std::cerr << "\n unable to change the socket to non-blocking call\n";

  mNonBlocking.store(true);
  mthread = std::thread([&]
                        { this->monitor_socket_thread(); });
}
/************************************************************************
 * monitor_socket_thread - A thread which monitors a valid client socket
 *                          for every 500ms in its state change such as 
 *                          for reading , HUP , ERROR.
 * 
 *  
 ************************************************************************/

void unix_socket::monitor_socket_thread() {

  using namespace std::chrono_literals;

  while (true)
  {
    if (false == server_started.load())
    {
      std::cerr << "\n socket connect is not established/broken \n";
      break;
    }

    mpoll_on_filedescriptor = {fd, POLLERR, 0};           // monitor client socket file descriptor for a state change.
    auto retval = poll(&mpoll_on_filedescriptor, 1, 500); // Let's do polling on it for utmost 500 ms
    if (retval < 0)
    {
      DEBUG_MSGS_COUT("poll is failed");
      continue;
    }
    if (retval == 0)
      continue; // Poll is timedout so can retry this.

    // The same logic can be implemented by looking at ~POLLIN or POLLRDHUP & POLLERR & POLLHUP
    if (mpoll_on_filedescriptor.revents & POLLHUP)
    {
      DEBUG_MSGS_COUT("Client socket state has changed & it is not readable anymore! So application will exit now.");
      m_is_socket_live.store(false);
      server_started.store(false); // Let's other monitoring threads exit
      break;
    }

    if (mpoll_on_filedescriptor.revents & POLLERR)
    {
      DEBUG_MSGS_COUT("Hurrah! client connection is lost.");
      m_is_socket_live.store(false);
      server_started.store(false);
      break;
    }
    std::this_thread::sleep_for(500ms); // monitor fd state change for every 500ms only as it is timedout non-blocking calls.

  } // end of while
}

#endif
