# Open Golf
A cross-platform minigolf game written in C. 

## Platforms
- HTML: https://mgerdes.github.io/minigolf.html (Works best in Chrome)
- iOS: https://apps.apple.com/us/app/open-golf/id1615224465
- Android: https://play.google.com/store/apps/details?id=me.mgerdes.open_golf
- Windows
- Linux

## Info
![Image](https://i.imgur.com/TBlXedl.gif)
- Used the [Sokol](https://github.com/floooh/sokol) libraries to create a cross platform application with 3D graphics and audio.
- Wrote the Physics code to handle collision detection and collision response for the golf ball.
- Used [ImGui](https://github.com/ocornut/imgui) to create in games tools for fast iteration. Also created an in game-editor that can be used to modify the terrain of a hole and then quickly play to get fast feedback. The game-editor can also run scripts to generate the points and faces of more interesting models.
![Image](https://i.imgur.com/fCoKT2e.gif)
- Used the library [Lightmapper](https://github.com/ands/lightmapper) to generate lightmaps for the terrain and also [xatlas](https://github.com/jpcy/xatlas) to generate lightmap UVs. These lightmaps are then baked into the files for the courses. It can also interpolate between multiple samples to create lightmaps for some moving objects.
![Image](https://i.imgur.com/ADw5kCw.gif)
![Image](https://i.imgur.com/tUJyHRk.gif)

## Building
### Windows
- To compile run `build\build-win64.bat`

- To start the game run `out\win64\golf.exe`

- This also creates `out\win64\golf.sln` which can be opened in Visual Studio to compile / run everything

### Linux
- To compile run `./build/build-linux.sh`

- To start the game run `out/linux/golf`

## OSX
- To compile run `./build/build-osx.sh`

- To start the game run `out/osx/golf`

## 3rd Party Libraries
- [cembed](https://github.com/rxi/cembed)
- [cimgui](https://github.com/cimgui/cimgui)
- [fast_obj](https://github.com/thisistherk/fast_obj)
- [glfw](https://github.com/glfw/glfw)
- [glslcc](https://github.com/septag/glslcc)
- [imgui](https://github.com/ocornut/imgui)
- [Kenny Art Assets](https://kenney.nl/assets)
- [lightmapper](https://github.com/ands/lightmapper)
- [mattiasgustavsson/libs](https://github.com/mattiasgustavsson/libs)
- [miniz](https://github.com/richgel999/miniz)
- [parson](https://github.com/kgabis/parson)
- [sokol](https://github.com/floooh/sokol)
- [stb](https://github.com/nothings/stb)
- [xatlas](https://github.com/jpcy/xatlas)
