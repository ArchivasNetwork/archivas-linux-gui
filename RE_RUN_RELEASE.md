# How to Re-run the Release Workflow to Get .deb Files

## The Problem

The Release workflow only runs automatically when you **create** a release. Since we've fixed the workflow after the release was created, we need to manually trigger it again.

## Solution: Re-run the Release Workflow

### Option 1: Re-run via GitHub UI (Easiest)

1. Go to: https://github.com/ArchivasNetwork/archivas-linux-gui/actions
2. Click on "Release" in the left sidebar (under "Build and Test")
3. Find the failed workflow run for v1.0.0
4. Click on it
5. Click the "Re-run jobs" button (top right)
6. Select "Re-run failed jobs"

This will re-run the Release workflow with all our fixes, and it should successfully create the .deb and AppImage packages.

### Option 2: Edit and Re-save the Release

1. Go to: https://github.com/ArchivasNetwork/archivas-linux-gui/releases/tag/v1.0.0
2. Click the "Edit" button (pencil icon, top right)
3. Click "Update release" without changing anything

This will trigger the Release workflow again.

### Option 3: Create a New Release (v1.0.1)

1. Go to: https://github.com/ArchivasNetwork/archivas-linux-gui/releases
2. Click "Create a new release"
3. Tag: `v1.0.1`
4. Title: `Archivas Core GUI v1.0.1`
5. Description: Same as v1.0.0 or add patch notes
6. Click "Publish release"

This will trigger a fresh Release workflow run.

## What Should Happen

After re-running, the workflow will:
1. ✅ Clone archivas repository
2. ✅ Update go.mod correctly
3. ✅ Verify go.mod configuration
4. ✅ Build the release version
5. ✅ Create AppImage package
6. ✅ Create .deb package
7. ✅ Upload both to the release page

## Expected Timeline

- Build: ~2-3 minutes
- Package creation: ~1-2 minutes
- Upload: ~30 seconds
- **Total: ~5 minutes**

## Verify Success

After the workflow completes:
1. Go to: https://github.com/ArchivasNetwork/archivas-linux-gui/releases/tag/v1.0.0
2. Scroll to "Assets" section
3. You should see:
   - `archivas-qt-x86_64.AppImage`
   - `archivas-qt_1.0.0_amd64.deb`

## If It Still Fails

Check the workflow logs:
1. Go to: https://github.com/ArchivasNetwork/archivas-linux-gui/actions
2. Click on the failed "Release" workflow
3. Check the logs for specific error messages
4. The verification step should catch any go.mod issues early

