# Fix Installation Issues

## Problem

The package installation failed due to repository connection issues. Two packages failed to download:
- `libxcb1-dev`
- `libxext-dev`

## Solution

Run this command to retry and fix the installation:

```bash
sudo apt install -y --fix-missing libxcb1-dev libxext-dev qtbase5-dev qtbase5-dev-tools cmake build-essential
```

Or use the fix script:

```bash
./fix-install.sh
```

## Alternative: Try Different Repository

If the repository continues to have issues, you can try:

1. **Wait and retry** - Repository issues are often temporary
2. **Use a different mirror** - Update your `/etc/apt/sources.list`
3. **Install packages individually** - Install each package one by one

## After Fixing Installation

Once the packages are installed, verify with:

```bash
which cmake qmake
cmake --version
qmake --version
```

Then build the application:

```bash
./setup-and-build.sh
```

## Check Installation Status

To see what's installed:

```bash
dpkg -l | grep -E "(cmake|qtbase5)"
```

You should see:
- `cmake`
- `qtbase5-dev`
- `qtbase5-dev-tools`
- `libqt5core5a` (or similar)
- Other Qt5 development packages

## If Installation Still Fails

1. Check your internet connection
2. Try updating repositories: `sudo apt update`
3. Check repository status: `sudo apt-get check`
4. Try a different mirror or wait for repository to recover

