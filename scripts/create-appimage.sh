#!/bin/bash

# Create AppImage package for Archivas Core GUI
# AppImage is a portable Linux application format

set -e

echo "Creating AppImage package"
echo "========================="
echo ""

# Ensure we're in the project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

# Check if release build exists
if [ ! -f "build-release/archivas-qt" ]; then
    echo "ERROR: Release build not found!"
    echo "Looking for: build-release/archivas-qt"
    echo "Current directory: $(pwd)"
    echo "Files in build-release:"
    ls -la build-release/ 2>/dev/null || echo "build-release directory doesn't exist"
    exit 1
fi

# Check for linuxdeployqt
if ! command -v linuxdeployqt &> /dev/null; then
    echo "ERROR: linuxdeployqt not found!"
    echo ""
    echo "Install linuxdeployqt:"
    echo "  wget https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage"
    echo "  chmod +x linuxdeployqt-continuous-x86_64.AppImage"
    echo "  sudo mv linuxdeployqt-continuous-x86_64.AppImage /usr/local/bin/linuxdeployqt"
    echo ""
    exit 1
fi

# Create AppDir structure
echo "Creating AppDir structure..."
APP_DIR="AppDir"
rm -rf "$APP_DIR"
mkdir -p "$APP_DIR/usr/bin"
mkdir -p "$APP_DIR/usr/share/applications"
mkdir -p "$APP_DIR/usr/share/icons/hicolor/256x256/apps"

# Copy executable
echo "Copying executable..."
cp build-release/archivas-qt "$APP_DIR/usr/bin/"

# Create .desktop file
echo "Creating .desktop file..."
cat > "$APP_DIR/usr/share/applications/archivas-qt.desktop" << EOF
[Desktop Entry]
Type=Application
Name=Archivas Core
Comment=Archivas Blockchain Node and Farmer GUI
Exec=archivas-qt
Icon=archivas-core
Categories=Network;Blockchain;
Terminal=false
EOF

# Copy icon (if available)
if [ -f "resources/icons/archivas-core-256x256.png" ]; then
    cp resources/icons/archivas-core-256x256.png "$APP_DIR/usr/share/icons/hicolor/256x256/apps/archivas-core.png"
fi

# Use linuxdeployqt to create AppImage
echo ""
echo "Bundling Qt libraries and creating AppImage..."
linuxdeployqt "$APP_DIR/usr/share/applications/archivas-qt.desktop" -appimage

# Check if AppImage was created
APPIMAGE=$(ls -t archivas-qt*.AppImage 2>/dev/null | head -1)
if [ -n "$APPIMAGE" ]; then
    echo ""
    echo "=========================================="
    echo "✓ AppImage created successfully!"
    echo "=========================================="
    echo ""
    echo "File: $APPIMAGE"
    echo "Size: $(du -h "$APPIMAGE" | cut -f1)"
    echo ""
    echo "To test:"
    echo "  chmod +x $APPIMAGE"
    echo "  ./$APPIMAGE"
    echo ""
else
    echo ""
    echo "ERROR: AppImage creation failed!"
    exit 1
fi

