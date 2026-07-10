#!/bin/zsh
# Run the FULL (YODA_VARIANT=FULL) build against the retail YODESK.DTA.
# Build first: cmake --build build-full   (configure: -DYODA_VARIANT=FULL)
cp build-sdl-indy/yoda YodaIndy/yoda
pushd YodaIndy
./yoda
popd
