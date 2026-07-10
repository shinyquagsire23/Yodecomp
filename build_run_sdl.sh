#!/bin/zsh
#./tools/link_exe.sh && ./run.sh

cmake -B build-sdl -DYODA_VARIANT=FULL -DYODA_PLATFORM=SDL -DYODA_DEBUG=ON
cmake --build build-sdl
./run_sdl.sh