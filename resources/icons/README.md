# Application Icon

## Current Icon

The application uses a neon green "R" on a dark purple circular background.

## Files

- `icon.svg` - SVG version of the icon (scalable)
- `icon.png` - PNG version (512x512 pixels)
- `icon.qrc` - Qt resource file that embeds the icon

## Replacing the Icon

If you have your own icon file:

1. **Replace the PNG file:**
   ```bash
   cp your-icon.png resources/icons/icon.png
   ```
   
   The icon should be:
   - Format: PNG (preferred) or SVG
   - Size: 512x512 pixels (or larger, will be scaled)
   - Recommended: Multiple sizes (16, 32, 48, 64, 128, 256, 512)

2. **Or replace the SVG:**
   ```bash
   cp your-icon.svg resources/icons/icon.svg
   ```

3. **Rebuild the application:**
   ```bash
   cd build
   make
   ```

## Icon Specifications

- **Background**: Dark purple circle (#2d1b4e)
- **Border**: Dark gray (#4a4a4a)
- **Letter**: Neon green "R" (#00ff88)
- **Style**: Rounded, bold, modern

## Usage

The icon will appear in:
- Application window title bar
- System taskbar/dock
- Application launcher
- System notifications
- File manager (when installed)

## Embedding

The icon is embedded in the application binary using Qt's resource system (`icon.qrc`). This means:
- No external files needed
- Icon is always available
- Faster loading
- Smaller installation size

