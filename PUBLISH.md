# Publishing to GitHub

This guide explains how to publish the Archivas Core GUI to the GitHub repository.

## Repository

**URL**: https://github.com/ArchivasNetwork/archivas-linux-gui.git

## Initial Setup

### 1. Initialize Git Repository (if not already done)

```bash
git init
git remote add origin https://github.com/ArchivasNetwork/archivas-linux-gui.git
```

### 2. Create .gitignore

Make sure `.gitignore` is in place (already created) to exclude build artifacts.

### 3. Initial Commit and Push

```bash
# Add all files
git add .

# Create initial commit
git commit -m "Initial release: Archivas Core GUI v1.0.0

- Complete node and farmer management
- Plot creation functionality
- Real-time chain status monitoring
- Automatic blockchain synchronization
- Background sync monitor
- Fork recovery mechanism"

# Push to GitHub
git branch -M main
git push -u origin main
```

## Creating a Release

### Option 1: Manual Release

1. **Build Release Version**:
   ```bash
   ./scripts/build-release.sh
   ```

2. **Create Packages**:
   ```bash
   # AppImage
   ./scripts/create-appimage.sh
   
   # Debian package
   ./scripts/create-deb.sh
   ```

3. **Create GitHub Release**:
   - Go to https://github.com/ArchivasNetwork/archivas-linux-gui/releases
   - Click "Create a new release"
   - Tag: `v1.0.0`
   - Title: `Archivas Core GUI v1.0.0`
   - Description: See `RELEASE_NOTES_TEMPLATE.md` (or use CHANGELOG.md)
   - Upload:
     - `archivas-qt-x86_64.AppImage`
     - `archivas-qt_1.0.0_amd64.deb`
   - Click "Publish release"

### Option 2: Automated Release (GitHub Actions)

The repository includes a GitHub Actions workflow (`.github/workflows/release.yml`) that automatically:
- Builds the release version
- Creates AppImage and .deb packages
- Uploads them to the release

**To use**:
1. Push code to GitHub
2. Create a release on GitHub (tag: `v1.0.0`)
3. GitHub Actions will automatically build and upload packages

## Release Checklist

- [ ] Update version in `CMakeLists.txt`
- [ ] Update `CHANGELOG.md` with new version
- [ ] Test release build locally
- [ ] Build release: `./scripts/build-release.sh`
- [ ] Test packages on clean system
- [ ] Commit and push all changes
- [ ] Create GitHub release
- [ ] Upload packages (or let GitHub Actions do it)
- [ ] Verify downloads work
- [ ] Announce release (if applicable)

## Version Numbering

Follow semantic versioning: `MAJOR.MINOR.PATCH`

Update in:
- `CMakeLists.txt`: `project(ArchivasCore VERSION 1.0.0 ...)`
- GitHub Release tag: `v1.0.0`

## Next Steps After Publishing

1. **Update README**: Ensure download links point to correct releases
2. **Documentation**: Keep docs up to date
3. **User Support**: Be ready to help users with installation
4. **Feedback**: Collect user feedback for next version
