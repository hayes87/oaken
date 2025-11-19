# Oaken Engine

## Build Instructions

1. Ensure you have CMake 3.25+ and a C++20 compiler installed.
2. Ensure you have Vcpkg installed and integrated.

```bash
mkdir Build
cd Build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build .
```

## Running

Run the `OakenEngine` executable from the `Build/Debug` directory.

## Controls

- **Space**: Cast Spell (Logs to Console)
- **Close Window**: Exit Application
