# @file
# Makefile to build connection-flurry.
#
# Copyright 2020, Verizon Media
# SPDX-License-Identifier: Apache-2.0

CXX = g++
CXX_FLAGS = -Wextra -Wpedantic -Werror -Wall -O3 -g -std=c++17

BUILD_DIR = ./build
BIN_DIR = ./bin

SRCS = $(wildcard *.cc)
OBJS = $(SRCS:%.cc=$(BUILD_DIR)/%.o)
DEPS = $(OBJS:%.o=%.d)

BIN = connection-flurry
BUILT_BIN = $(BIN_DIR)/$(BIN)

all : $(BUILT_BIN)

# Actual target of the binary - depends on all .o files.
$(BUILT_BIN) : $(OBJS)
	mkdir -p $(@D)
	$(CXX) $(CXX_FLAGS) $^ -o $@

# Include all .d files
-include $(DEPS)

# Build target for every single object file.
# The potential dependency on header files is covered
# by calling `-include $(DEP)`.
$(BUILD_DIR)/%.o : %.cc
	mkdir -p $(@D)
	$(CXX) $(CXX_FLAGS) -MMD -c $< -o $@

.PHONY : clean
clean :
	-rm -f $(BUILT_BIN) $(OBJS) $(DEPS)
	-test -d $(BIN_DIR) && rmdir $(BIN_DIR)
	-test -d $(BUILD_DIR) && rmdir $(BUILD_DIR)

clang-format :
	clang-format --style=file -i $(SRCS)
