#!/bin/zsh
#./tools/link_exe.sh && ./run.sh

cmake -B build-sdl-indy -DYODA_GAME=INDY -DYODA_PLATFORM=SDL -DYODA_DEBUG=ON
cmake --build build-sdl-indy
./run_sdl_indy.sh