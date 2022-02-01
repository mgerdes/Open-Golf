#include_directories(SYSTEM ../../src/3rd_party)
#include_directories(../../src)
add_library(common STATIC
    alloc.c
    base64.c
    bvh.c
    data.c
    debug_console.c
    file.c
    graphics.c
    inputs.c
    json.c
    level.c
    log.c
    level.c
    map.c
    maths.c
    script.c
    string.c
    thread.c
    vec.c)
