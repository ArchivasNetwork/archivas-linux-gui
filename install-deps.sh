#!/bin/bash

# Installation script for Archivas Core GUI dependencies
# Run this with: sudo ./install-deps.sh

set -e

echo "Installing dependencies for Archivas Core GUI..."
echo "================================================"
echo ""

# Detect distribution
if [ -f /etc/debian_version ]; then
    echo "Detected Debian/Ubuntu system"
    echo "Installing Qt5 development packages and CMake..."
    apt update
    apt install -y qtbase5-dev qtbase5-dev-tools cmake build-essential
elif [ -f /etc/redhat-release ]; then
    echo "Detected Red Hat/Fedora system"
    echo "Installing Qt5 development packages and CMake..."
    dnf install -y qt5-qtbase-devel cmake gcc-c++
elif [ -f /etc/arch-release ]; then
    echo "Detected Arch Linux system"
    echo "Installing Qt5 development packages and CMake..."
    pacman -S --noconfirm qt5-base cmake base-devel
else
    echo "Unknown distribution. Please install manually:"
    echo "  - qtbase5-dev (or qt5-qtbase-devel)"
    echo "  - cmake"
    echo "  - build-essential (or equivalent)"
    exit 1
fi

echo ""
echo "================================================"
echo "Dependencies installed successfully!"
echo "================================================"
echo ""
echo "You can now build the application with:"
echo "  ./build.sh"
echo ""

