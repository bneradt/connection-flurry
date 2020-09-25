/** @file
 * Implementation of connection-flurry.
 *
 * Copyright 2020, Verizon Media
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <list>
#include <stdlib.h>
#include <netinet/tcp.h>
#include <vector>
#include <sys/time.h>
#include <stdio.h>
#include <chrono>

using namespace std;

static bool verbose = false;
static uint32_t establishedConnections = 0;
static uint32_t attemptedConnections = 0;
static uint32_t failedConnections = 0;
static uint32_t totalConnections = 1;
/*
  not connected > sent request
*/

#if 0
enum
{
  TCP_ESTABLISHED = 1,
  TCP_SYN_SENT,
  TCP_SYN_RECV,
  TCP_FIN_WAIT1,
  TCP_FIN_WAIT2,
  TCP_TIME_WAIT,
  TCP_CLOSE,
  TCP_CLOSE_WAIT,
  TCP_LAST_ACK,
  TCP_LISTEN,
  TCP_CLOSING   /* now a valid state */
};
#endif

/**
 * Class to keep track of the information for each socket connection
 */
class Connection
{
public:
  Connection() : _state(NOT_CONNECTED), _fd(-1), _counter(0), _time(0), _addr(NULL) { }

  int socket(const char *host, const char *port);
  int bind(const addrinfo *addr);
  int connect();
  void
  close()
  {
    _state = NOT_CONNECTED;
    ::close(_fd);
    _counter = 0;
    _time = 0;
  }

  enum State { NOT_CONNECTED, CONNECTING, CONNECTED, REQUEST_SENT };

  State _state;
  int _fd;
  int _counter;
  time_t _time;
  struct addrinfo *_addr;
};

int
Connection::bind(const addrinfo *addr)
{
  return ::bind(_fd, addr->ai_addr, addr->ai_addrlen);
}

/**
 * Create the socket
 */
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

/**
 * Connects to the server
 */
int
Connection::connect()
{
  _time = time(NULL);

  // set to non blocking
  assert(fcntl(_fd, F_SETFL, O_NONBLOCK) == 0);

  // make the connection
  int rval = ::connect(_fd, _addr->ai_addr, _addr->ai_addrlen);
  ++attemptedConnections;

  if (rval == 0) {
    _state = CONNECTED;
  } else {
    if (errno == EINPROGRESS) {
      _state = CONNECTING;
    } else {
      cout << errno << endl;
      perror("connecting: ");
      abort();
    }
  }

  return rval;
}

/**
 * A pool of connection, this will be the class we use to do the testing
 */
class ConnectionPool
{
public:
  ConnectionPool(const int size, const string host, const string port)
    : _host(host)
    , _port(port)
    , _size(size)
    , _epollFd(0)
    , _connection()
    , _events(NULL)
    , _usePoll(false)
    , _pollFd(NULL)
    , _connected(0)
  {
    for (int i = 0; i < size; ++i) {
      _connection.push_back(Connection());
    }

    // epoll
    _epollFd = epoll_create(size);
    _events = (epoll_event *)calloc(sizeof(epoll_event), size * 2);
    bzero(_events, size * sizeof(epoll_event));

    srand((unsigned int)time(NULL));

    // setup the hints
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *addr;
    // look up the address
    getaddrinfo("192.168.1.12", NULL, &hints, &addr);
    _bind_address.push_back(addr);
    getaddrinfo("192.168.1.14", NULL, &hints, &addr);
    _bind_address.push_back(addr);
    getaddrinfo("192.168.1.15", NULL, &hints, &addr);
    _bind_address.push_back(addr);
    getaddrinfo("192.168.1.16", NULL, &hints, &addr);
    _bind_address.push_back(addr);
    getaddrinfo("192.168.1.17", NULL, &hints, &addr);
    _bind_address.push_back(addr);
  }

  void
  connect()
  {
    int count = 0;
    for (vector<Connection>::iterator it = _connection.begin(); it != _connection.end(); ++it) {
      ++count;

      if (it->_state == Connection::NOT_CONNECTED) {
        it->socket(_host.c_str(), _port.c_str());
        it->bind(_bind_address[attemptedConnections % _bind_address.size()]);

        epoll_event event;
        event.events = EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLPRI;
        event.data.ptr = &(*it);
        assert(epoll_ctl(_epollFd, EPOLL_CTL_ADD, it->_fd, &event) == 0);
        it->connect();
      }
    }
  }

  void
  state()
  {
    cout << "size of the pool: " << _connection.size() << endl;
    if (verbose) {
      cout << "established connections: " << _connected << endl;
    }

    int established = 0;
    int count = 0;
    time_t now = time(NULL);
    for (vector<Connection>::iterator it = _connection.begin(); it != _connection.end(); ++it) {
      ++count;
      it->_counter++;

      // get the socket tcp info
      struct tcp_info info;
      socklen_t len = sizeof(info);
      assert(getsockopt(it->_fd, it->_addr->ai_protocol, TCP_INFO, &info, &len) == 0);
      if (info.tcpi_state == TCP_ESTABLISHED) {
        ++established;
        // cout << "established" << endl;
      } else {
        // cout << "slot: " << count << ", state: " << (int) info.tcpi_state << " counter: " <<
        // it->_counter << " time: " << time(NULL) - it->_time << endl;
        if (now - it->_time > 5) {
          it->close();
          // connect again
          if (it->_state == Connection::NOT_CONNECTED) {
            it->socket(_host.c_str(), _port.c_str());
            it->bind(_bind_address[attemptedConnections % _bind_address.size()]);

            epoll_event event;
            event.events = EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLPRI;
            event.data.ptr = &(*it);
            assert(epoll_ctl(_epollFd, EPOLL_CTL_ADD, it->_fd, &event) == 0);
            it->connect();
          }
        }
      }
    }
    cout << "states checked: " << count << endl;
    cout << "established: " << established << endl;
  }

  // this is where most of the work is done
  void
  recycle()
  {
    int num = epoll_wait(_epollFd, _events, _size * 2, 10);

    if (verbose) {
      cout << "triggered events: " << num << endl;
    }
    for (int i = 0; i < num; ++i) {
      if (_events[i].events & EPOLLIN || _events[i].events & EPOLLOUT) {
        Connection *con = (Connection *)_events[i].data.ptr;

        struct tcp_info info;
        socklen_t len = sizeof(info);
        assert(getsockopt(con->_fd, con->_addr->ai_protocol, TCP_INFO, &info, &len) == 0);

        if (info.tcpi_state == TCP_ESTABLISHED) {
          if (verbose)
            cout << "established" << endl;
          con->_state = Connection::CONNECTED;
          ++establishedConnections;
          if (establishedConnections % 1000 == 0) {
            cerr << ".";
          }
          write(con->_fd, "GET", 3);
          ++_connected;
          continue;
        } else {
          cerr << "X";
          ++failedConnections;
        }
        // close the connection
        assert(epoll_ctl(_epollFd, EPOLL_CTL_DEL, con->_fd, NULL) == 0);
        // con->close();
        --_connected;

        if (attemptedConnections < totalConnections) {
          // connect again
          if (con->_state == Connection::NOT_CONNECTED) {
            con->socket(_host.c_str(), _port.c_str());
            con->bind(_bind_address[attemptedConnections % _bind_address.size()]);

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
  close()
  {
    for (vector<Connection>::iterator it = _connection.begin(); it != _connection.end(); ++it) {
      if (it->_state == Connection::CONNECTED) {
        assert(epoll_ctl(_epollFd, EPOLL_CTL_DEL, it->_fd, NULL) == 0);
        it->close();
        --_connected;
      }
    }
  }

private:
  const string _host;
  const string _port;
  int _size;
  int _epollFd;
  vector<Connection> _connection;
  vector<addrinfo *> _bind_address;
  epoll_event *_events;
  bool _usePoll;
  struct pollfd *_pollFd;
  int _connected;

  ConnectionPool(const ConnectionPool &source);
  ConnectionPool &operator=(const ConnectionPool &source);
};

//----------------------------------------------------------------------------
void
usage(const char *prog)
{
  cerr << "USAGE: " << prog
       << " [-c concurent_connections] [-t total_connections] [-p port] [-v] hostname" << endl;
}

//----------------------------------------------------------------------------
int
main(int argc, char **argv)
{
  // getopt
  int connections = 1;
  string port = "80";
  string host;
  int opt;

  // get the command line arguments
  while ((opt = getopt(argc, argv, "hp:c:t:v")) != -1) {
    switch (opt) {
    case 'c':
      connections = atoi(optarg);
      break;
    case 't':
      totalConnections = atoi(optarg);
      break;
    case 'p':
      port = optarg;
      break;
    case 'v':
      verbose = true;
      break;
    case 'h':
    default:
      usage(argv[0]);
      exit(1);
    }
  }

  if (optind < argc) {
    host = argv[optind];
  } else {
    usage(argv[0]);
    exit(1);
  }

  // create a pool of connections
  ConnectionPool x(connections, host, port);
  auto start = std::chrono::high_resolution_clock::now();
  x.connect(); // establish connections to the server

  // display some information
  cout << "One period equals 1,000 established connections..." << endl;

  // loop over until we have established enough connections
  while (establishedConnections < totalConnections) {
    x.recycle();
    x.state();
  }
  sleep(100);

  x.close(); // close all the connections
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::high_resolution_clock::now() - start)
                      .count();

  // display summary information
  cout << endl
       << "Total time in milliseconds:              " << duration << endl
       << "Total number of established connections: " << establishedConnections << endl
       << "Total number of attempted connections:   " << attemptedConnections << endl
       << "Total number of failed connections:      " << failedConnections << endl
       << "Established connections per second:      "
       << (double)establishedConnections / double((double)duration * (double)1000) << endl;
  return 0;
}
