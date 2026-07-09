#!/bin/zsh
#./tools/link_exe.sh && ./run.sh

cmake -B build-full -DCMAKE_TOOLCHAIN_FILE=toolchain/vc42.cmake -DYODA_VARIANT=FULL
cmake --build build-full
./run_full.sh