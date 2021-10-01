mkdir -p out
mkdir -p out/osx
mkdir -p out/temp
cd out/osx
cmake ../..
cmake --build .
cd ../..
