# CI Build Fix Applied

## Problem Identified

The GitHub Actions workflows were failing because:
- `go.mod` has a `replace` directive pointing to `/home/iljanemesis/archivas`
- This local path doesn't exist in the CI environment
- Go build fails when trying to resolve the dependency

## Solution Applied

Updated both workflow files to:
1. Clone the `archivas` repository from GitHub during CI
2. Update the `go.mod` replace directive to point to the cloned location
3. This allows the build to work in CI while keeping local development working

## Files Changed

- `.github/workflows/release.yml` - Added step to clone archivas repo
- `.github/workflows/build.yml` - Added step to clone archivas repo

## Next Steps

1. **Push the fix** (if not already pushed):
   ```bash
   git push origin main
   ```

2. **Re-run the failed workflow**:
   - Go to: https://github.com/ArchivasNetwork/archivas-linux-gui/actions
   - Find the failed "Release" workflow
   - Click "Re-run jobs" → "Re-run failed jobs"

3. **Or create a new release**:
   - The fix will automatically apply to new releases
   - Existing release can be re-triggered by editing and saving it

## What to Expect

After the fix:
- ✅ Build should complete successfully
- ✅ AppImage and .deb packages will be created
- ✅ Assets will appear on the release page

## Monitoring

Watch the build progress at:
- Actions: https://github.com/ArchivasNetwork/archivas-linux-gui/actions
- Release: https://github.com/ArchivasNetwork/archivas-linux-gui/releases/tag/v1.0.0

