#!/bin/zsh
cp $(pwd)/tmp/build-link/yoda.exe $(pwd)/YodaDemo
pushd YodaDemo
PATH="/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin:/usr/bin:/bin:/usr/sbin:/sbin" DYLD_LIBRARY_PATH="/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/lib64:/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/lib32on64" /Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine --bottle "General" $(pwd)/yoda.exe ;
popd