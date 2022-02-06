add_library(sokol STATIC
	impl.c)

if(CMAKE_SYSTEM_NAME STREQUAL iOS)
    target_compile_options(sokol PRIVATE -x objective-c)
endif()
