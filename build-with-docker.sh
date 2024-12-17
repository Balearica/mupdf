#!/bin/bash

set -euo pipefail

EM_VERSION=3.1.60

# docker pull emscripten/emsdk:$EM_VERSION
docker run \
  -v $PWD:/src \
  -v $PWD/build/wasm/cache:/emsdk/upstream/emscripten/cache \
  emscripten/emsdk:$EM_VERSION \
  sh -c 'bash ./build.sh'