add_library(sokol STATIC
	impl.c)

if(CMAKE_SYSTEM_NAME STREQUAL iOS OR CMAKE_SYSTEM_NAME STREQUAL Darwin)
    target_compile_options(sokol PRIVATE -x objective-c)
endif()
