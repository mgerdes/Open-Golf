mkdir -p out
mkdir -p out/linux
mkdir -p out/temp
cd out/linux
cmake ../..
cmake --build .
cd ../..
