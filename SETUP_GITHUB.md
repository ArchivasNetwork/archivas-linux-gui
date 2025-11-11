# Setting Up GitHub Repository for Automated Releases

This guide will help you set up the GitHub repository with automated release builds using GitHub Actions.

## Step 1: Initialize Git Repository

```bash
# Initialize git (if not already done)
git init

# Add remote
git remote add origin https://github.com/ArchivasNetwork/archivas-linux-gui.git

# Check remote
git remote -v
```

## Step 2: Prepare Files for Commit

Make sure you have:
- ✅ `.gitignore` (excludes build artifacts)
- ✅ `.github/workflows/release.yml` (automated release workflow)
- ✅ `.github/workflows/build.yml` (CI build workflow)
- ✅ All source code
- ✅ Documentation files

## Step 3: Initial Commit and Push

```bash
# Add all files
git add .

# Create initial commit
git commit -m "Initial release: Archivas Core GUI v1.0.0

Features:
- Complete node and farmer management
- Plot creation functionality
- Real-time chain status monitoring
- Automatic blockchain synchronization
- Background sync monitor
- Fork recovery mechanism

Includes:
- GitHub Actions workflows for automated releases
- Packaging scripts (AppImage, .deb)
- Complete documentation"

# Set main branch
git branch -M main

# Push to GitHub
git push -u origin main
```

## Step 4: Create First Release

### Option A: Via GitHub Web Interface

1. Go to: https://github.com/ArchivasNetwork/archivas-linux-gui/releases
2. Click "Create a new release"
3. **Tag version**: `v1.0.0` (must start with 'v')
4. **Release title**: `Archivas Core GUI v1.0.0`
5. **Description**: Copy from `CHANGELOG.md` or use:
   ```
   ## Archivas Core GUI v1.0.0
   
   First stable release of the Archivas Core GUI application.
   
   ### Features
   - Node management (start/stop/restart)
   - Farmer management (start/stop/restart)
   - Plot creation from GUI
   - Real-time chain status monitoring
   - Block and transaction browsing
   - Automatic blockchain synchronization
   - Background sync monitor
   - Fork recovery mechanism
   
   ### Downloads
   Packages will be automatically built and attached to this release.
   ```
6. Check "Set as the latest release"
7. Click "Publish release"

### Option B: Via Git Tags

```bash
# Create and push tag
git tag -a v1.0.0 -m "Archivas Core GUI v1.0.0"
git push origin v1.0.0

# Then create release on GitHub web interface (tag will be available)
```

## Step 5: Monitor GitHub Actions

1. Go to: https://github.com/ArchivasNetwork/archivas-linux-gui/actions
2. You should see the "Release" workflow running
3. Wait for it to complete (usually 5-10 minutes)
4. Check the workflow logs if there are any errors

## Step 6: Verify Release Assets

1. Go back to the release page
2. You should see:
   - `archivas-qt-x86_64.AppImage`
   - `archivas-qt_1.0.0_amd64.deb`
3. Download and test one of the packages

## Troubleshooting

### GitHub Actions Not Running

- Check that `.github/workflows/release.yml` exists
- Verify the file is committed and pushed
- Check GitHub Actions is enabled in repository settings

### Build Failures

- Check the Actions logs for specific errors
- Common issues:
  - Missing dependencies (should be handled by workflow)
  - Go module issues (check `go.mod` replace directive)
  - Qt version mismatches

### Packages Not Appearing

- Wait for workflow to complete
- Check workflow logs for upload errors
- Verify `GITHUB_TOKEN` is available (should be automatic)

## Future Releases

For future releases, simply:

1. Update version in `CMakeLists.txt`
2. Update `CHANGELOG.md`
3. Commit and push changes
4. Create a new release on GitHub with tag `v1.0.1` (or next version)
5. GitHub Actions will automatically build and upload packages

## Workflow Files

- **`.github/workflows/release.yml`**: Runs when a release is created
- **`.github/workflows/build.yml`**: Runs on every push/PR (for CI)

## Notes

- The workflow uses `GITHUB_TOKEN` automatically (no setup needed)
- Packages are built on Ubuntu latest
- Both AppImage and .deb packages are created
- The workflow handles all dependencies automatically

