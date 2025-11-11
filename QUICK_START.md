# Quick Start Guide

## Running Archivas Core GUI

### Recommended: Use the Wrapper Script

```bash
./run-archivas-qt.sh
```

This runs the application silently without any output messages.

### Direct Run (May Show Qt Messages)

```bash
./build/archivas-qt
```

You may see Qt plugin loading messages - these are **normal and harmless**.

### From Application Menu

Search for "Archivas Core" in your application launcher and click it.

## About Qt Plugin Messages

If you see messages like:
```
Found metadata in lib /usr/lib/x86_64-linux-gnu/qt5/plugins/...
QFactoryLoader::QFactoryLoader() checking directory path...
```

**These are completely normal!** They show Qt discovering available plugins (Wayland, XCB, input methods, themes, etc.). The application works perfectly fine with or without these messages.

To avoid seeing them:
- Use the wrapper script: `./run-archivas-qt.sh`
- Or redirect output: `./build/archivas-qt 2>/dev/null`

## Troubleshooting

### Application Won't Start

1. Check if another instance is running:
   ```bash
   ps aux | grep archivas-qt
   ```

2. Kill existing instances:
   ```bash
   killall archivas-qt
   ```

3. Remove stale lock file:
   ```bash
   rm -f /tmp/archivas-core-gui.lock
   ```

### Window Doesn't Appear

1. Check if the application is running:
   ```bash
   ps aux | grep archivas-qt
   ```

2. Look in your taskbar/dock for the Archivas Core icon

3. Try Alt+Tab to switch windows

4. Check all workspaces/desktops

### Build Issues

1. Install dependencies:
   ```bash
   ./install-deps.sh
   ```

2. Build the application:
   ```bash
   ./setup-and-build.sh
   ```

## More Information

- See `README.md` for detailed documentation
- See `README_QT_MESSAGES.md` for information about Qt messages
- See `SPECIFICATION.md` for project specifications

