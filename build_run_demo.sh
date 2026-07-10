#!/bin/zsh
#./tools/link_exe.sh && ./run.sh

cmake -B build-demo -DCMAKE_TOOLCHAIN_FILE=toolchain/vc42.cmake -DYODA_VARIANT=DEMO
cmake --build build-demo
./run_demo.sh