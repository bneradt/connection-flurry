/** @file
 * Connection declaration.
 *
 * Copyright 2020, Verizon Media
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#pragma once

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>


/** Class to keep track of the information for each socket connection
 */
class Connection
{
public:
  Connection() = default;

  int socket(const char *host, const char *port);
  int bind(const addrinfo *addr);
  int connect();
  void close();

  enum State { NOT_CONNECTED, CONNECTING, CONNECTED, REQUEST_SENT };

  State _state = NOT_CONNECTED;
  int _fd = -1;
  int _counter = 0;
  time_t _time = 0;
  struct addrinfo *_addr = nullptr;
};

