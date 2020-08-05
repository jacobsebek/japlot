# ![alt text](https://raw.githubusercontent.com/jacobsebek/japlot/master/res/icon.bmp "Logo")	   \< The JaPlot Graphing Calculator \>

**JaPlot** (*/d͡ʒeɪplɒt/*) is an open source graphing calculator written in C,
some of it's main features are :

 - The JaPlot shell environment
 - Ability to add variables, constants, functions & more
 - Graphing and calculating arbitrary equations
 - Transparent usage of plugin functions (.dll)
 - Plotting data from/to a file
 - Script running


![alt text](https://raw.githubusercontent.com/jacobsebek/japlot/master/doc/screenshots/script_demo.png "A script demo showing 3 gaussian curves")

<sup>DISCLAIMER : JaPlot isn't a computing environment as it doesn't support conditional branching (..yet?)</sup>

## Download
Binaries for JaPlot aren't available yet as it is incomplete and still under development

## Building
JaPlot can be build from the source code using a modern compiler

### On Windows
It is strongly recommended to use msys2 + Mingw64 for building on windows
(as JaPlot is tested with it and it makes compiling other dependencies easier overall)

The dependencies for building on windows are : 
 - `SDL2`, `SDL_gpu` (graphics)
- `pthread-win32` (threads, comes installed with mingw64)
- `dlfcn-win32` (dynamic loading for plugins)
- `Dash` (data structure library, repo [jacobsebek/dash](https://github.com/jacobsebek/dash))

After installing the dependencies, specify these environment variables :

- `SDL_CONFIG` - path to your sdl2-config file (usually in the SDL2 `bin` folder)
- `DASH_PATH` - path to the `dash` installation directory (the one containing the `lib` and `include` - folders)
- `DLFCN_PATH` - path to the `dlfcn-win32` library (the one containing the `lib` and `include folders`)

And finally, build using `Makefile-windows`

__Notes__ : 
Make sure that the `SDL_gpu` include files are in the same directory as all other `SDL2` files and that the library files are in the `lib` directory of the `SDL2` installation too (same structure as if on Linux)
The makefile is assuming you are using the precompiled version of `SDL_gpu`, so it is looking for the file `SDL_gpu.lib`, you can change this to `-lSDL2_gpu` if you happen to have your own compiled archive

### On Linux
Compiling on Linux is a lot easier than on windows 
The dependencies for building on Linux are : 

- `SDL2`, `SDL_gpu` (graphics)
- `Dash` (data structure library, repo [jacobsebek/dash](https://github.com/jacobsebek/dash))

After installing the dependencies, specify these environment variables :

- `SDL_CONFIG` - path to your `sdl2-config` file (usually in the SDL2 `bin` folder), defaulted to `/usr/local/bin/sdl2-config`
- `DASH_PATH` - path to the dash installation directory (the one containing the `lib` and `include` folders)

And finally, build using the `Makefile`

