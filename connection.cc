/** @file
 * Connection implementation.
 *
 * Copyright 2020, Verizon Media
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "connection.h"

#include "globals.h"

#include <cassert>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <iostream>

void
Connection::close()
{
  _state = NOT_CONNECTED;
  ::close(_fd);
  _counter = 0;
  _time = 0;
}

int
Connection::bind(const addrinfo *addr)
{
  return ::bind(_fd, addr->ai_addr, addr->ai_addrlen);
}

int
Connection::socket(const char *host, const char *port)
{
  // setup the hints
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = PF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  // look up the address
  getaddrinfo(host, port, &hints, &_addr);

  // create a socket
  _fd = ::socket(_addr->ai_family, _addr->ai_socktype, _addr->ai_protocol);

  if (_fd < 0) {
    perror("error calling socket");
  }

  return _fd;
}

int
Connection::connect()
{
  _time = time(nullptr);

  // set to non blocking
  assert(fcntl(_fd, F_SETFL, O_NONBLOCK) == 0);

  // make the connection
  int rval = ::connect(_fd, _addr->ai_addr, _addr->ai_addrlen);
  ++cf::attemptedConnections;

  if (rval == 0) {
    _state = CONNECTED;
  } else {
    if (errno == EINPROGRESS) {
      _state = CONNECTING;
    } else {
      std::cout << errno << std::endl;
      perror("connecting: ");
      abort();
    }
  }

  return rval;
}
