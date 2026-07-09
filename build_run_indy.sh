#!/bin/zsh
# Build + run the Indy (YODA_GAME=INDY) build against DESKTOP.DAW.
# Add -DYODA_DEBUG=ON to the configure line to get worldgen tracing in YodaIndy/yoda_debug.log.

cmake -B build-indy -DCMAKE_TOOLCHAIN_FILE=toolchain/vc42.cmake -DYODA_GAME=INDY
cmake --build build-indy
./run_indy.sh
