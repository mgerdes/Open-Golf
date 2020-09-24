# Minigolf
![Image](https://i.imgur.com/TBlXedl.gif)

A minigolf game written (mostly) from scratch in C. Try it here (Works best in Chrome): http://mgerdes.github.io/minigolf.html
- Used the [Sokol](https://github.com/floooh/sokol) libraries to create a cross platform application with 3D graphics and audio. This allowed using Emscripten to compile into JavaScript and make it playable at the link above.
- Wrote the Physics code to handle collision detection and collision response for the golf ball.
- Used [ImGui](https://github.com/ocornut/imgui) to create in games tools for fast iteration. Also created an in game-editor that can be used to modify the terrain of a hole and then quickly play to get fast feedback. The game-editor can also run Scheme scripts to generate the points and faces of more interesting models.

![Image](https://i.imgur.com/fCoKT2e.gif)
- Used the library [Lightmapper](https://github.com/ands/lightmapper) to generate lightmaps for the terrain and also [xatlas](https://github.com/jpcy/xatlas) to generate lightmap UVs. These lightmaps are then baked into the files for the courses. It can also interpolate between multiple samples to create lightmaps for some moving objects.

![Image](https://i.imgur.com/ADw5kCw.gif)
![Image](https://i.imgur.com/tUJyHRk.gif)
