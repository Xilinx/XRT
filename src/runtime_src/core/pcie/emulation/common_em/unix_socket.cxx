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
#include "utility.h"

#include <unistd.h>
#include <signal.h>

#include <chrono>

unix_socket::unix_socket(const std::string& env, const std::string& sock_id, double timeout_insec, bool fatal_error)
{
  std::string socket = sock_id;
  server_started.store(false);
  simprocess_socket_live.store(false);
  mStopThread.store(false);
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
  start_server(timeout_insec,fatal_error);
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
    fd = sock;
    std::cout<<"\n server socket name is\t"<<name<<"\n";
    server_started.store(true);
    simprocess_socket_live.store(true);
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

  fd = accept(sock, 0, 0);
  close(sock);
  if (fd == -1){
    perror("socket acceptance failed");
    exit(1);
  } else {
    server_started.store(true);
    simprocess_socket_live.store(true);
  }
  return;
}

ssize_t unix_socket::sk_write(const void *wbuf, size_t count)
{
  if (not server_started || (false == simprocess_socket_live) ) {
    std::cout<< "\n unix_socket::sk_write failed, no socket connection established or failed.\n";
    return -1;
  }
  ssize_t r;
  ssize_t wlen = 0;
  auto buf = reinterpret_cast<const unsigned char*>(wbuf);
  do {
    if ((r = write(fd, buf + wlen, count - wlen)) < 0) {
      if (EBADF == errno || (false == simprocess_socket_live)){
        std::cout<< "\n file descriptor pointing to invalid file.\n";
        break;
      }
      if (errno == EINTR || errno == EAGAIN ) {
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
  if (not server_started || (false == simprocess_socket_live)) {
    std::cout<< "\n unix_socket::sk_read failed, no socket connection established or failed.\n";
    return -1;
  }
  ssize_t r;
  ssize_t rlen = 0;
  auto buf = reinterpret_cast<unsigned char*>(rbuf);

  do {
    if ((r = read(fd, buf + rlen, count - rlen)) < 0) {
      if (errno == EINTR || errno == EAGAIN || (false == simprocess_socket_live) )
        continue;
      return -1;
    }
    rlen += r;
  } while ((rlen < static_cast<unsigned int>(count)) );

  return rlen;
}

void unix_socket::monitor_socket_status() {
  mcheck_socket_status_thread = std::thread(&unix_socket::monitor_socket_status_thread,this);
}

void unix_socket::monitor_socket_status_thread() {
  using namespace std::chrono_literals;

  while( false==mStopThread ) {
    if ( false == server_started) {
      std::this_thread::sleep_for(1s);
      continue;
    }

  /*  if (std::system("pgrep xsim > /dev/null") != 0){
      std::cout<<"\n xsim is no longer running so skipping socket calls now! & Exiting the application...\n";
      simprocess_socket_live.store(false);
      kill(getpid(),SIGKILL);
     // assert(simprocess_socket_live == true);
      //exit(1);
    }
    */
    auto pid_value = cUtility::proc_find("xsim");
    if (-1 != pid_value){
      std::cout<<"\n xsim is no longer running so skipping socket calls now! & Exiting the application...\n";
      simprocess_socket_live.store(false);
      kill(getpid(),SIGKILL);
     
    }
    else{

      simprocess_socket_live.store(true);
    }
      

    std::this_thread::sleep_for(500ms);
  }
}

#endif
