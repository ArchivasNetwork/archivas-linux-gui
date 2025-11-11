# Third-Party Attributions

## Bitcoin Core

This project is a fork of Bitcoin Core's Qt application framework. We have reused the mature Qt desktop shell architecture (menus, status bar, debug window, RPC console layout, platform integration) and replaced Bitcoin-specific logic with Archivas-specific functionality.

**Original Project**: Bitcoin Core
**Repository**: https://github.com/bitcoin/bitcoin
**License**: MIT License
**Copyright**: Copyright (c) 2009-2023 The Bitcoin Core developers

### What We Used

- Qt application architecture and patterns
- UI layout concepts (sidebar navigation, stacked widgets, etc.)
- Process management patterns
- Configuration management patterns
- Platform integration concepts

### What We Changed

- Removed all Bitcoin-specific consensus logic
- Removed Bitcoin wallet functionality
- Removed Bitcoin P2P networking
- Removed Bitcoin RPC server
- Replaced with Archivas node/farmer process management
- Replaced with Archivas HTTP RPC client
- Rebranded from "Bitcoin Core" to "Archivas Core"
- Changed all UI text and branding

### License

Bitcoin Core is licensed under the MIT License:

```
Copyright (c) 2009-2023 The Bitcoin Core developers
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

This project maintains the same MIT license and includes this attribution as required.

## Qt Framework

**Project**: Qt
**Website**: https://www.qt.io/
**License**: LGPL v3 (or commercial)
**Copyright**: The Qt Company Ltd.

Qt is used under the terms of the GNU Lesser General Public License version 3 (LGPL v3) or a commercial license. See https://www.qt.io/licensing/ for details.

## Archivas Network

**Project**: Archivas Network
**Website**: https://archivas.ai/
**Note**: This GUI is designed to work with Archivas node and farmer binaries, which are separate projects.

---

## Disclaimer

This project is a derivative work based on Bitcoin Core. While we have made significant changes to remove Bitcoin-specific functionality and replace it with Archivas-specific features, some architectural patterns and UI concepts are derived from Bitcoin Core's implementation.

We acknowledge and thank the Bitcoin Core developers for their excellent work on the Qt framework that made this project possible.

