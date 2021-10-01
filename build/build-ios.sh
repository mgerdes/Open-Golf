mkdir -p out
mkdir -p out/ios
mkdir -p out/temp
cd out/ios
cmake ../.. -G Xcode -DCMAKE_TOOLCHAIN_FILE=../../build/ios.toolchain.cmake -DPLATFORM=OS64
cmake --build . --config Release
cd ../..
