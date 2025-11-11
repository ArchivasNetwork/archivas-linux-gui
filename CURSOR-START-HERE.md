# Cursor: Start Here - All-in-One Archivas Core GUI

## What We're Building

An **all-in-one desktop application** that embeds Archivas node and farmer Go code directly into a Qt GUI (forked from Bitcoin Core). This is NOT a wrapper - everything runs in one process using cgo.

## Key Architecture Points

1. **Embed Go Code**: Include Archivas Go codebase as submodule or copy
2. **Use cgo**: Bridge Go functions to C++, then to Qt
3. **Single Process**: Node and farmer run as goroutines, called directly from Qt
4. **No External Binaries**: Everything is compiled into one application

## First Steps for Cursor

### Step 1: Fork and Setup
```bash
# Clone Bitcoin Core
git clone https://github.com/bitcoin/bitcoin.git archivas-core-gui
cd archivas-core-gui

# Add Archivas Go code as submodule
git submodule add https://github.com/ArchivasNetwork/archivas.git src/go/archivas

# Initial commit
git commit -m "Initial fork from Bitcoin Core"
```

### Step 2: Create C Bridge Layer

Create `src/go/bridge/` directory with:

1. **C Headers** (`node.h`, `farmer.h`) - Define C-compatible function signatures
2. **Go Bridge Files** (`node.go`, `farmer.go`) - Use `//export` to expose Go functions to C
3. **CMake Integration** - Build Go code with cgo as part of Qt build

### Step 3: Create C++ Wrapper

Create `src/qt/archivasnodemanager.{h,cpp}`:
- Wraps C bridge functions
- Emits Qt signals for UI updates
- Handles log callbacks from Go

### Step 4: Update Build System

Modify CMakeLists.txt to:
- Find Go compiler
- Build Go bridge with cgo
- Link Qt app against Go bridge library

### Step 5: Replace Bitcoin UI

- Global rename: Bitcoin → Archivas
- Remove Bitcoin-specific features
- Wire up `ArchivasNodeManager` to UI
- Replace Overview page with Archivas data

## Critical Implementation Notes

### cgo Pattern

**Go side (`node.go`):**
```go
//export archivas_node_start
func archivas_node_start(networkID *C.char) C.int {
    // Start node in goroutine
    go func() {
        // Actual node start logic
    }()
    return 0
}
```

**C++ side (`archivasnodemanager.cpp`):**
```cpp
extern "C" {
#include "go/bridge/node.h"
}

bool ArchivasNodeManager::startNode(const QString &networkId) {
    QByteArray bytes = networkId.toUtf8();
    int result = archivas_node_start(bytes.constData());
    return result == 0;
}
```

### Logging Callbacks

Go → C function pointer → Qt signal:
1. Go code calls C callback function
2. C callback is static C++ method
3. C++ method emits Qt signal
4. Qt UI receives signal and updates display

### Threading

- **Go code**: Runs in goroutines (handled by Go runtime)
- **Qt UI**: Must stay on main thread
- **Communication**: Use callbacks/signals, never block UI thread

## What NOT to Do

❌ Don't use `QProcess` to launch external binaries  
❌ Don't try to rewrite Go code in C++  
❌ Don't call blocking Go functions from Qt UI thread  
❌ Don't forget to initialize Go runtime before calling Go functions  

## What TO Do

✅ Use cgo to bridge Go and C++  
✅ Start Go code in goroutines (non-blocking)  
✅ Use callbacks for Go → Qt communication  
✅ Build everything as one application  
✅ Test cgo integration early and often  

## Success Criteria

- Application compiles with embedded Go code
- Node can be started/stopped from Qt UI
- Farmer can be started/stopped from Qt UI
- Logs appear in Qt UI via callbacks
- Chain status updates in real-time
- Single binary/executable (no external dependencies)

## Reference

See `SPECIFICATION.md` for complete details on architecture, API design, and implementation phases.


