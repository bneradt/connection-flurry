/** @file
 * connection-flurry globals.
 *
 * Copyright 2020, Verizon Media
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include "globals.h"

namespace cf
{
bool verbose = false;
uint32_t establishedConnections = 0;
uint32_t attemptedConnections = 0;
uint32_t failedConnections = 0;
uint32_t totalConnections = 1;

} // namespace cf
