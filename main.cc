/** @file
 * Implementation of connection-flurry.
 *
 * Copyright 2020, Verizon Media
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "connection.h"
#include "connection_pool.h"
#include "globals.h"

#include <iostream>
#include <string>
#include <unistd.h>

#include <chrono>

//----------------------------------------------------------------------------
void
usage(const char *prog)
{
  std::cerr << "USAGE: " << prog
            << " [-c concurent_connections] [-t total_connections] [-p port] [-v] hostname"
            << std::endl;
}

//----------------------------------------------------------------------------
int
main(int argc, char **argv)
{
  // getopt
  int concurrent_connections = 1;
  std::string port = "80";
  std::string host;
  int opt;

  // get the command line arguments
  while ((opt = getopt(argc, argv, "hp:c:t:v")) != -1) {
    switch (opt) {
    case 'c':
      concurrent_connections = atoi(optarg);
      break;
    case 't':
      cf::totalConnections = atoi(optarg);
      break;
    case 'p':
      port = optarg;
      break;
    case 'v':
      cf::verbose = true;
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
  ConnectionPool pool(concurrent_connections, host, port);
  auto start = std::chrono::high_resolution_clock::now();
  pool.connect(); // establish connections to the server

  // display some information
  std::cout << "One period equals 1,000 established connections..." << std::endl;

  // loop over until we have established enough connections
  while (cf::establishedConnections < cf::totalConnections) {
    pool.recycle();
    pool.state();
  }
  // sleep(100);

  pool.close(); // close all the connections
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::high_resolution_clock::now() - start)
                      .count();

  // display summary information
  std::cout << std::endl
            << "Total time in milliseconds:              " << duration << std::endl
            << "Total number of established connections: " << cf::establishedConnections
            << std::endl
            << "Total number of attempted connections:   " << cf::attemptedConnections << std::endl
            << "Total number of failed connections:      " << cf::failedConnections << std::endl
            << "Established connections per second:      "
            << ((double)cf::establishedConnections / double((double)duration) * (double)1000)
            << std::endl;
  return 0;
}
