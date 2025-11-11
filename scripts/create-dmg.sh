#!/bin/bash

# Create macOS DMG package for Archivas Core GUI
# This script creates a .dmg file for distribution on macOS

set -e

VERSION="1.0.0"
APP_NAME="Archivas Core"
DMG_NAME="archivas-qt-${VERSION}-macos.dmg"
APP_BUNDLE="${APP_NAME}.app"

echo "Creating macOS DMG package"
echo "=========================="
echo ""

# Check if we're on macOS
if [[ "$OSTYPE" != "darwin"* ]]; then
    echo "ERROR: This script must be run on macOS"
    exit 1
fi

# Check if release build exists
if [ ! -d "build-release/${APP_BUNDLE}" ]; then
    echo "ERROR: App bundle not found!"
    echo "Looking for: build-release/${APP_BUNDLE}"
    echo "Current directory: $(pwd)"
    echo "Files in build-release:"
    ls -la build-release/ 2>/dev/null || echo "build-release directory doesn't exist"
    exit 1
fi

# Clean previous DMG
rm -f "$DMG_NAME"
rm -rf dmg-temp

# Create temporary directory for DMG
echo "Creating DMG structure..."
mkdir -p dmg-temp
cp -R "build-release/${APP_BUNDLE}" dmg-temp/

# Create Applications symlink
ln -s /Applications dmg-temp/Applications

# Create DMG
echo ""
echo "Creating DMG file..."
hdiutil create -volname "${APP_NAME}" -srcfolder dmg-temp -ov -format UDZO "$DMG_NAME"

# Clean up
rm -rf dmg-temp

# Check if DMG was created
if [ -f "$DMG_NAME" ]; then
    echo ""
    echo "=========================================="
    echo "✓ DMG package created successfully!"
    echo "=========================================="
    echo ""
    echo "File: $DMG_NAME"
    echo "Size: $(du -h "$DMG_NAME" | cut -f1)"
    echo ""
    echo "To test:"
    echo "  open $DMG_NAME"
    echo ""
else
    echo ""
    echo "ERROR: DMG creation failed!"
    exit 1
fi

