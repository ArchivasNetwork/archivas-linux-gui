#!/bin/bash

# Install Archivas Core icon and desktop file for Linux

set -e

echo "Installing Archivas Core icon and desktop file..."
echo "================================================"
echo ""

# Get the script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ICON_DIR="$SCRIPT_DIR/resources/icons"
DESKTOP_FILE="$SCRIPT_DIR/resources/archivas-core.desktop"

# Check if icons exist
if [ ! -f "$ICON_DIR/archivas-core.png" ]; then
    echo "Creating icons..."
    python3 "$ICON_DIR/create-icon.py"
fi

# Create icon directory in user's home
USER_ICON_DIR="$HOME/.local/share/icons/hicolor"
mkdir -p "$USER_ICON_DIR"

# Install icons in standard sizes
SIZES=(16 22 24 32 48 64 96 128 256 512)
for size in "${SIZES[@]}"; do
    if [ -f "$ICON_DIR/archivas-core-${size}x${size}.png" ]; then
        size_dir="$USER_ICON_DIR/${size}x${size}/apps"
        mkdir -p "$size_dir"
        cp "$ICON_DIR/archivas-core-${size}x${size}.png" "$size_dir/archivas-core.png"
        echo "  Installed ${size}x${size} icon"
    fi
done

# Install scalable icon (if exists)
if [ -f "$ICON_DIR/archivas-core.svg.png" ]; then
    scalable_dir="$USER_ICON_DIR/scalable/apps"
    mkdir -p "$scalable_dir"
    cp "$ICON_DIR/archivas-core.svg.png" "$scalable_dir/archivas-core.png"
    echo "  Installed scalable icon"
fi

# Update icon cache
if command -v gtk-update-icon-cache &> /dev/null; then
    gtk-update-icon-cache -f -t "$USER_ICON_DIR" 2>/dev/null || true
    echo "  Updated icon cache"
fi

# Install desktop file
DESKTOP_DIR="$HOME/.local/share/applications"
mkdir -p "$DESKTOP_DIR"

# Get absolute path to executable
EXECUTABLE_PATH="$SCRIPT_DIR/build/archivas-qt"
if [ ! -f "$EXECUTABLE_PATH" ]; then
    echo "Warning: Executable not found at $EXECUTABLE_PATH"
    echo "  Desktop file will use 'archivas-qt' (must be in PATH)"
    EXECUTABLE="archivas-qt"
else
    EXECUTABLE="$EXECUTABLE_PATH"
fi

# Create desktop file with correct executable path
cat > "$DESKTOP_DIR/archivas-core.desktop" << EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=Archivas Core
Comment=Desktop GUI for managing Archivas node and farmer processes
Exec=$EXECUTABLE
Icon=archivas-core
Terminal=false
Categories=Network;Blockchain;
StartupNotify=true
MimeType=
EOF

chmod +x "$DESKTOP_DIR/archivas-core.desktop"
echo "  Installed desktop file: $DESKTOP_DIR/archivas-core.desktop"

echo ""
echo "================================================"
echo "Installation complete!"
echo "================================================"
echo ""
echo "The icon should now appear in your application launcher and taskbar."
echo ""
echo "To update the icon cache system-wide (requires sudo):"
echo "  sudo gtk-update-icon-cache /usr/share/icons/hicolor"
echo ""

