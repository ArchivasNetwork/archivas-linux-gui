# Fork Bitcoin Core and Rebrand into "Archivas Core" (GUI for Archivas Node + Farmer)

## Goal

We are forking the Bitcoin Core repo, but we are not building a Bitcoin client. We want to reuse the mature Qt desktop shell (menus, status bar, debug window, RPC console layout, platform integration) and replace Bitcoin-specific logic with Archivas-specific logic. The final app should be an **all-in-one desktop application** that embeds the Archivas node and farmer directly (not as external processes), displays chain status, and shows logs.

**Repo to fork from:**
- https://github.com/bitcoin/bitcoin

**Create a new repo:**
- `https://github.com/ArchivasNetwork/archivas-core-gui`

**Archivas Go code source:**
- Include Archivas Go code as a submodule or copy from `https://github.com/ArchivasNetwork/archivas`

This is an **all-in-one application** - the GUI embeds the Go node and farmer code directly using cgo.

---

## 1. Strip Bitcoin-specific Identity

In the new repo, do the following renames and removals:

### 1.1 Global Rename

- "Bitcoin Core" → "Archivas Core"
- "bitcoin-qt" → "archivas-qt"
- "Bitcoin" → "Archivas"
- "BTC" tickers and coin units → "RCHV"

### 1.2 Remove/Disable Features We Do Not Need from Bitcoin

- UTXO-specific views
- Coin control
- Mining controls for PoW
- Peers list that shows Bitcoin p2p message types
- All RPC commands that assume Bitcoin's JSON-RPC schema

### 1.3 Keep

- Qt application skeleton
- Main window layout
- Status bar (we will replace the content)
- Debug window (we will repurpose it to show logs from Archivas)
- Settings dialog

The idea is to keep the battle-tested Qt frame and swap the data sources.

---

## 2. Integration Model - Embedding Go Code Directly

**IMPORTANT:** This is an **all-in-one application**. We embed the Archivas Go code directly into the Qt application using **cgo**, not as external processes.

### 2.1 Architecture Overview

- **Qt/C++ GUI Layer**: Handles UI, user interaction, display
- **Go Code (via cgo)**: Archivas node and farmer logic embedded directly
- **C Wrappers**: Bridge between C++/Qt and Go code
- **Single Process**: Everything runs in one application

### 2.2 Include Archivas Go Code

**Option A: Git Submodule (Recommended)**
```bash
git submodule add https://github.com/ArchivasNetwork/archivas.git src/go/archivas
```

**Option B: Copy Go Code**
- Copy the entire Archivas Go codebase into `src/go/archivas/`
- This includes: `cmd/archivas-node/`, `cmd/archivas-farmer/`, `rpc/`, `ledger/`, etc.

### 2.3 Create C Wrappers for Go Functions

Create `src/go/bridge/` directory with C-compatible wrappers:

**File: `src/go/bridge/node.h`**
```c
#ifndef ARCHIVAS_NODE_BRIDGE_H
#define ARCHIVAS_NODE_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// Node lifecycle
int archivas_node_start(const char* network_id, const char* rpc_bind, const char* bootnodes);
void archivas_node_stop();
int archivas_node_is_running();

// Status queries
int archivas_node_get_height();
const char* archivas_node_get_tip_hash();
int archivas_node_get_peer_count();

// Logging callback
typedef void (*log_callback_t)(const char* level, const char* message);
void archivas_node_set_log_callback(log_callback_t callback);

#ifdef __cplusplus
}
#endif

#endif
```

**File: `src/go/bridge/node.go`**
```go
package bridge

/*
#include <stdlib.h>
#include "node.h"
*/
import "C"
import (
    "unsafe"
    "github.com/ArchivasNetwork/archivas/cmd/archivas-node"
)

var logCallback C.log_callback_t

//export archivas_node_start
func archivas_node_start(networkID, rpcBind, bootnodes *C.char) C.int {
    // Call the actual Go node start function
    // This is a simplified example - actual implementation will be more complex
    go func() {
        // Start node in goroutine
        node.Start(C.GoString(networkID), C.GoString(rpcBind), C.GoString(bootnodes))
    }()
    return 0
}

//export archivas_node_stop
func archivas_node_stop() {
    // Stop the node
    node.Stop()
}

//export archivas_node_is_running
func archivas_node_is_running() C.int {
    if node.IsRunning() {
        return 1
    }
    return 0
}

//export archivas_node_get_height
func archivas_node_get_height() C.int {
    return C.int(node.GetCurrentHeight())
}

//export archivas_node_set_log_callback
func archivas_node_set_log_callback(callback C.log_callback_t) {
    logCallback = callback
    // Wire up Go logging to C callback
}
```

**File: `src/go/bridge/farmer.h`**
```c
#ifndef ARCHIVAS_FARMER_BRIDGE_H
#define ARCHIVAS_FARMER_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// Farmer lifecycle
int archivas_farmer_start(const char* node_url, const char* plots_path, const char* farmer_privkey);
void archivas_farmer_stop();
int archivas_farmer_is_running();

// Status queries
int archivas_farmer_get_plot_count();
const char* archivas_farmer_get_last_proof();

// Logging callback
typedef void (*farmer_log_callback_t)(const char* level, const char* message);
void archivas_farmer_set_log_callback(farmer_log_callback_t callback);

#ifdef __cplusplus
}
#endif

#endif
```

### 2.4 Create C++ Wrapper Class: `ArchivasNodeManager`

Create `src/qt/archivasnodemanager.{h,cpp}`:

**Header:**
```cpp
#ifndef ARCHIVAS_NODE_MANAGER_H
#define ARCHIVAS_NODE_MANAGER_H

#include <QObject>
#include <QTimer>
#include <QString>

extern "C" {
#include "go/bridge/node.h"
#include "go/bridge/farmer.h"
}

class ArchivasNodeManager : public QObject {
    Q_OBJECT

public:
    explicit ArchivasNodeManager(QObject *parent = nullptr);
    ~ArchivasNodeManager();

    bool startNode(const QString &networkId, const QString &rpcBind, const QString &bootnodes);
    void stopNode();
    bool isNodeRunning() const;

    bool startFarmer(const QString &nodeUrl, const QString &plotsPath, const QString &farmerPrivkey);
    void stopFarmer();
    bool isFarmerRunning() const;

    // Status queries
    int getCurrentHeight() const;
    QString getTipHash() const;
    int getPeerCount() const;
    int getPlotCount() const;

signals:
    void nodeStarted();
    void nodeStopped();
    void nodeLog(const QString &level, const QString &message);
    void farmerStarted();
    void farmerStopped();
    void farmerLog(const QString &level, const QString &message);
    void statusUpdated();

private slots:
    void updateStatus();

private:
    QTimer *m_statusTimer;
    static void logCallback(const char* level, const char* message);
    static void farmerLogCallback(const char* level, const char* message);
    static ArchivasNodeManager* s_instance;
};

#endif
```

**Implementation:**
```cpp
#include "archivasnodemanager.h"
#include <QDebug>

ArchivasNodeManager* ArchivasNodeManager::s_instance = nullptr;

ArchivasNodeManager::ArchivasNodeManager(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
    
    // Set up log callbacks
    archivas_node_set_log_callback(logCallback);
    archivas_farmer_set_log_callback(farmerLogCallback);
    
    // Status update timer
    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout, this, &ArchivasNodeManager::updateStatus);
    m_statusTimer->start(5000); // Update every 5 seconds
}

bool ArchivasNodeManager::startNode(const QString &networkId, const QString &rpcBind, const QString &bootnodes)
{
    QByteArray networkIdBytes = networkId.toUtf8();
    QByteArray rpcBindBytes = rpcBind.toUtf8();
    QByteArray bootnodesBytes = bootnodes.toUtf8();
    
    int result = archivas_node_start(
        networkIdBytes.constData(),
        rpcBindBytes.constData(),
        bootnodesBytes.constData()
    );
    
    if (result == 0) {
        emit nodeStarted();
        return true;
    }
    return false;
}

void ArchivasNodeManager::stopNode()
{
    archivas_node_stop();
    emit nodeStopped();
}

bool ArchivasNodeManager::isNodeRunning() const
{
    return archivas_node_is_running() != 0;
}

int ArchivasNodeManager::getCurrentHeight() const
{
    return archivas_node_get_height();
}

void ArchivasNodeManager::logCallback(const char* level, const char* message)
{
    if (s_instance) {
        emit s_instance->nodeLog(QString::fromUtf8(level), QString::fromUtf8(message));
    }
}

void ArchivasNodeManager::updateStatus()
{
    emit statusUpdated();
}
```

### 2.5 Build System Integration

**CMakeLists.txt additions:**
```cmake
# Enable cgo
set(CGO_ENABLED 1)

# Find Go
find_program(GO_EXECUTABLE go REQUIRED)

# Add Go bridge as a library
add_subdirectory(src/go/bridge)

# Link Qt app against Go bridge
target_link_libraries(archivas-qt archivas_go_bridge)
```

**src/go/bridge/CMakeLists.txt:**
```cmake
# Build Go code with cgo
add_custom_command(
    OUTPUT archivas_bridge.a
    COMMAND ${GO_EXECUTABLE} build -buildmode=c-archive -o archivas_bridge.a .
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS node.go farmer.go
)

add_library(archivas_go_bridge STATIC IMPORTED)
set_target_properties(archivas_go_bridge PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/archivas_bridge.a
)
```

### 2.6 Key Differences from Process Manager Approach

- **Direct function calls** instead of process management
- **Shared memory** - no IPC overhead
- **Single process** - easier debugging, better performance
- **Go code runs in goroutines** - non-blocking for Qt UI
- **Logs via callbacks** - direct from Go to Qt signals
- **No external binaries needed** - everything bundled

This replaces Bitcoin's internal node with embedded Archivas Go code.

---

## 3. Replace RPC Layer

Bitcoin Core has its own JSON-RPC server and client expectations. Archivas already serves HTTP JSON at:

- `GET /chainTip`
- `GET /blocks/recent?limit=N`
- `GET /account/<addr>`
- `GET /tx/recent?limit=N`
- `POST /submit`

### 3.1 Create New Module: `ArchivasRpcClient`

Create `src/qt/archivasrpcclient.{h,cpp}`:

**Key Features:**
- Configurable base URL (default `http://127.0.0.1:8080`)
- Uses `QNetworkAccessManager` to call Archivas endpoints
- Parses JSON into simple structs
- Emits signals when data is refreshed

**Data Structures:**
```cpp
struct ChainTip {
    QString height;
    QString hash;
    QString difficulty;
    QString timestamp;
};

struct BlockInfo {
    QString height;
    QString hash;
    QString farmer;
    int txCount;
    QString timestamp;
};

struct TransactionInfo {
    QString hash;
    QString from;
    QString to;
    QString amount;
    QString fee;
    QString height;
};
```

**Key Methods:**
- `getChainTip()` - async call to `/chainTip`
- `getRecentBlocks(int limit)` - async call to `/blocks/recent?limit=N`
- `getRecentTransactions(int limit)` - async call to `/tx/recent?limit=N`
- `getAccount(const QString &address)` - async call to `/account/<addr>`
- `submitTransaction(const QByteArray &txData)` - POST to `/submit`

**Signals:**
```cpp
signals:
    void chainTipUpdated(const ChainTip &tip);
    void blocksUpdated(const QList<BlockInfo> &blocks);
    void transactionsUpdated(const QList<TransactionInfo> &txs);
    void accountUpdated(const AccountInfo &account);
    void error(const QString &message);
```

**Fallback Behavior:**
- If localhost RPC is down, fall back to `https://seed.archivas.ai` in read-only mode
- Show connection status in the UI

---

## 4. GUI Changes to Make It a Real Farmer/Node GUI

Update the main window to have these pages in the left sidebar:

### 4.1 Overview Page

**Display:**
- Chain height (from `/chainTip`)
- Difficulty
- Last block hash
- Node status: Running / Stopped (with indicator)
- Farmer status: Running / Stopped (with indicator)
- Network: archivas-devnet-v4 / archivas-mainnet
- RPC connection status

**Auto-refresh:** Every 5 seconds

### 4.2 Node Page

**Controls:**
- Buttons: "Start Node", "Stop Node", "Restart Node"
- Status indicator (green/red)
- Network selection: archivas-devnet-v4 / archivas-mainnet
- RPC bind address (editable, default: 127.0.0.1:8080)
- Bootnodes (editable, default: seed.archivas.ai:9090)
- Show last 50 lines of node log in a scrollable text area
- Show peer count (from embedded node)
- Show sync status (synced / syncing / behind)

**Log Display:**
- Real-time log output from embedded Go node (via callbacks)
- Color-coded (info/warn/error)
- Auto-scroll to bottom option

### 4.3 Farmer Page

**Controls:**
- Buttons: "Start Farmer", "Stop Farmer", "Restart Farmer"
- Status indicator (green/red)
- Node URL (editable, default: http://127.0.0.1:8080)
- Show plots path (editable)
- Show farmer private key path (masked, editable)
- Show last qualities submitted
- Show last successful proof
- Show total plots loaded (from embedded farmer)
- Show total plot size (TB)

**Log Display:**
- Real-time log output from embedded Go farmer (via callbacks)
- Color-coded (info/warn/error)
- Highlight winning proofs

### 4.4 Blocks Page

**Table View:**
- Bound to `/blocks/recent?limit=20`
- Columns:
  - Height
  - Hash (truncated, clickable to copy)
  - Farmer Address
  - Transaction Count
  - Timestamp
  - Difficulty
- Auto-refresh every 10 seconds
- Double-click row to show block details (future)

### 4.5 Transactions Page

**Table View:**
- Bound to `/tx/recent?limit=50`
- Columns:
  - Hash (truncated, clickable to copy)
  - From Address
  - To Address
  - Amount (RCHV)
  - Fee (RCHV)
  - Height
  - Timestamp
- Filter by address (optional)
- Auto-refresh every 10 seconds
- Double-click row to show transaction details (future)

### 4.6 Logs Page

**Two Tabs:**
1. **Node Logs** - QPlainTextEdit bound to node process output
2. **Farmer Logs** - QPlainTextEdit bound to farmer process output

**Features:**
- Clear button for each tab
- Save logs to file
- Search/filter
- Timestamp toggle
- Auto-scroll toggle

This makes it clearly an Archivas app and not a Bitcoin wallet.

---

## 5. Remove Bitcoin Consensus/Wallet Internals from the Build

In `src/` of Bitcoin Core there are a lot of consensus and wallet files. For this fork:

- **Keep:**
  - Qt app (`src/qt/`)
  - Utility libraries (logging, args, fs)
  - Platform integration code

- **Remove/Stub:**
  - Bitcoin validation (`src/consensus/`, `src/validation/`)
  - Bitcoin wallet (`src/wallet/`)
  - Bitcoin P2P (`src/net_processing.cpp`, etc.)
  - Bitcoin RPC server (we use external node)

- **CMake/qmake Changes:**
  - Adjust build system to only compile GUI app
  - Include new `ArchivasProcessManager` and `ArchivasRpcClient`
  - Remove Bitcoin-specific dependencies where possible
  - Keep Qt dependencies

The goal is not to ship Bitcoin's consensus. The goal is to ship a desktop controller for Archivas.

---

## 6. Config and Paths

Add a simple config file under the user's home directory:

**Paths:**
- Linux: `~/.archivas-core/config.json`
- Windows: `%APPDATA%\ArchivasCore\config.json`
- macOS: `~/Library/Application Support/ArchivasCore/config.json`

**Config File Structure:**
```json
{
  "node": {
    "network": "archivas-devnet-v4",
    "rpc_bind": "127.0.0.1:8080",
    "data_dir": "/home/user/.archivas/data",
    "bootnodes": "seed.archivas.ai:9090",
    "auto_start": false
  },
  "farmer": {
    "plots_path": "/home/user/archivas/plots",
    "farmer_privkey_path": "/home/user/.archivas/farmer.key",
    "node_url": "http://127.0.0.1:8080",
    "auto_start": false
  },
  "rpc": {
    "url": "http://127.0.0.1:8080",
    "fallback_url": "https://seed.archivas.ai",
    "poll_interval_ms": 5000
  },
  "ui": {
    "theme": "dark",
    "language": "en",
    "minimize_to_tray": true
  }
}
```

**Settings Dialog:**
- Allow user to edit all configuration options
- Validate paths exist before saving (plots_path, data_dir, farmer_privkey_path)
- Show file picker for directories and key files
- Mask private key path in UI (show as `****`)
- No executable paths needed (Go code is embedded)

---

## 7. Build Targets

Rename build targets from:

- `bitcoin-qt` → `archivas-qt`
- `bitcoin-cli` → **remove** (not needed)
- `bitcoind` → **remove** (node is external, in Go)

We only need to build the Qt desktop app here.

**Build Output:**
- Linux: `archivas-qt` binary
- Windows: `archivas-qt.exe`
- macOS: `Archivas Core.app`

---

## 8. Platform-Specific Considerations

### 8.1 Linux

- Systemd integration (optional): Create `.desktop` file for launcher
- AppIndicator support for system tray
- Use `QStandardPaths` for config location

### 8.2 Windows

- Windows installer (NSIS or WiX)
- Add to Start Menu
- System tray icon
- Windows service integration (optional, for auto-start)

### 8.3 macOS

- Bundle structure: `Archivas Core.app/Contents/`
- Code signing (for distribution)
- macOS menu bar integration
- Use `QStandardPaths` for config location

---

## 9. Implementation Phases

### Phase 1: Foundation
1. Fork Bitcoin Core repo
2. Global rename (Bitcoin → Archivas)
3. Remove Bitcoin-specific UI elements
4. Add Archivas Go code (submodule or copy)
5. Create C bridge layer (`src/go/bridge/`)
6. Create `ArchivasNodeManager` C++ wrapper
7. Create `ArchivasRpcClient` skeleton
8. Set up cgo build system

### Phase 2: Core Functionality
1. Implement C bridge functions (node start/stop, farmer start/stop)
2. Wire up Go code to C++ via cgo
3. Implement RPC client (chainTip, blocks, transactions)
4. Update Overview page with real data
5. Add Node and Farmer pages with controls
6. Wire up log callbacks from Go to Qt signals

### Phase 3: Polish
1. Add Blocks and Transactions tables
2. Add Logs page with real-time output
3. Implement config file loading/saving
4. Add Settings dialog
5. Error handling and user feedback

### Phase 4: Distribution
1. Build system cleanup (remove Bitcoin dependencies)
2. Platform-specific packaging
3. Documentation
4. Release builds

---

## 10. Deliverables

1. **New repo initialized from Bitcoin Core**
   - All "Bitcoin" strings renamed to "Archivas"
   - Build system updated

2. **New Classes:**
   - `ArchivasNodeManager` (C++ wrapper for embedded Go node/farmer)
   - `ArchivasRpcClient` (poll Archivas HTTP API)
   - C bridge layer (`src/go/bridge/`) for Go ↔ C++ interop

3. **Updated Main Window:**
   - Overview page with chain status
   - Node page with controls and logs
   - Farmer page with controls and logs
   - Blocks table
   - Transactions table
   - Logs viewer

4. **Config File Support:**
   - JSON config with all paths
   - Settings dialog for editing

5. **Build Instructions:**
   - Linux (CMake/qmake)
   - macOS (Xcode/CMake)
   - Windows (Visual Studio/CMake)

6. **Documentation:**
   - README with setup instructions
   - User guide for GUI features
   - Developer guide for extending

---

## 11. Getting Started with Cursor

Tell Cursor to start by:

1. **Creating the new repo structure**
   - Fork/clone Bitcoin Core
   - Create new repo `archivas-core-gui`
   - Add Archivas Go code as submodule: `git submodule add https://github.com/ArchivasNetwork/archivas.git src/go/archivas`
   - Initial commit with Bitcoin Core codebase

2. **Setting up cgo integration**
   - Create `src/go/bridge/` directory
   - Create C header files (`node.h`, `farmer.h`)
   - Create Go bridge files (`node.go`, `farmer.go`) with `//export` functions
   - Set up CMake to build Go code with cgo

3. **Creating C++ wrapper and renaming Qt app**
   - Global find/replace: Bitcoin → Archivas
   - Create `ArchivasNodeManager` class that calls C bridge functions
   - Wire up basic start/stop functionality (calls Go code directly)
   - Wire up log callbacks from Go to Qt signals

4. **Adding the Archivas RPC client in Qt**
   - Create `ArchivasRpcClient` class
   - Implement `getChainTip()` first
   - Can use embedded node's RPC or fallback to seed.archivas.ai

5. **Replacing the overview page with Archivas data**
   - Remove Bitcoin balance/UTXO displays
   - Add chain height, difficulty, node/farmer status (from embedded code)
   - Wire up auto-refresh
   - Show logs from embedded Go code via callbacks

**Key Point:** Everything runs in one process. The Go node and farmer run as goroutines, and the Qt UI calls them directly via C wrappers. No external binaries needed.

That will get you a true all-in-one farmer/node GUI on top of a forked Bitcoin Core UI.

---

## 12. Technical Notes

### 12.1 Go Integration via cgo

- Use cgo to call Go functions from C++
- Go code runs in goroutines (non-blocking for Qt UI thread)
- Use `//export` functions in Go to expose C-compatible API
- Handle Go panics gracefully (recover in Go, return error codes to C++)
- Use callbacks for logging (Go → C function pointer → Qt signal)
- Ensure Go runtime is initialized before calling any Go functions

### 12.2 RPC Client

- Use `QNetworkAccessManager` for async HTTP requests
- Implement timeout handling (default 5 seconds)
- Parse JSON with `QJsonDocument` and `QJsonObject`
- Handle network errors and show user-friendly messages

### 12.3 Threading

- RPC calls should be async (non-blocking UI)
- Go code runs in goroutines (handled by Go runtime)
- Qt UI thread must not call blocking Go functions directly
- Use callbacks/signals for Go → Qt communication
- Use Qt's signal/slot mechanism for thread-safe updates
- Consider using `QThread` for any long-running C++ operations that call Go

### 12.4 Error Handling

- Validate all user inputs (paths, URLs)
- Show clear error messages
- Log errors to file for debugging
- Graceful degradation (fallback to seed node if local fails)

---

## 13. Future Enhancements

- Plot management UI (list plots, verify integrity)
- Wallet integration (send/receive RCHV)
- Transaction builder (create and sign transactions)
- Network statistics dashboard
- Farmer pool integration (if pools are implemented)
- Dark/light theme toggle
- Multi-language support
- Auto-update mechanism

---

## 14. License and Attribution

- Bitcoin Core is licensed under MIT
- Keep original license and attribution
- Add `THIRD_PARTY.md` documenting the fork
- Clearly state this is a fork/derivative work

---

**End of Specification**

