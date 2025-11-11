# Building for macOS

## Prerequisites

- macOS 10.14 or later
- Xcode Command Line Tools
- Homebrew
- CMake 3.16 or higher
- Qt 6.x or Qt 5.15+

## Install Dependencies

### Using Homebrew

```bash
# Install Homebrew if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install cmake qt@6 go

# Or for Qt5:
brew install cmake qt@5 go
```

## Build

1. **Clone the repository**:
   ```bash
   git clone https://github.com/ArchivasNetwork/archivas-linux-gui.git
   cd archivas-linux-gui
   ```

2. **Setup Go module** (if using local archivas repo):
   ```bash
   ./scripts/setup-go-mod.sh /path/to/your/archivas
   ```

3. **Build the application**:
   ```bash
   mkdir build
   cd build
   cmake .. -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6)
   make -j$(sysctl -n hw.ncpu)
   ```

4. **Run the application**:
   ```bash
   ./archivas-qt
   ```

## Create App Bundle

To create a macOS app bundle:

```bash
mkdir -p "Archivas Core.app/Contents/MacOS"
mkdir -p "Archivas Core.app/Contents/Resources"
cp build/archivas-qt "Archivas Core.app/Contents/MacOS/Archivas Core"
cp resources/icons/archivas-core-512x512.png "Archivas Core.app/Contents/Resources/icon.png"

# Create Info.plist
cat > "Archivas Core.app/Contents/Info.plist" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>Archivas Core</string>
    <key>CFBundleIconFile</key>
    <string>icon</string>
    <key>CFBundleIdentifier</key>
    <string>ai.archivas.archivas-core</string>
    <key>CFBundleName</key>
    <string>Archivas Core</string>
    <key>CFBundleVersion</key>
    <string>1.0.0</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0.0</string>
    <key>LSMinimumSystemVersion</key>
    <string>10.14</string>
</dict>
</plist>
EOF

# Make executable
chmod +x "Archivas Core.app/Contents/MacOS/Archivas Core"
```

## Create DMG

To create a distributable DMG file:

```bash
# Use the create-dmg script
./scripts/create-dmg.sh
```

Or manually:

```bash
# Create DMG
hdiutil create -volname "Archivas Core" -srcfolder "Archivas Core.app" -ov -format UDZO archivas-qt-1.0.0-macos.dmg
```

## Code Signing (Optional)

For distribution outside the App Store, you may want to code sign the application:

```bash
# Code sign the app
codesign --deep --force --verify --verbose --sign "Developer ID Application: Your Name" "Archivas Core.app"

# Verify signature
codesign --verify --verbose "Archivas Core.app"
```

## Notarization (Optional)

For distribution outside the App Store, you may want to notarize the application:

```bash
# Create a zip for notarization
ditto -c -k --keepParent "Archivas Core.app" archivas-core.zip

# Submit for notarization
xcrun altool --notarize-app \
    --primary-bundle-id "ai.archivas.archivas-core" \
    --username "your-apple-id@example.com" \
    --password "@keychain:AC_PASSWORD" \
    --file archivas-core.zip

# Check notarization status
xcrun altool --notarization-info <request-uuid> \
    --username "your-apple-id@example.com" \
    --password "@keychain:AC_PASSWORD"

# Staple the notarization ticket
xcrun stapler staple "Archivas Core.app"
```

## Troubleshooting

### CMake can't find Qt

Set the `CMAKE_PREFIX_PATH` variable:
```bash
cmake .. -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6)
```

### Go module issues

Make sure Go is installed and the archivas module is available:
```bash
go version
./scripts/setup-go-mod.sh /path/to/your/archivas
```

### Library linking issues

On macOS, most system libraries are automatically linked. If you encounter linking errors, check that Qt is properly installed:
```bash
brew list qt@6
```

## Configuration

On first run, the application will create a default configuration file at:
- `~/Library/Application Support/ArchivasCore/config.json`

This is handled automatically by Qt's `QStandardPaths`, so no manual configuration is needed.

