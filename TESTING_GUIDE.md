# Testing Guide for Archivas Core GUI

## Build Status

✅ **Build Successful**: The application compiles successfully with real Archivas Go code integrated.

## Running the Application

```bash
cd build
./archivas-qt
```

## Expected Behavior

### On First Launch

1. **Welcome Message**: A welcome dialog appears after 2 seconds
2. **Directory Creation**: 
   - `~/.archivas/data` - Node database
   - `~/.archivas/plots` - Farmer plots directory
   - `~/.archivas/farmer.key` - Farmer private key (auto-generated)
3. **Auto-Start**:
   - Node starts automatically (after 500ms delay)
   - Farmer starts automatically (after 1.5s delay, waits for node)
4. **Logs Appear**:
   - Node logs in Node page
   - Farmer logs in Farmer page
   - Real Archivas activity (not placeholders)

### Node Behavior

- **Initialization**: Opens database, creates/loads genesis block
- **P2P Network**: Connects to bootnodes (seed.archivas.ai:9090)
- **RPC Server**: Starts on 127.0.0.1:8080
- **IBD**: Initial Block Download from peers
- **Logs**: Real node activity appears in logs

### Farmer Behavior

- **Key Generation**: Auto-generates farmer key if doesn't exist
- **Plot Loading**: Scans ~/.archivas/plots for .arcv files
- **Farming Loop**: Queries node for challenges every 2 seconds
- **Proof Submission**: Submits blocks when winning proofs found
- **Logs**: Real farmer activity appears in logs

## Verifying Real Functionality

### Check Node Status

1. **Node Page**:
   - Status should show "Running" (green)
   - Peer count should increase as connections are made
   - Logs should show real node activity

2. **Overview Page**:
   - Chain Height should update (starts at 0, increases as blocks are mined)
   - Tip Hash should show real block hash
   - Node Status should show "Running"
   - Peer count should show connected peers

### Check Farmer Status

1. **Farmer Page**:
   - Status should show "Running" (green)
   - Plot count should show number of loaded plots
   - Logs should show farming activity

2. **Overview Page**:
   - Farmer Status should show "Running"
   - Last proof information (if any proofs found)

## Troubleshooting

### Node Doesn't Start

- Check logs in Node page for errors
- Verify data directory is writable
- Check if port 8080 is available
- Verify bootnodes are reachable

### Farmer Doesn't Start

- Check logs in Farmer page for errors
- Verify plots directory is writable
- Check if farmer key was generated
- Verify node RPC is accessible (http://127.0.0.1:8080)

### No Logs Appear

- Verify callbacks are set up correctly
- Check if Go runtime is initialized
- Verify thread-safe signal emission

### App Crashes

- Check for nil pointer dereferences
- Verify Go runtime initialization
- Check for threading issues
- Look for error messages in terminal

## Testing Checklist

- [ ] Application launches without crashes
- [ ] Welcome message appears on first run
- [ ] Directories are created automatically
- [ ] Node starts automatically
- [ ] Farmer starts automatically
- [ ] Node logs appear in Node page
- [ ] Farmer logs appear in Farmer page
- [ ] Node connects to network (peer count > 0)
- [ ] Chain height updates in Overview page
- [ ] Tip hash shows real block hash
- [ ] Farmer loads plots (if any exist)
- [ ] Real Archivas activity in logs (not placeholders)

## Next Steps

1. **Generate Plots**: Use archivas-farmer plot to create plot files
2. **Test Farming**: Verify farmer finds and submits proofs
3. **Test Networking**: Verify node syncs with network
4. **Test RPC**: Verify RPC endpoints work correctly
5. **Performance**: Monitor resource usage and optimize if needed

