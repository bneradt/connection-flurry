# @file
# Makefile to build connection-flurry.
#
# Copyright 2020, Verizon Media
# SPDX-License-Identifier: Apache-2.0

src = connection-flurry.cc
binary = $(src:.cc=)

$(binary) : $(src)
	g++ -Werror -Wall -O3 -g -std=c++17 -o $@ $<

clang-format :
	clang-format --style=file -i $(src)

clean:
	rm $(binary)
