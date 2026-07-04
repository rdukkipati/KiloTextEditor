#!/bin/zsh
mkdir -p build
pushd build
clang++ -Werror -Wall -Wextra -Wpedantic -Wno-unused-function -g -O0 ../main.cpp -o Kilo
popd
