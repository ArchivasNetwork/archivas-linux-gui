# About Qt Plugin Loading Messages

## What You're Seeing

When running the Archivas Core GUI application, you may see messages like:

```
Found metadata in lib /usr/lib/x86_64-linux-gnu/qt5/plugins/platforms/libqwayland-generic.so
QFactoryLoader::QFactoryLoader() checking directory path...
```

## Are These Messages a Problem?

**No! These messages are completely normal and harmless.**

They are informational messages from Qt's plugin system that show:
- Qt discovering available platform plugins (Wayland, XCB, etc.)
- Qt loading input method plugins (ibus, compose)
- Qt loading theme plugins (gtk3)
- Qt loading icon engine plugins (svg)

## Why Do They Appear?

These messages are printed by Qt's plugin loader **before** our application code runs. This happens during Qt's initialization phase, so we cannot suppress them from within the application code.

## How to Suppress Them

### Option 1: Use the Wrapper Script (Recommended)

```bash
./run-archivas-qt.sh
```

This script runs the application with all output redirected, so you won't see any messages.

### Option 2: Run in Background

```bash
./build/archivas-qt >/dev/null 2>&1 &
```

### Option 3: Redirect Output Manually

```bash
./build/archivas-qt 2>/dev/null
```

### Option 4: Filter the Output

```bash
./build/archivas-qt 2>&1 | grep -v "QFactoryLoader" | grep -v "Found metadata"
```

## Desktop File Integration

The desktop file (`~/.local/share/applications/archivas-core.desktop`) is configured to use the wrapper script, so when you launch the application from your application menu, you won't see these messages.

## Technical Details

- These messages come from Qt's `QFactoryLoader` class
- They're printed to `stderr` during plugin discovery
- They occur before `QApplication` is created
- They're part of Qt's normal initialization process
- The application works correctly regardless of these messages

## Conclusion

**You can safely ignore these messages.** They don't indicate any problems, and the application functions normally. Use the wrapper script if you want a clean, silent startup.

