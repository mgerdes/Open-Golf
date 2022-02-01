add_library(xatlas STATIC
    xatlas.cpp)
target_compile_options(xatlas PRIVATE 
    -std=c++11)
