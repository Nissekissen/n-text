# N-text

NilsText, n-text or simply ntxt is a terminal based text-editor written in C. It is built by me to learn more about the C language.

## Building

Dependencies are managed with [vcpkg](https://vcpkg.io) and the build with CMake. You'll need:

- CMake (>= 3.21)
- A C compiler (Clang/GCC)
- [vcpkg](https://github.com/microsoft/vcpkg), with the `VCPKG_ROOT` environment variable set to its install location

```sh
cmake --preset default
cmake --build build
```

The first `configure` step will have vcpkg fetch and build the project's dependencies (currently just `cjson`), which can take a while the first time.

## Running

```sh
./build/n_text
```

Quit with Ctrl+Q.

## TODO

- [ ] Open/save files
- [ ] Forward delete key (only backspace exists)
- [x] Tabspace
- [ ] Scrolling for documents taller than the terminal window
- [ ] Text selection
- [ ] Undo/redo
- [ ] Print buffer is only 1024 characters
- [x] Show line numbers
- [ ] Copy/paste
- [x] Goal column when moving cursor

