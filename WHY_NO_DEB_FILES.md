# Why .deb Files Are Missing

## The Problem

The **build workflow** (build.yml) runs on every push and builds the application, but it does **NOT** create `.deb` files.

The **Release workflow** (release.yml) is what creates the `.deb` and AppImage packages, but it only runs when:
1. A release is **created**
2. A release is **edited**
3. Manually triggered via `workflow_dispatch`

## Current Status

✅ **Build workflow** - Working (runs on every push)
❌ **Release workflow** - Hasn't been triggered since we fixed it

## Solution: Trigger the Release Workflow

### Option 1: Edit and Re-save the Release (Recommended) ⭐

1. Go to: https://github.com/ArchivasNetwork/archivas-linux-gui/releases/tag/v1.0.0
2. Click the **"Edit"** button (pencil icon, top right)
3. Click **"Update release"** (without changing anything)
4. Wait 5-7 minutes for the workflow to complete
5. Check the **"Assets"** section for the `.deb` file

### Option 2: Manually Trigger the Workflow

1. Go to: https://github.com/ArchivasNetwork/archivas-linux-gui/actions/workflows/release.yml
2. Click **"Run workflow"** button (top right, next to "Filter workflow runs")
3. Select branch: `main`
4. Version: `v1.0.0` (or leave as default)
5. Click **"Run workflow"**

### Option 3: Re-run the Failed Workflow

1. Go to: https://github.com/ArchivasNetwork/archivas-linux-gui/actions
2. Click on **"Release"** in the left sidebar
3. Find the failed workflow run for v1.0.0
4. Click on it
5. Click **"Re-run jobs"** → **"Re-run failed jobs"**

## What Will Happen

After triggering, the Release workflow will:
1. ✅ Clone archivas repository to `/tmp/archivas`
2. ✅ Update `go.mod` to use `/tmp/archivas` (remove old paths)
3. ✅ Verify `go.mod` is correctly configured
4. ✅ Build the release version
5. ✅ Create `archivas-qt_1.0.0_amd64.deb`
6. ✅ Create `archivas-qt-x86_64.AppImage` (optional)
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
- Filter by: "Release" workflow (not "Build and Test")

## Key Difference

- **Build workflow** = Tests that code compiles (what you're seeing succeed)
- **Release workflow** = Creates packages and uploads to release (what creates .deb files)

The Release workflow needs to be triggered manually since the release was already created before we fixed the build issues.

