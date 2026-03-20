#!/bin/bash
cd /Users/tanner.davison/projects/cpp/sdl-sandbox
c++ -std=c++17 -o pngdims pngdims.cpp 2>&1 && ./pngdims \
  game_assets/backgrounds/*.png \
  2>&1

