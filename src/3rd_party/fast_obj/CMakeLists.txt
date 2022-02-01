add_library(fast_obj STATIC
    fast_obj.c)
target_compile_definitions(fast_obj PRIVATE 
    _CRT_SECURE_NO_WARNINGS)
