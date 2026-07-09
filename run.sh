#!/bin/zsh
cp $(pwd)/tmp/build-link/yoda.exe $(pwd)/YodaDemo
pushd YodaDemo
# NOTE: do NOT pre-warm a persistent wineserver here — a `wineserver -p` started in a headless
# context leaves the bottle with a windowless server that later GUI runs inherit (no window).
# wineserver persistence lives only in the build wrappers (toolchain/bin/{cl,link}, ~/.wine).
PATH="/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin:/usr/bin:/bin:/usr/sbin:/sbin" DYLD_LIBRARY_PATH="/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/lib64:/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/lib32on64" /Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine --bottle "General" $(pwd)/yoda.exe ;
popd