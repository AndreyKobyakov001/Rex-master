#!/usr/bin/env bash

set -e

# `tee /dev/tty` is so the output is sent to stdout and to the piped command
find Driver/ Graph/ Walker/ Linker/ -iname *.h -o -iname *.cpp | tee /dev/tty | xargs clang-format -i
