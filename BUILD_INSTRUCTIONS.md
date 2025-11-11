# Build Instructions - Step by Step

## Current Situation

✅ Source code is ready and complete
❌ Build dependencies need to be installed
❌ Application needs to be compiled

## Step 1: Install Dependencies

You need to install Qt5 development packages and CMake. Run:

```bash
sudo ./install-deps.sh
```

Or manually:

```bash
sudo apt update
sudo apt install qtbase5-dev qtbase5-dev-tools cmake build-essential
```

## Step 2: Build the Application

Once dependencies are installed, build with:

```bash
./build.sh
```

Or manually:

```bash
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

## Step 3: Run the Application

```bash
./build/archivas-qt
```

## Verification

After building, you should have:
- `build/archivas-qt` - The executable
- `build/CMakeCache.txt` - CMake configuration
- `build/CMakeFiles/` - Build artifacts

## Troubleshooting

### "Permission denied" when running install-deps.sh
Make sure the script is executable:
```bash
chmod +x install-deps.sh
```

### "Qt not found" after installation
Try:
```bash
export QT_DIR=/usr/lib/x86_64-linux-gnu/cmake/Qt5
cmake .. -DCMAKE_PREFIX_PATH=/usr/lib/x86_64-linux-gnu/qt5
```

### "CMake not found"
Make sure cmake is in your PATH:
```bash
which cmake
# If not found, you may need to install it or add it to PATH
```

## What You'll Get

After successful build:
- **Location**: `build/archivas-qt`
- **Size**: ~1-5 MB (depends on Qt linking)
- **Type**: ELF 64-bit executable
- **Dependencies**: Qt5 libraries (should be installed)

## Next Steps After Building

1. Run the application
2. Configure paths to `archivas-node` and `archivas-farmer` in Settings
3. Start the node and farmer
4. View chain status and logs

