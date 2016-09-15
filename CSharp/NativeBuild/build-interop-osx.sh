#!/bin/bash

set -e
pushd "`dirname $0`/../prebuilt"
CURRENT_DIR=`pwd`
cd ../prebuilt
OUTPUT_DIR=`pwd`
popd

rm -f $OUTPUT_DIR/libCBForest-Interop.dylib
pushd $CURRENT_DIR/../../
rm -rf build
xcodebuild ARCHS="i386 x86_64" -scheme "CBForest-Interop" -configuration Release -derivedDataPath $OUTPUT_DIR/build clean build

mv $OUTPUT_DIR/build/Build/Products/Release/libCBForest-Interop.dylib $OUTPUT_DIR/libCBForest-Interop.dylib

rm -rf $OUTPUT_DIR/build
popd
