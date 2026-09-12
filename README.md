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

## Running the Network Demo

Three executables let you run OsborneX as a real multi-process system: a **server**
hosting the orderbook, one or more **bots** (market participants trading a basic
randomized strategy), and a **TUI** that shows live bids/asks and the trade tape.
Build them like any other target (they're part of the default build):

```powershell
cmake --build --preset windows-msvc-debug --target ServerMain BotMain TuiMain
```

```bash
cmake --build --preset linux-gcc-debug --target ServerMain BotMain TuiMain
```

All three take positional command-line arguments with sensible defaults, so they
can be run with no arguments at all for a quick local demo.

### 1. Start the server

```powershell
.\build\windows-msvc-debug\Server\Debug\ServerMain.exe [order_entry_port] [market_data_group] [market_data_port]
```

```bash
./build/linux-gcc-debug/Server/ServerMain [order_entry_port] [market_data_group] [market_data_port]
```

Defaults: order-entry TCP port `9001`, market-data UDP multicast group `239.1.1.1:9002`.
Press Enter in its console to stop it.

### 2. Start one or more bots

```powershell
.\build\windows-msvc-debug\Bot\Debug\BotMain.exe [server_host] [order_entry_port] [market_data_group] [market_data_port] [symbol]
```

```bash
./build/linux-gcc-debug/Bot/BotMain [server_host] [order_entry_port] [market_data_group] [market_data_port] [symbol]
```

Defaults: `127.0.0.1 9001 239.1.1.1 9002 1`. A bot retries connecting to the server
for up to ~10 seconds, so it's fine to start it before or shortly after the server.
Run it twice (same symbol, default `1`) to get two participants trading against
each other. Press Enter in its console to stop it.

### 3. Watch it live in the TUI

```powershell
.\build\windows-msvc-debug\Tui\Debug\TuiMain.exe [market_data_group] [market_data_port] [order_entry_host] [order_entry_port]
```

```bash
./build/linux-gcc-debug/Tui/TuiMain [market_data_group] [market_data_port] [order_entry_host] [order_entry_port]
```

Defaults: `239.1.1.1 9002 127.0.0.1 9001`. Shows live per-symbol top-of-book and a
scrolling trade tape; press `Ctrl+C` (or `q`, depending on terminal) to exit. It's a
passive viewer for now — it connects an order-entry client but never sends from it,
so wiring up manual order entry later is a small addition, not a rewrite.

### One-click demo (VS Code)

`.vscode/launch.json` includes compound launch configurations that start the server
and two bots (and optionally the TUI) together — open the Run and Debug panel and
pick **"Launch Demo: Server + 2 Bots (Windows/MSVC)"** (or the `+ TUI` / Linux/GCC
variants). Each process gets its own integrated terminal tab.

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
