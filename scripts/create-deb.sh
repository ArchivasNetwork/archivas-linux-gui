#!/bin/bash

# Create Debian package (.deb) for Archivas Core GUI

set -e

VERSION="1.0.0"
PACKAGE_NAME="archivas-qt"
DEB_DIR="deb-package"

echo "Creating Debian package"
echo "======================="
echo ""

# Check if release build exists
if [ ! -f "build-release/archivas-qt" ]; then
    echo "ERROR: Release build not found!"
    echo "Please run ./scripts/build-release.sh first"
    exit 1
fi

# Clean previous package
rm -rf "$DEB_DIR"

# Create package structure
echo "Creating package structure..."
mkdir -p "$DEB_DIR/DEBIAN"
mkdir -p "$DEB_DIR/usr/bin"
mkdir -p "$DEB_DIR/usr/share/applications"
mkdir -p "$DEB_DIR/usr/share/icons/hicolor/256x256/apps"
mkdir -p "$DEB_DIR/usr/share/doc/$PACKAGE_NAME"

# Copy executable
echo "Copying files..."
cp build-release/archivas-qt "$DEB_DIR/usr/bin/"

# Create .desktop file
cat > "$DEB_DIR/usr/share/applications/archivas-qt.desktop" << EOF
[Desktop Entry]
Type=Application
Name=Archivas Core
Comment=Archivas Blockchain Node and Farmer GUI
Exec=archivas-qt
Icon=archivas-core
Categories=Network;Blockchain;
Terminal=false
EOF

# Copy icon
if [ -f "resources/icons/archivas-core-256x256.png" ]; then
    cp resources/icons/archivas-core-256x256.png "$DEB_DIR/usr/share/icons/hicolor/256x256/apps/archivas-core.png"
fi

# Copy documentation
if [ -f "README.md" ]; then
    cp README.md "$DEB_DIR/usr/share/doc/$PACKAGE_NAME/"
fi
if [ -f "LICENSE" ]; then
    cp LICENSE "$DEB_DIR/usr/share/doc/$PACKAGE_NAME/copyright"
fi

# Create control file
echo "Creating control file..."
cat > "$DEB_DIR/DEBIAN/control" << EOF
Package: $PACKAGE_NAME
Version: $VERSION
Section: net
Priority: optional
Architecture: amd64
Depends: libqt5core5a, libqt5widgets5, libqt5network5, libc6
Maintainer: Archivas Team <team@archivas.ai>
Description: Archivas Blockchain Node and Farmer GUI
 A desktop GUI application for managing Archivas node and farmer processes.
 Features include node management, farmer management, plot creation, and
 real-time chain status monitoring.
EOF

# Create postinst script (optional)
cat > "$DEB_DIR/DEBIAN/postinst" << 'EOF'
#!/bin/bash
# Update desktop database
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database
fi
# Update icon cache
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f /usr/share/icons/hicolor
fi
EOF
chmod +x "$DEB_DIR/DEBIAN/postinst"

# Build package
echo ""
echo "Building Debian package..."
dpkg-deb --build "$DEB_DIR" "${PACKAGE_NAME}_${VERSION}_amd64.deb"

# Check if package was created
if [ -f "${PACKAGE_NAME}_${VERSION}_amd64.deb" ]; then
    echo ""
    echo "=========================================="
    echo "✓ Debian package created successfully!"
    echo "=========================================="
    echo ""
    echo "File: ${PACKAGE_NAME}_${VERSION}_amd64.deb"
    echo "Size: $(du -h "${PACKAGE_NAME}_${VERSION}_amd64.deb" | cut -f1)"
    echo ""
    echo "To install:"
    echo "  sudo dpkg -i ${PACKAGE_NAME}_${VERSION}_amd64.deb"
    echo ""
    echo "To fix dependencies (if needed):"
    echo "  sudo apt-get install -f"
    echo ""
else
    echo ""
    echo "ERROR: Package creation failed!"
    exit 1
fi

