/** @file
 * ConnectionPool declaration.
 *
 * Copyright 2020, Verizon Media
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#pragma once

#include "connection.h"

#include <netdb.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <string>
#include <string_view>
#include <vector>


/**
 * A pool of connection, this will be the class we use to do the testing
 */
class ConnectionPool
{
public:
  ConnectionPool(unsigned int size, std::string_view host, std::string_view port);
  ConnectionPool(const ConnectionPool &source) = delete;
  ConnectionPool &operator=(const ConnectionPool &source) = delete;

  void connect();
  void state();

  // this is where most of the work is done
  void recycle();

  void close();

private:
  const std::string _server_name;
  const std::string _server_port;
  int _epollFd = 0;
  std::vector<Connection> _connections;
  std::vector<addrinfo *> _local_addresses;
  epoll_event *_events = nullptr;
  bool _usePoll = false;
  struct pollfd *_pollFd = nullptr;
  int _num_connections = 0;

};
