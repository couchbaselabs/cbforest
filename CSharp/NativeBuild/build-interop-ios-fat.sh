#!/bin/bash

set -e

pushd `dirname $0`/../prebuilt
OUTPUT_DIR="`pwd`"
popd

rm -f $OUTPUT_DIR/libCBForest-Interop.a

pushd $OUTPUT_DIR/../../
rm -rf build
xcodebuild -scheme "CBForest static" -configuration Release -derivedDataPath build -sdk iphoneos
xcodebuild -scheme "CBForest static" -configuration Release -derivedDataPath build -sdk iphonesimulator -destination 'platform=iOS Simulator,name=iPhone 6,OS=latest'

lipo -create -output $OUTPUT_DIR/libCBForest-Interop.a build/Build/Products/Release-iphoneos/libCBForest.a build/Build/Products/Release-iphonesimulator/libCBForest.a

rm -rf build
popd
