# qoiview 

[![build](https://github.com/floooh/qoiview/actions/workflows/main.yml/badge.svg)](https://github.com/floooh/qoiview/actions/workflows/main.yml)

A simple .qoi image file viewer on top of the sokol headers.

QOI: https://github.com/phoboslab/qoi
Sokol: https://github.com/floooh/sokol

[WASM version](https://floooh.github.io/qoiview/qoiview.html) (see below for build instructions)

## Clone:

```bash
> git clone https://github.com/floooh/qoiview
> cd qoiview
```

## Build:

```bash
> mkdir build
> cd build

> cmake ..
> cmake --build .
```

To build a Release version on Linux and Mac:

```bash
> cmake -DCMAKE_BUILD_TYPE=MinSizeRel ..
> cmake --build .
```

To build a Release version on Windows with the VisualStudio toolchain:

```bash
> cmake ..
> cmake --build . --config MinSizeRel
```

NOTE: on Linux you'll also need to install the 'usual' dev-packages needed for X11+GL development. On OpenBSD, it is assumed you have X installed.

## Run:

On Linux, OpenBSD and macOS:
```bash
> ./qoiview file=../images/dice.qoi
```

On Windows with the Visual Studio toolchain the exe is in a subdirectory:
```bash
> Debug\qoiview.exe file=../images/dice.qoi
> MinSizeRel\qoiview.exe file=../images/dice.qoi
```

## Build and Run WASM/HTML version via Emscripten (Linux, macOS)

Setup the emscripten SDK as described here:

https://emscripten.org/docs/getting_started/downloads.html#installation-instructions

Don't forget to run ```source ./emsdk_env.sh``` after activating the SDK.

And then in the ```qoiview``` directory:

```
mkdir build
cd build
emcmake cmake -DCMAKE_BUILD_TYPE=MinSizeRel ..
cmake --build .
```

To run the compilation result in the system web browser:

```
> emrun qoiview.html
```

...which should look like [this](https://floooh.github.io/qoiview/qoiview.html).

(this procedure should also work on Windows with ```make``` in the path, but
is currently untested)

## IDE Support

### Visual Studio (Windows)

On Windows, cmake will automatically create a Visual Studio solution file in
the build directory, which can be opened with:

```bash
> start qoiview.sln
```

### Xcode (macOS)

Replace ```cmake ..``` with ```cmake -GXcode ..``` and open the generated
Xcode project:

```bash
> cmake -GXcode ..
> open qoiview.xcodeproj
```

### Visual Studio Code (Windows, Linux, macOS)

Use the [MS C/C++ extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)
together with the [MS CMake Tools extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools)
and start Visual Studio code from the project's root directory. The CMake
extension will detect the CMakeLists.txt file and take over from there.
