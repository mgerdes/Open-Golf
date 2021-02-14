BIN := minigolf
SOURCES := src/assets.c src/audio.c src/config.c src/data_stream.c src/file.c src/game.c src/game_editor.c src/hole.c src/hotloader.c src/lightmaps.c src/log.c src/maths.c src/main.c src/obj_converter.c src/profiler.c src/renderer.c src/single_file_libs.c src/ui.c libraries/array.c libraries/glad.c libraries/map.c libraries/miniz.c libraries/s7.c libraries/vec.c libraries/mscript.c libraries/mscript/parser.c
OBJS := $(SOURCES:.c=.o)
DEPS = $(SOURCES:%.c=%.d)
CPP_SOURCES := libraries/cimgui.cpp libraries/xatlas.cpp libraries/imgui/imgui.cpp libraries/imgui/imgui_draw.cpp libraries/imgui/imgui_widgets.cpp libraries/imgui/imgui_demo.cpp
CPP_OBJS := $(CPP_SOURCES:.cpp=.o)
CPP_DEPS = $(CPP_SOURCES:%.cpp=%.d)
CFLAGS := -Isrc -Wall -Ilibraries -g -Wno-unused-value -Wno-unused-function -Wno-unused-variable -DSOKOL_GLCORE33 -DHOLE_EDITOR -DHOTLOADER_ACTIVE

all: $(BIN)

$(BIN): $(OBJS) $(CPP_OBJS)
	gcc -o $@ $(OBJS) $(CPP_OBJS) -lm -lpthread -lstdc++ -lGL -ldl -lX11 -lasound -lXi -lXcursor -g

$(OBJS): %.o: %.c
	gcc $(CFLAGS) -c -MMD -o $@ $< -g

$(CPP_OBJS): %.o: %.cpp
	gcc $(CFLAGS) -c -MMD -o $@ $< -std=c++11 -g

-include $(DEPS) 
-include $(CPP_DEPS) 

clean: 
	rm -f $(BIN) $(OBJS) $(CPP_OBJS) $(DEPS) $(CPP_DEPS)
