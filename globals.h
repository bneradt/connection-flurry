/** @file
 * connection-flurry globals.
 *
 * Copyright 2020, Verizon Media
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#pragma once

#include <cstdint>

namespace cf {

extern bool verbose;
extern uint32_t establishedConnections;
extern uint32_t attemptedConnections;
extern uint32_t failedConnections;
extern uint32_t totalConnections;

}
