mkdir -p out
mkdir -p out/emscripten
mkdir -p out/temp
cd out/emscripten
cmake ../.. -G Ninja -DCMAKE_TOOLCHAIN_FILE=../../build/emscripten/Emscripten.cmake -DEMSCRIPTEN_ROOT_PATH=/home/michael/code/emsdk/upstream/emscripten
cmake --build .
cd ../..


