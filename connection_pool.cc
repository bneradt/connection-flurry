/** @file
 * ConnectionPool implementation.
 *
 * Copyright 2020, Verizon Media
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "connection_pool.h"

#include "globals.h"

#include <cassert>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <strings.h>
#include <unistd.h>

#include <iostream>

ConnectionPool::ConnectionPool(unsigned int size, std::string_view host, std::string_view port)
  : _host(host)
  , _port(port)
  , _size(size)
  , _connections()
{
  for (auto i = 0u; i < size; ++i) {
    _connections.push_back(Connection());
  }

  // epoll
  _epollFd = epoll_create(size);
  _events = (epoll_event *)calloc(sizeof(epoll_event), size * 2);

  srand((unsigned int)time(nullptr));

  // setup the hints
  struct addrinfo hints;
  bzero(&hints, sizeof(hints));
  hints.ai_family = PF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  struct addrinfo *addr = nullptr;
  // look up the address
  getaddrinfo("192.168.1.12", NULL, &hints, &addr);
  _bind_addresses.push_back(addr);
  getaddrinfo("192.168.1.14", NULL, &hints, &addr);
  _bind_addresses.push_back(addr);
  getaddrinfo("192.168.1.15", NULL, &hints, &addr);
  _bind_addresses.push_back(addr);
  getaddrinfo("192.168.1.16", NULL, &hints, &addr);
  _bind_addresses.push_back(addr);
  getaddrinfo("192.168.1.17", NULL, &hints, &addr);
  _bind_addresses.push_back(addr);
}

void
ConnectionPool::connect()
{
  int count = 0;
  for (auto &connection : _connections) {
    ++count;

    if (connection._state == Connection::NOT_CONNECTED) {
      connection.socket(_host.c_str(), _port.c_str());
      connection.bind(_bind_addresses[cf::attemptedConnections % _bind_addresses.size()]);

      epoll_event event;
      event.events = EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLPRI;
      event.data.ptr = &connection;
      assert(epoll_ctl(_epollFd, EPOLL_CTL_ADD, connection._fd, &event) == 0);
      connection.connect();
    }
  }
}

void
ConnectionPool::state()
{
  std::cout << "size of the pool: " << _connections.size() << std::endl;
  if (cf::verbose) {
    std::cout << "established connections: " << _num_connections << std::endl;
  }

  int established = 0;
  int count = 0;
  time_t now = time(NULL);
  for (auto &connection : _connections) {
    ++count;
    connection._counter++;

    // get the socket tcp info
    struct tcp_info info;
    socklen_t len = sizeof(info);
    assert(getsockopt(connection._fd, connection._addr->ai_protocol, TCP_INFO, &info, &len) == 0);
    if (info.tcpi_state == TCP_ESTABLISHED) {
      ++established;
      // std::cout << "established" << std::endl;
    } else {
      // std::cout << "slot: " << count << ", state: " << (int) info.tcpi_state << " counter: " <<
      // connection._counter << " time: " << time(NULL) - connection._time << std::endl;
      if (now - connection._time > 5) {
        connection.close();
        // connect again
        if (connection._state == Connection::NOT_CONNECTED) {
          connection.socket(_host.c_str(), _port.c_str());
          connection.bind(_bind_addresses[cf::attemptedConnections % _bind_addresses.size()]);

          epoll_event event;
          event.events = EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLPRI;
          event.data.ptr = &connection;
          assert(epoll_ctl(_epollFd, EPOLL_CTL_ADD, connection._fd, &event) == 0);
          connection.connect();
        }
      }
    }
  }
  std::cout << "states checked: " << count << std::endl;
  std::cout << "established: " << established << std::endl;
}

// this is where most of the work is done
void
ConnectionPool::recycle()
{
  int num = epoll_wait(_epollFd, _events, _size * 2, 10);

  if (cf::verbose) {
    std::cout << "triggered events: " << num << std::endl;
  }
  for (int i = 0; i < num; ++i) {
    if (_events[i].events & EPOLLIN || _events[i].events & EPOLLOUT) {
      Connection *con = (Connection *)_events[i].data.ptr;

      struct tcp_info info;
      socklen_t len = sizeof(info);
      assert(getsockopt(con->_fd, con->_addr->ai_protocol, TCP_INFO, &info, &len) == 0);

      if (info.tcpi_state == TCP_ESTABLISHED) {
        if (cf::verbose)
          std::cout << "established" << std::endl;
        con->_state = Connection::CONNECTED;
        ++cf::establishedConnections;
        if (cf::establishedConnections % 1000 == 0) {
          std::cerr << ".";
        }
        write(con->_fd, "GET", 3);
        ++_num_connections;
        continue;
      } else {
        std::cerr << "X";
        ++cf::failedConnections;
      }
      // close the connection
      assert(epoll_ctl(_epollFd, EPOLL_CTL_DEL, con->_fd, NULL) == 0);
      // con->close();
      --_num_connections;

      if (cf::attemptedConnections < cf::totalConnections) {
        // connect again
        if (con->_state == Connection::NOT_CONNECTED) {
          con->socket(_host.c_str(), _port.c_str());
          con->bind(_bind_addresses[cf::attemptedConnections % _bind_addresses.size()]);

          epoll_event event;
          event.events = EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLPRI;
          event.data.ptr = con;
          assert(epoll_ctl(_epollFd, EPOLL_CTL_ADD, con->_fd, &event) == 0);
          con->connect();
        }
      }
    }
  }
}

void
ConnectionPool::close()
{
  for (auto &connection : _connections) {
    if (connection._state == Connection::CONNECTED) {
      assert(epoll_ctl(_epollFd, EPOLL_CTL_DEL, connection._fd, NULL) == 0);
      connection.close();
      --_num_connections;
    }
  }
}
