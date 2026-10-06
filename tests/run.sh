#!/usr/bin/env bash
# Builds and runs the council parser tests on a computer. Run from the repo
# root. Arguments are passed through (e.g. --live page.html).
set -euo pipefail
mkdir -p tests/build
c++ -std=c++17 -Wall -Wextra -Isrc tests/council_parser_test.cpp src/council_parser.cpp \
  -o tests/build/council_parser_test
tests/build/council_parser_test "$@"
