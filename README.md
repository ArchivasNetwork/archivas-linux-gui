# Archivas Core GUI

A desktop GUI application for managing Archivas node and farmer processes, forked from Bitcoin Core's Qt framework.

## Overview

This project forks Bitcoin Core's mature Qt desktop application framework and rebrands it as "Archivas Core" - a GUI wrapper that manages external Archivas node and farmer binaries (written in Go).

**This is NOT a Bitcoin client.** We reuse the Qt UI framework and replace all Bitcoin-specific logic with Archivas-specific functionality.

## Project Status

✅ **Active Development** - Application is functional and available for use.

## Goals

- Desktop GUI for starting/stopping Archivas node and farmer
- Real-time chain status display
- Process log viewing
- Block and transaction browsing
- Configuration management

## Architecture

- **GUI Layer**: Qt/C++ (forked from Bitcoin Core)
- **Node/Farmer**: External Go binaries (separate repo)
- **Communication**: HTTP RPC to Archivas node

## Quick Start

### Option 1: Download Pre-built Package (Recommended)

**Linux AppImage**:
1. Download `archivas-qt-x86_64.AppImage` from [Releases](https://github.com/ArchivasNetwork/archivas-linux-gui/releases)
2. Make executable: `chmod +x archivas-qt-x86_64.AppImage`
3. Run: `./archivas-qt-x86_64.AppImage`

**Debian/Ubuntu**:
1. Download `archivas-qt_1.0.0_amd64.deb` from [Releases](https://github.com/ArchivasNetwork/archivas-linux-gui/releases)
2. Install: `sudo dpkg -i archivas-qt_1.0.0_amd64.deb`
3. Run: `archivas-qt`

### Option 2: Build from Source

1. **Install Dependencies**:
   ```bash
   sudo apt update
   sudo apt install qt6-base-dev qt6-base-dev-tools cmake build-essential
   ```

2. **Clone and Build**:
   ```bash
   git clone https://github.com/ArchivasNetwork/archivas-linux-gui.git
   cd archivas-linux-gui
   ./build.sh
   ```

3. **Run**:
   ```bash
   ./build/archivas-qt
   ```

See [BUILD.md](./BUILD.md) for detailed build instructions.

## Features

- ✅ **Node Management**: Start, stop, and monitor your Archivas node
- ✅ **Farmer Management**: Manage your farmer and view farming status  
- ✅ **Plot Creation**: Create new plots directly from the GUI
- ✅ **Real-time Status**: View chain height, difficulty, and sync status
- ✅ **Block Browser**: Browse recent blocks and transactions
- ✅ **Automatic Sync**: Background monitoring keeps your node synced
- ✅ **Fork Recovery**: Automatic recovery from blockchain forks
- ✅ **Self-contained**: Genesis file and resources bundled in the application

## Documentation

- [BUILD.md](./BUILD.md) - Detailed build instructions
- [DISTRIBUTION.md](./DISTRIBUTION.md) - How to package and distribute the application
- [CHANGELOG.md](./CHANGELOG.md) - Version history and changes
- [SPECIFICATION.md](./SPECIFICATION.md) - Technical specification
- [QUICKSTART.md](./QUICKSTART.md) - Quick start guide

## Distribution

See [DISTRIBUTION.md](./DISTRIBUTION.md) for information on:
- Creating release builds
- Packaging (AppImage, .deb)
- GitHub Releases
- Distribution checklist

## License

MIT

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.


