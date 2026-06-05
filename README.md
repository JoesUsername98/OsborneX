# OsborneX

An orderbook project to practice concurrency in C++.

## Prerequisites

- **CMake** 3.25 or later
- **Windows:** Visual Studio 2026 with the **Desktop development with C++** workload
- **Linux (WSL):** GCC (`g++`) — install with `sudo apt install cmake g++ build-essential`
- **Visual Studio 2026** (optional) — for IDE integration via Open Folder

## Building

OsborneX uses [CMake presets](CMakePresets.json) for cross-platform builds. List available presets:

```bash
cmake --list-presets
```

### Windows (MSVC)

From regular **PowerShell** or **cmd** (Visual Studio 2026 must be installed):

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
```

For a release build:

```powershell
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
```

These presets use the **Visual Studio 2026 generator**, so CMake finds MSVC automatically.

### Linux (WSL)

```bash
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
```

For a release build:

```bash
cmake --preset linux-gcc-release
cmake --build --preset linux-gcc-release
```

Build output is written to `build/<preset-name>/`.

## Running Tests

Using CTest with presets:

```powershell
ctest --preset windows-msvc-debug
```

```bash
ctest --preset linux-gcc-debug
```

Or run the test binary directly:

```powershell
.\build\windows-msvc-debug\Orderbook\Debug\OrderbookTest.exe
```

```bash
./build/linux-gcc-debug/Orderbook/OrderbookTest
```

## Running Benchmarks

```powershell
.\build\windows-msvc-debug\Orderbook\Debug\OrderbookBench.exe
```

```bash
./build/linux-gcc-debug/Orderbook/OrderbookBench
```

Convenience targets are also available after configuring:

```powershell
cmake --build --preset windows-msvc-debug --target run-tests
cmake --build --preset windows-msvc-debug --target run-benchmarks
```

## Project Layout

```
OsborneX/
├── CMakeLists.txt              # Root project configuration
├── CMakePresets.json           # Cross-platform build presets
├── cmake/
│   ├── CompilerWarnings.cmake  # Warnings-as-errors per toolchain
│   ├── Dependencies.cmake      # FetchContent for gtest & benchmark
│   └── SolutionFolders.cmake   # Visual Studio Solution Explorer folders
└── Orderbook/
    ├── CMakeLists.txt          # Library, test, and benchmark targets
    ├── inc/
    │   └── Orderbook/          # Public headers (.hpp)
    ├── src/                    # Library sources (.cpp)
    ├── test/                   # GoogleTest sources
    └── bench/                  # Google Benchmark sources
```
