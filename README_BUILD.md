# 🚀 Build the Application - Quick Start

## Current Status

✅ **Source code**: 100% complete  
✅ **Build system**: Ready  
⚠️ **Dependencies**: Need to be installed  

## One-Command Build

Run this single command to install dependencies and build:

```bash
sudo apt update && sudo apt install -y qtbase5-dev qtbase5-dev-tools cmake build-essential && ./setup-and-build.sh
```

## Step-by-Step

### 1. Install Dependencies

```bash
sudo apt update
sudo apt install -y qtbase5-dev qtbase5-dev-tools cmake build-essential
```

### 2. Build

```bash
./setup-and-build.sh
```

### 3. Run

```bash
./build/archivas-qt
```

## Where is the App?

After building, the Linux executable will be at:
```
build/archivas-qt
```

## Verification

After building, verify the executable exists:
```bash
ls -lh build/archivas-qt
file build/archivas-qt
```

You should see:
- File size: ~1-5 MB
- Type: ELF 64-bit LSB executable

## Troubleshooting

### "Permission denied" on scripts
```bash
chmod +x *.sh
```

### "CMake not found"
Install cmake:
```bash
sudo apt install cmake
```

### "Qt not found"
Install Qt development packages:
```bash
sudo apt install qtbase5-dev qtbase5-dev-tools
```

### Build errors
Check that all dependencies are installed:
```bash
dpkg -l | grep -E "(qtbase5-dev|cmake|build-essential)"
```

## Next Steps After Building

1. Run the application: `./build/archivas-qt`
2. Configure paths in Settings to point to `archivas-node` and `archivas-farmer`
3. Start the node and farmer
4. View chain status and logs

## Files Created

- `setup-and-build.sh` - Automated build script (checks dependencies, builds app)
- `install-deps.sh` - Dependency installation script
- `build.sh` - Basic build script
- `INSTALL_NOW.txt` - Quick installation guide

## Need More Help?

- See `BUILD.md` for detailed build instructions
- See `QUICKSTART.md` for quick reference
- See `SPECIFICATION.md` for implementation details

