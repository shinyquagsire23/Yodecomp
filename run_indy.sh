#!/bin/zsh
# Run the Indy (YODA_GAME=INDY) build against DESKTOP.DAW.
# Build first: cmake -B build-indy -DCMAKE_TOOLCHAIN_FILE=toolchain/vc42.cmake -DYODA_GAME=INDY; cmake --build build-indy
cp build-indy/yoda.exe YodaIndy/yoda.exe
pushd YodaIndy
PATH="/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin:/usr/bin:/bin:/usr/sbin:/sbin" \
DYLD_LIBRARY_PATH="/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/lib64:/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/lib32on64" \
/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine --bottle General $(pwd)/yoda.exe
popd
