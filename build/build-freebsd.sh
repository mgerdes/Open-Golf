mkdir -p out
mkdir -p out/freebsd
mkdir -p out/temp
cd out/freebsd
cmake ../..
cmake --build .
cd ../..
