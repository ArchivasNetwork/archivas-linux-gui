# How to Trigger the Release Workflow to Generate .deb Files

## The Problem

The Release workflow only runs automatically when you **create** or **edit** a release. Since we've fixed the workflow after the release was created, we need to manually trigger it again.

## Solution: Trigger the Release Workflow

### Option 1: Edit and Re-save the Release (Easiest) ⭐

1. Go to: https://github.com/ArchivasNetwork/archivas-linux-gui/releases/tag/v1.0.0
2. Click the **"Edit"** button (pencil icon, top right)
3. Click **"Update release"** (without changing anything)

This will trigger the Release workflow again with all our fixes, and it should successfully create the .deb and AppImage packages.

### Option 2: Manually Trigger the Workflow

1. Go to: https://github.com/ArchivasNetwork/archivas-linux-gui/actions/workflows/release.yml
2. Click **"Run workflow"** (top right, next to "Filter workflow runs")
3. Select branch: `main`
4. Version: `v1.0.0` (or leave as default)
5. Click **"Run workflow"**

This will manually trigger the Release workflow.

### Option 3: Re-run the Failed Workflow

1. Go to: https://github.com/ArchivasNetwork/archivas-linux-gui/actions
2. Click on **"Release"** in the left sidebar (under workflow names)
3. Find the failed workflow run for v1.0.0
4. Click on it
5. Click **"Re-run jobs"** → **"Re-run failed jobs"**

## What Will Happen

After triggering, the workflow will:
1. ✅ Clone archivas repository to `/tmp/archivas`
2. ✅ Update `go.mod` to use `/tmp/archivas` (remove old paths)
3. ✅ Verify `go.mod` is correctly configured
4. ✅ Build the release version
5. ✅ Create `archivas-qt-x86_64.AppImage`
6. ✅ Create `archivas-qt_1.0.0_amd64.deb`
7. ✅ Upload both to the release page

## Expected Timeline

- Setup: ~1 minute
- Build: ~2-3 minutes
- Package creation: ~1-2 minutes
- Upload: ~30 seconds
- **Total: ~5-7 minutes**

## Verify Success

After the workflow completes:
1. Go to: https://github.com/ArchivasNetwork/archivas-linux-gui/releases/tag/v1.0.0
2. Scroll to **"Assets"** section
3. You should see:
   - ✅ `archivas-qt-x86_64.AppImage`
   - ✅ `archivas-qt_1.0.0_amd64.deb`

## Monitor Progress

Watch the workflow run at:
- Actions: https://github.com/ArchivasNetwork/archivas-linux-gui/actions
- Filter by: "Release" workflow

## If It Still Fails

Check the workflow logs for specific errors. The verification steps we added should catch issues early and provide clear error messages.

## Quick Action

**Just edit and re-save the release** - that's the quickest way to trigger the workflow!

1. https://github.com/ArchivasNetwork/archivas-linux-gui/releases/tag/v1.0.0
2. Click "Edit"
3. Click "Update release"
4. Wait ~5-7 minutes
5. Check "Assets" section for the .deb file!

