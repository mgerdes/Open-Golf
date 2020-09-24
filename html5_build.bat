if "%1" == "libs" (
    emcc libraries\array.c libraries\cimgui.cpp libraries\glad.c libraries\map.c libraries\miniz.c libraries\imgui\imgui.cpp libraries\imgui\imgui_draw.cpp libraries\imgui\imgui_widgets.cpp libraries\imgui\imgui_demo.cpp -o libs.o -O3 -Isrc -Ilibraries -DSOKOL_GLES3 -DNDEBUG
)

emcc src\assets.c src\audio.c src\config.c src\data_stream.c src\file.c src\game.c src\game_editor.c src\hole.c src\hotloader.c src\lightmaps.c src\log.c src\maths.c src\main.c src\profiler.c src\renderer.c src\single_file_libs.c src\ui.c libs.o -o minigolf.html -O3 -Isrc -Ilibraries -DSOKOL_GLES3 -DNDEBUG -s USE_WEBGL2=1 -s FULL_ES3=1 -s ERROR_ON_UNDEFINED_SYMBOLS=1 -s ALLOW_MEMORY_GROWTH=1 -s USE_PTHREADS=1 -s WASM_MEM_MAX=100Mb --preload-file assets --shell-file shell.html
