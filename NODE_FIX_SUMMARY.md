# Node Configuration Fix - Forked Chain Filtering

## Problem
The node was connecting to multiple peers, including 72.251.11.191:9090 (Server B) which is on a forked chain. This caused:
- Constant "prev hash mismatch" errors
- Node stuck at height 10752
- Unable to sync to correct chain (~480k blocks)
- Spam from forked chain peers

## Solution

### 1. Bootnode Filtering ✅
- **File**: `src/go/bridge/node.go`
- Only `seed.archivas.ai:9090` is allowed as bootnode
- Other bootnodes are filtered out with warning
- Default config set to only `seed.archivas.ai:9090`

### 2. IBD Filtering ✅
- **File**: `src/go/bridge/node.go` (lines 455-496)
- Only uses `seed.archivas.ai` for Initial Block Download
- Other peers are ignored for IBD
- Clear logging when IBD starts

### 3. Peer Filtering ✅
- **File**: `src/go/bridge/node.go` (lines 685-735)
- Non-seed peers with height > 200k are banned (likely forked chain)
- Banned peers are tracked in `bannedPeers` map
- Banned peers are ignored for block announcements
- Only `seed.archivas.ai` can trigger IBD

### 4. Block Validation ✅
- **File**: `src/go/bridge/node.go` (lines 960-1010, 1138-1145)
- Prev hash mismatch errors are clearer
- Genesis hash is verified for first block
- Blocks from forked chains are rejected in both `VerifyAndApplyBlock` and `ApplyBlock`

### 5. Gossip Disabled ✅
- **File**: `src/go/bridge/node.go` (lines 407-413)
- Gossip is disabled to prevent discovering wrong-chain peers
- Peer count limited to 10
- Dial frequency reduced to 2 per minute

### 6. Config Default ✅
- **File**: `src/qt/configmanager.cpp` (line 54)
- Default bootnodes set to only `seed.archivas.ai:9090`

## Testing

### If Node is Stuck on Forked Chain

If the node is stuck at height 10752 with prev hash mismatches, it may be on a forked chain. To fix:

1. **Stop the node** (if running)

2. **Clear the database and resync from scratch**:
   ```bash
   # Backup current data (optional)
   mv ~/.archivas/data ~/.archivas/data.backup
   
   # Clear peer store
   rm -f ~/.archivas/data/peers.json
   
   # Or clear entire data directory to resync from scratch
   rm -rf ~/.archivas/data
   ```

3. **Restart the application**:
   ```bash
   ./build/archivas-qt
   ```

4. **Verify**:
   - Node should only connect to `seed.archivas.ai:9090`
   - IBD should start from seed.archivas.ai
   - No connections to 72.251.11.191 or other wrong-chain peers
   - Height should sync toward ~480k

### Expected Behavior

- **Bootnode Connection**: Only `seed.archivas.ai:9090`
- **IBD Source**: Only `seed.archivas.ai`
- **Peer Filtering**: Non-seed peers with height > 200k are banned
- **Block Validation**: Prev hash mismatches are rejected
- **Gossip**: Disabled to prevent wrong-chain peer discovery

### Log Messages

You should see:
- `"Filtered out bootnode X (only seed.archivas.ai allowed)"` - when filtering bootnodes
- `"Using https://seed.archivas.ai for IBD"` - when starting IBD
- `"Ignoring block announcement from non-seed peer X (height Y, likely forked chain)"` - when banning peers
- `"Starting IBD from seed"` - when IBD starts from seed
- `"prev hash mismatch (likely forked chain)"` - when rejecting forked blocks

## Files Modified

1. `src/go/bridge/node.go`:
   - Added `bannedPeers` and `bannedPeersMutex` globals
   - Filtered bootnodes to only seed.archivas.ai
   - Filtered IBD to only use seed.archivas.ai
   - Added peer filtering in `OnNewBlock`
   - Enhanced block validation in `VerifyAndApplyBlock` and `ApplyBlock`
   - Disabled gossip

2. `src/qt/configmanager.cpp`:
   - Set default bootnodes to only `seed.archivas.ai:9090`

## Status

✅ **Build Successful**: Application compiles with all fixes
✅ **Ready to Test**: Application is ready to run with forked chain filtering

## Next Steps

1. Clear corrupted database (if node is stuck)
2. Restart application
3. Verify only seed.archivas.ai connections
4. Monitor IBD progress
5. Verify height syncs toward ~480k

