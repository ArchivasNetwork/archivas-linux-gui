# Building Archivas Core GUI

## Prerequisites

- CMake 3.16 or higher
- Qt 5.15+ or Qt 6.x (with QtCore, QtWidgets, QtNetwork modules)
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)

## Linux Build Instructions

### Install Dependencies

#### Ubuntu/Debian
```bash
sudo apt update
sudo apt install build-essential cmake qt6-base-dev qt6-base-dev-tools
# Or for Qt5:
sudo apt install build-essential cmake qtbase5-dev qtbase5-dev-tools
```

#### Fedora
```bash
sudo dnf install gcc-c++ cmake qt6-qtbase-devel qt6-qtbase
# Or for Qt5:
sudo dnf install gcc-c++ cmake qt5-qtbase-devel qt5-qtbase
```

#### Arch Linux
```bash
sudo pacman -S base-devel cmake qt6-base
# Or for Qt5:
sudo pacman -S base-devel cmake qt5-base
```

### Build

```bash
mkdir build
cd build
cmake ..
make
```

The executable will be created as `build/archivas-qt`.

### Run

```bash
./archivas-qt
```

## macOS Build Instructions

### Install Dependencies

```bash
# Using Homebrew
brew install cmake qt@6
# Or for Qt5:
brew install cmake qt@5
```

### Build

```bash
mkdir build
cd build
cmake .. -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6)
make
# Or for Qt5:
cmake .. -DCMAKE_PREFIX_PATH=$(brew --prefix qt@5)
make
```

### Run

```bash
./archivas-qt
```

## Windows Build Instructions

### Install Dependencies

1. Install Visual Studio 2019 or later with C++ development tools
2. Install CMake from https://cmake.org/download/
3. Install Qt from https://www.qt.io/download

### Build

```bash
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019" -A x64 -DCMAKE_PREFIX_PATH=C:/Qt/6.x.x/msvc2019_64
cmake --build . --config Release
```

Replace `6.x.x` with your Qt version and `msvc2019_64` with your compiler version.

### Run

The executable will be in `build/Release/archivas-qt.exe`.

## Configuration

On first run, the application will create a default configuration file at:

- **Linux**: `~/.config/ArchivasCore/config.json` or `~/.archivas-core/config.json`
- **macOS**: `~/Library/Application Support/ArchivasCore/config.json`
- **Windows**: `%APPDATA%\ArchivasCore\config.json`

You can configure:
- Node executable path
- Farmer executable path
- Plots directory
- RPC URL
- Network settings
- Auto-start options

Use the Settings menu to configure these options.

## Troubleshooting

### CMake can't find Qt

Set the `CMAKE_PREFIX_PATH` variable to your Qt installation:
```bash
cmake .. -DCMAKE_PREFIX_PATH=/path/to/qt
```

### Missing Qt modules

Ensure you have installed the following Qt modules:
- QtCore
- QtWidgets
- QtNetwork

### Compilation errors

Make sure you're using a C++17 compatible compiler and have all required dependencies installed.

## Development

### Project Structure

```
archivas-core-gui/
├── src/qt/           # Source files
├── include/qt/       # Header files
├── resources/ui/     # UI files (if using .ui files)
├── CMakeLists.txt    # Build configuration
└── BUILD.md          # This file
```

### Adding New Features

1. Add header files to `include/qt/`
2. Add source files to `src/qt/`
3. Update `CMakeLists.txt` with new files
4. Rebuild the project

## License

MIT (inherited from Bitcoin Core)

