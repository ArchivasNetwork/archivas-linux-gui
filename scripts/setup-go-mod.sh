#!/bin/bash

# Setup script for go.mod replace directive
# This allows users to build with a local archivas repository

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BRIDGE_DIR="$SCRIPT_DIR/../src/go/bridge"

echo "Archivas Core GUI - Go Module Setup"
echo "===================================="
echo ""

# Check if archivas path is provided
if [ -z "$1" ]; then
    echo "Usage: $0 <path-to-archivas-repo>"
    echo ""
    echo "Example:"
    echo "  $0 /home/user/archivas"
    echo "  $0 ../archivas"
    echo ""
    echo "This script updates go.mod to use your local archivas repository."
    echo "This is optional - if you don't have a local archivas repo,"
    echo "the build will use the GitHub module (if available)."
    exit 1
fi

ARCHIVAS_PATH="$1"

# Check if path exists
if [ ! -d "$ARCHIVAS_PATH" ]; then
    echo "ERROR: Path does not exist: $ARCHIVAS_PATH"
    exit 1
fi

# Convert to absolute path
ARCHIVAS_PATH=$(cd "$ARCHIVAS_PATH" && pwd)

echo "Setting up go.mod to use local archivas repository:"
echo "  Path: $ARCHIVAS_PATH"
echo ""

cd "$BRIDGE_DIR"

# Check if replace directive already exists
if grep -q "replace github.com/ArchivasNetwork/archivas" go.mod; then
    # Update existing replace directive
    if [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS
        sed -i '' "s|replace github.com/ArchivasNetwork/archivas => .*|replace github.com/ArchivasNetwork/archivas => $ARCHIVAS_PATH|" go.mod
    else
        # Linux
        sed -i "s|replace github.com/ArchivasNetwork/archivas => .*|replace github.com/ArchivasNetwork/archivas => $ARCHIVAS_PATH|" go.mod
    fi
    echo "✓ Updated existing replace directive"
else
    # Add new replace directive
    echo "" >> go.mod
    echo "replace github.com/ArchivasNetwork/archivas => $ARCHIVAS_PATH" >> go.mod
    echo "✓ Added replace directive"
fi

echo ""
echo "Setup complete! You can now build the application."
echo ""
echo "To remove the replace directive (use GitHub module instead):"
echo "  Edit src/go/bridge/go.mod and remove the 'replace' line"

