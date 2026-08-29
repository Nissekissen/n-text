# N-text

NilsText, n-text or simply ntxt is a terminal based text-editor written in C. It is built by me to learn more about the C language.

## Building

The build is managed with CMake, wired up through [vcpkg](https://vcpkg.io) for dependency management (none currently in use, but the toolchain is set up for when that changes). You'll need:

- CMake (>= 3.21)
- A C compiler (Clang/GCC)
- [vcpkg](https://github.com/microsoft/vcpkg), with the `VCPKG_ROOT` environment variable set to its install location

```sh
cmake --preset default
cmake --build build
```

This produces the binary at `build/ntxt`.

## Running

```sh
./build/ntxt file.txt
```

Quit with Ctrl+Q. Use this directly (rather than an installed copy) while developing — see below.

## Installing

To make `ntxt` available system-wide as a regular command:

```sh
cmake --install build --prefix ~/.local   # or omit --prefix to install to the system default (/usr/local, needs sudo)
```

Make sure the install location's `bin` directory is on your `$PATH`. Once installed, you can run it from anywhere:

```sh
ntxt file.txt
```

## TODO

- [x] Open/save files
- [x] Forward delete key
- [x] Tabspace
- [x] Scrolling for documents taller than the terminal window
- [x] Text selection
- [ ] Undo/redo
- [x] Show line numbers
- [ ] Copy/paste
- [x] Goal column when moving cursor
- [ ] Rendering bug when line overflows
