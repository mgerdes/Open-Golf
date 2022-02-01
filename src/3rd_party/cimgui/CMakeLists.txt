add_subdirectory(imgui)

add_library(cimgui STATIC
    cimgui.cpp)
target_compile_options(cimgui PRIVATE 
    -std=c++11)
target_link_libraries(cimgui PRIVATE 
	imgui)
