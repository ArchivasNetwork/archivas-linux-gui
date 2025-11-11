# Quick Start Guide

## Current Status

✅ **Source code is ready** - All implementation is complete
⚠️ **Application needs to be built** - Qt and CMake must be installed first

## Where is the Linux App?

The Linux executable will be created at:
```
build/archivas-qt
```

**But first, you need to build it!**

## Building the Application

### Step 1: Install Dependencies

#### Ubuntu/Debian
```bash
sudo apt update
sudo apt install qt6-base-dev qt6-base-dev-tools cmake build-essential
```

#### Fedora
```bash
sudo dnf install qt6-qtbase-devel qt6-qtbase cmake gcc-c++
```

#### Arch Linux
```bash
sudo pacman -S qt6-base cmake base-devel
```

### Step 2: Build the Application

#### Option A: Use the build script (recommended)
```bash
./build.sh
```

#### Option B: Build manually
```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

### Step 3: Run the Application

```bash
./build/archivas-qt
```

Or from the build directory:
```bash
cd build
./archivas-qt
```

## What Gets Built?

After building, you'll have:
- **Executable**: `build/archivas-qt` - The main application
- **Build artifacts**: Various `.o` files and other build artifacts in `build/`

## Installation (Optional)

To install system-wide:
```bash
cd build
sudo make install
```

This will install `archivas-qt` to `/usr/local/bin/` (or your system's equivalent).

## Troubleshooting

### "Qt is not installed"
- Install Qt development packages (see Step 1 above)
- Make sure `qmake` or `pkg-config` can find Qt

### "CMake is not installed"
- Install CMake: `sudo apt install cmake` (Ubuntu/Debian)

### "CMake can't find Qt"
- Set `CMAKE_PREFIX_PATH`:
  ```bash
  cmake .. -DCMAKE_PREFIX_PATH=/usr/lib/qt6
  ```

### Build errors
- Check that you have a C++17 compatible compiler
- Verify all Qt modules are installed (Core, Widgets, Network)
- Check the error messages for missing dependencies

## Next Steps

1. **Build the application** (see above)
2. **Run the application**: `./build/archivas-qt`
3. **Configure paths** in Settings to point to your `archivas-node` and `archivas-farmer` binaries
4. **Start farming!**

## File Locations

- **Source code**: `src/qt/` and `include/qt/`
- **Build directory**: `build/`
- **Executable**: `build/archivas-qt` (after building)
- **Configuration**: `~/.config/ArchivasCore/config.json` (created on first run)

## Need Help?

- See [BUILD.md](./BUILD.md) for detailed build instructions
- See [SPECIFICATION.md](./SPECIFICATION.md) for implementation details
- See [IMPLEMENTATION.md](./IMPLEMENTATION.md) for architecture overview

