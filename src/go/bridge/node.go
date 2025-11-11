package main

/*
#cgo CFLAGS: -I${SRCDIR}
#include <stdlib.h>
#include "node.h"

// Helper function to call the callback (needed because cgo can't call function pointers directly)
static void call_log_callback(log_callback_t cb, char* level, char* message) {
    if (cb != NULL) {
        cb(level, message);
    }
}
*/
import "C"
import (
	"context"
	"crypto/sha256"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"time"
	"unsafe"

	"github.com/ArchivasNetwork/archivas/config"
	"github.com/ArchivasNetwork/archivas/consensus"
	"github.com/ArchivasNetwork/archivas/health"
	"github.com/ArchivasNetwork/archivas/ledger"
	"github.com/ArchivasNetwork/archivas/mempool"
	"github.com/ArchivasNetwork/archivas/metrics"
	"github.com/ArchivasNetwork/archivas/node"
	"github.com/ArchivasNetwork/archivas/p2p"
	"github.com/ArchivasNetwork/archivas/pospace"
	"github.com/ArchivasNetwork/archivas/rpc"
	"github.com/ArchivasNetwork/archivas/storage"
)

// Block represents a blockchain block with Proof-of-Space (copied from archivas-node/main.go)
type Block struct {
	Height         uint64
	TimestampUnix  int64
	PrevHash       [32]byte
	Difficulty     uint64
	Challenge      [32]byte
	Txs            []ledger.Transaction
	Proof          *pospace.Proof
	FarmerAddr     string
	CumulativeWork uint64
}

// NodeState holds the entire node state (copied from archivas-node/main.go)
type NodeState struct {
	sync.RWMutex
	Chain            []Block
	WorldState       *ledger.WorldState
	Mempool          *mempool.Mempool
	Consensus        *consensus.Consensus
	CurrentHeight    uint64
	CurrentChallenge [32]byte
	DB               *storage.DB
	BlockStore       *storage.BlockStorage
	StateStore       *storage.StateStorage
	MetaStore        *storage.MetadataStorage
	P2P              *p2p.Network
	GenesisHash      [32]byte
	NetworkID        string
	Health           *health.ChainHealth
	ReorgDetector    *consensus.ReorgDetector
	VDFSeed          []byte
	VDFIterations    uint64
	VDFOutput        []byte
	HasVDF           bool
	DataDir          string // Store dataDir for fork recovery
}

// Global state
var (
	nodeRunning      bool
	nodeMutex        sync.RWMutex
	nodeContext      context.Context
	nodeCancel       context.CancelFunc
	nodeState        *NodeState
	nodeStateMutex   sync.RWMutex
	logCallback      C.log_callback_t
	logCallbackMutex sync.RWMutex

	// Track peers that are on wrong chain (banned from block requests)
	bannedPeers      map[string]bool
	bannedPeersMutex sync.RWMutex
	bannedPeersFile  string // Path to banned peers file

	// Track invalid block attempts from peers (for banning)
	peerInvalidBlocks      map[string]int
	peerInvalidBlocksMutex sync.RWMutex

	// IBD progress tracking
	ibdLastAppliedHeight uint64
	ibdLastAppliedTime   time.Time
	ibdAppliedBlocks     uint64 // Count of blocks applied in current session
	ibdProgressMutex     sync.RWMutex
	ibdRunning           bool // Track if IBD is currently running
	ibdRunningMutex      sync.RWMutex

	// Network difficulty cache (from seed.archivas.ai)
	networkDifficulty      uint64
	networkDifficultyTime  time.Time
	networkDifficultyMutex sync.RWMutex

	// Genesis path storage (for fork recovery)
	currentGenesisPath string
	genesisPathMutex   sync.RWMutex

	// Network hash cache (for blocks without proof - stores network's hash)
	networkBlockHashes map[uint64][32]byte // height -> network hash
	networkHashMutex   sync.RWMutex
)

// hashBlock calculates the hash of a block (copied from archivas-node/main.go)
func hashBlock(b *Block) [32]byte {
	h := sha256.New()
	fmt.Fprintf(h, "%d", b.Height)
	fmt.Fprintf(h, "%d", b.TimestampUnix)
	h.Write(b.PrevHash[:])
	fmt.Fprintf(h, "%d", b.Difficulty)
	h.Write(b.Challenge[:])
	if b.Proof != nil {
		h.Write(b.Proof.Hash[:])
	}
	return sha256.Sum256(h.Sum(nil))
}

// recoverFromFork clears the database and reinitializes from genesis without stopping the node
func recoverFromFork(dataDirPath string, genesisPath string, networkID string, rpcBindAddr string) error {
	callLogCallback("INFO", "Fork detected, clearing database and resyncing from genesis...")

	// Step 1: Close database
	nodeStateMutex.Lock()
	var dbToClose *storage.DB
	if nodeState != nil && nodeState.DB != nil {
		dbToClose = nodeState.DB
	}
	nodeStateMutex.Unlock()

	if dbToClose != nil {
		callLogCallback("INFO", "Fork recovery: Closing database connection...")
		dbToClose.Close()
	}

	// Step 2: Remove database directory
	callLogCallback("INFO", fmt.Sprintf("Fork recovery: Removing forked chain database: %s", dataDirPath))
	if err := os.RemoveAll(dataDirPath); err != nil {
		return fmt.Errorf("failed to remove database: %w", err)
	}

	// Step 3: Recreate data directory
	if err := os.MkdirAll(dataDirPath, 0755); err != nil {
		return fmt.Errorf("failed to recreate data directory: %w", err)
	}

	// Step 4: Reopen database
	callLogCallback("INFO", "Fork recovery: Reopening database...")
	db, err := storage.OpenDB(dataDirPath)
	if err != nil {
		return fmt.Errorf("failed to reopen database: %w", err)
	}
	blockStore := storage.NewBlockStorage(db)
	stateStore := storage.NewStateStorage(db)
	metaStore := storage.NewMetadataStorage(db)

	// Step 5: Load genesis file
	gen, err := config.LoadGenesis(genesisPath)
	if err != nil {
		// Try fallback paths
		possiblePaths := []string{
			"genesis/devnet.genesis.json",
			"../archivas/genesis/devnet.genesis.json",
			"../../archivas/genesis/devnet.genesis.json",
		}
		gen = nil
		for _, path := range possiblePaths {
			if g, err := config.LoadGenesis(path); err == nil {
				gen = g
				genesisPath = path
				break
			}
		}
		if gen == nil {
			return fmt.Errorf("failed to load genesis file: %w", err)
		}
	}

	// Step 6: Create genesis block
	genesisAllocs := config.GenesisAllocToMap(gen.Allocations)
	genesisChallenge := consensus.GenerateGenesisChallenge()
	genesisBlockDifficulty := uint64(1125899906842624) // 2^50

	genesisBlock := Block{
		Height:         0,
		TimestampUnix:  gen.Timestamp,
		PrevHash:       [32]byte{},
		Difficulty:     genesisBlockDifficulty,
		Challenge:      genesisChallenge,
		Txs:            nil,
		Proof:          nil,
		FarmerAddr:     "",
		CumulativeWork: consensus.CalculateWork(genesisBlockDifficulty),
	}

	calculatedGenesisHash := hashBlock(&genesisBlock)

	// Step 7: Initialize chain state
	worldState := ledger.NewWorldState(genesisAllocs)
	cs := consensus.NewConsensus()
	// Set initial difficulty from genesis file (not from genesis block difficulty)
	// Genesis block difficulty is 2^50, but consensus starts at gen.InitialDifficulty
	cs.DifficultyTarget = gen.InitialDifficulty
	chain := []Block{genesisBlock}
	currentHeight := uint64(0)
	genesisHash := calculatedGenesisHash

	// Step 8: Persist genesis block and state
	if err := blockStore.SaveBlock(0, genesisBlock); err != nil {
		return fmt.Errorf("failed to save genesis block: %w", err)
	}
	for addr, balance := range genesisAllocs {
		if err := stateStore.SaveAccount(addr, balance, 0); err != nil {
			return fmt.Errorf("failed to save genesis account %s: %w", addr, err)
		}
	}
	if err := metaStore.SaveTipHeight(0); err != nil {
		return fmt.Errorf("failed to save tip height: %w", err)
	}
	if err := metaStore.SaveDifficulty(cs.DifficultyTarget); err != nil {
		return fmt.Errorf("failed to save difficulty: %w", err)
	}
	if err := metaStore.SaveGenesisHash(genesisHash); err != nil {
		return fmt.Errorf("failed to save genesis hash: %w", err)
	}
	if err := metaStore.SaveNetworkID(networkID); err != nil {
		return fmt.Errorf("failed to save network ID: %w", err)
	}

	callLogCallback("INFO", fmt.Sprintf("Fork recovery: Genesis block saved (height=0, hash=%x)", genesisHash[:8]))
	callLogCallback("INFO", fmt.Sprintf("Fork recovery: Initialized %d genesis accounts", len(gen.Allocations)))

	// Step 9: Update nodeState atomically
	nodeStateMutex.Lock()
	if nodeState != nil {
		// Update existing nodeState
		nodeState.Lock()
		nodeState.Chain = chain
		nodeState.WorldState = worldState
		nodeState.Consensus = cs
		nodeState.CurrentHeight = currentHeight
		nodeState.GenesisHash = genesisHash
		nodeState.CurrentChallenge = genesisChallenge
		nodeState.DB = db
		nodeState.BlockStore = blockStore
		nodeState.StateStore = stateStore
		nodeState.MetaStore = metaStore
		nodeState.DataDir = dataDirPath
		// Reset health and reorg detector for fresh chain
		if nodeState.Health == nil {
			nodeState.Health = health.NewChainHealth()
		}
		if nodeState.ReorgDetector == nil {
			nodeState.ReorgDetector = consensus.NewReorgDetector()
		}
		nodeState.Unlock()
	} else {
		// Create new nodeState (shouldn't happen, but handle it)
		nodeState = &NodeState{
			Chain:            chain,
			WorldState:       worldState,
			Mempool:          mempool.NewMempool(),
			Consensus:        cs,
			CurrentHeight:    currentHeight,
			CurrentChallenge: genesisChallenge,
			DB:               db,
			BlockStore:       blockStore,
			StateStore:       stateStore,
			MetaStore:        metaStore,
			Health:           health.NewChainHealth(),
			ReorgDetector:    consensus.NewReorgDetector(),
			GenesisHash:      genesisHash,
			NetworkID:        networkID,
			DataDir:          dataDirPath,
		}
	}
	nodeStateMutex.Unlock()

	// Step 10: Reset IBD tracking
	ibdRunningMutex.Lock()
	ibdRunning = false
	ibdRunningMutex.Unlock()

	ibdProgressMutex.Lock()
	ibdLastAppliedHeight = 0
	ibdLastAppliedTime = time.Now()
	ibdAppliedBlocks = 0
	ibdProgressMutex.Unlock()

	callLogCallback("INFO", "Fork recovery: Database cleared and reinitialized from genesis")
	callLogCallback("INFO", "Fork recovery: IBD will restart automatically from height 0")

	return nil
}

// callLogCallback safely calls the C callback function
func callLogCallback(level, message string) {
	logCallbackMutex.RLock()
	cb := logCallback
	logCallbackMutex.RUnlock()

	if cb != nil && uintptr(unsafe.Pointer(cb)) != 0 {
		levelCStr := C.CString(level)
		messageCStr := C.CString(message)
		defer C.free(unsafe.Pointer(levelCStr))
		defer C.free(unsafe.Pointer(messageCStr))
		C.call_log_callback(cb, levelCStr, messageCStr)
	} else {
		log.Printf("[%s] %s", level, message)
	}
}

// logWriter wraps callLogCallback to implement io.Writer for redirecting std logs
type logWriter struct {
	level string
}

func (w logWriter) Write(p []byte) (n int, err error) {
	callLogCallback(w.level, strings.TrimSpace(string(p)))
	return len(p), nil
}

//export archivas_node_start
func archivas_node_start(networkID, rpcBind, dataDir, bootnodes, genesisPath *C.char) C.int {
	nodeMutex.Lock()
	defer nodeMutex.Unlock()

	if nodeRunning {
		return 1 // Already running
	}

	// Banned peers are loaded in startArchivasNode from disk
	// No need to initialize here - it's done there

	peerInvalidBlocksMutex.Lock()
	if peerInvalidBlocks == nil {
		peerInvalidBlocks = make(map[string]int)
	}
	peerInvalidBlocksMutex.Unlock()

	// Parse C strings to Go strings
	networkIDStr := C.GoString(networkID)
	rpcBindStr := C.GoString(rpcBind)
	dataDirStr := C.GoString(dataDir)
	bootnodesStr := C.GoString(bootnodes)
	genesisPathStr := C.GoString(genesisPath)

	// Create context for cancellation
	nodeContext, nodeCancel = context.WithCancel(context.Background())

	// Start node in goroutine (non-blocking)
	go func() {
		nodeMutex.Lock()
		nodeRunning = true
		nodeMutex.Unlock()

		callLogCallback("INFO", fmt.Sprintf("Starting Archivas node: network=%s, rpc_bind=%s, data_dir=%s",
			networkIDStr, rpcBindStr, dataDirStr))

		// Initialize node
		err := startArchivasNode(nodeContext, networkIDStr, rpcBindStr, dataDirStr, bootnodesStr, genesisPathStr)
		if err != nil {
			callLogCallback("ERROR", fmt.Sprintf("Failed to start node: %v", err))
			nodeMutex.Lock()
			nodeRunning = false
			nodeMutex.Unlock()
			return
		}

		// Wait for cancellation
		<-nodeContext.Done()

		// Cleanup
		stopArchivasNode()
		nodeMutex.Lock()
		nodeRunning = false
		nodeMutex.Unlock()
		callLogCallback("INFO", "Archivas node stopped")
	}()

	return 0 // Success
}

// loadBannedPeers loads banned peers from disk
func loadBannedPeers(filePath string) error {
	bannedPeersMutex.Lock()
	defer bannedPeersMutex.Unlock()

	if bannedPeers == nil {
		bannedPeers = make(map[string]bool)
	}

	// Always ban Server B
	bannedPeers["72.251.11.191:9090"] = true
	bannedPeers["72.251.11.191"] = true

	// Try to load from file (non-blocking, don't fail if file doesn't exist)
	data, err := os.ReadFile(filePath)
	if err != nil {
		if os.IsNotExist(err) {
			// File doesn't exist, that's OK - we'll create it later if needed
			// Don't call saveBannedPeers here as it might hang
			return nil
		}
		// If there's an error reading, just log it and continue with defaults
		return nil // Don't fail initialization due to banned peers file error
	}

	var peers []string
	if err := json.Unmarshal(data, &peers); err != nil {
		// If JSON is invalid, just continue with defaults
		return nil
	}

	for _, peer := range peers {
		bannedPeers[peer] = true
	}

	// Ensure Server B is always banned
	bannedPeers["72.251.11.191:9090"] = true
	bannedPeers["72.251.11.191"] = true

	return nil
}

// saveBannedPeers saves banned peers to disk
func saveBannedPeers(filePath string) error {
	bannedPeersMutex.RLock()
	defer bannedPeersMutex.RUnlock()

	peers := make([]string, 0, len(bannedPeers))
	for peer := range bannedPeers {
		peers = append(peers, peer)
	}

	data, err := json.MarshalIndent(peers, "", "  ")
	if err != nil {
		return err
	}

	// Ensure directory exists
	dir := filepath.Dir(filePath)
	if err := os.MkdirAll(dir, 0755); err != nil {
		return err
	}

	// Atomic write
	tmpPath := filePath + ".tmp"
	if err := os.WriteFile(tmpPath, data, 0644); err != nil {
		return err
	}

	return os.Rename(tmpPath, filePath)
}

// isBanned checks if a peer is banned (thread-safe)
func isBanned(peerAddr string) bool {
	bannedPeersMutex.RLock()
	defer bannedPeersMutex.RUnlock()

	// Check exact match
	if bannedPeers[peerAddr] {
		return true
	}

	// Check if contains Server B IP
	if strings.Contains(peerAddr, "72.251.11.191") {
		return true
	}

	return false
}

// startArchivasNode initializes and starts the Archivas node
func startArchivasNode(ctx context.Context, networkID, rpcBind, dataDir, bootnodes, genesisPath string) error {
	callLogCallback("INFO", "Initializing Archivas node...")

	// Ensure RPC binds to 0.0.0.0 if no host specified
	callLogCallback("INFO", "Step 1: Setting up RPC bind address")
	rpcBindAddr := rpcBind
	if strings.HasPrefix(rpcBindAddr, ":") {
		rpcBindAddr = "0.0.0.0" + rpcBindAddr
	}
	callLogCallback("INFO", fmt.Sprintf("RPC bind address: %s", rpcBindAddr))

	// Ensure data directory exists
	callLogCallback("INFO", fmt.Sprintf("Step 2: Ensuring data directory exists: %s", dataDir))
	if err := os.MkdirAll(dataDir, 0755); err != nil {
		callLogCallback("ERROR", fmt.Sprintf("Failed to create data directory: %v", err))
		return fmt.Errorf("failed to create data directory: %w", err)
	}
	callLogCallback("INFO", "Data directory ready")

	// Load banned peers from disk
	callLogCallback("INFO", "Step 3: Loading banned peers")
	bannedPeersFile = dataDir + "/banned_peers.json"
	if err := loadBannedPeers(bannedPeersFile); err != nil {
		callLogCallback("WARN", fmt.Sprintf("Failed to load banned peers: %v", err))
	} else {
		bannedPeersMutex.RLock()
		count := len(bannedPeers)
		bannedPeersMutex.RUnlock()
		callLogCallback("INFO", fmt.Sprintf("Loaded %d banned peer(s) (Server B permanently banned)", count))
	}

	// Open database
	callLogCallback("INFO", fmt.Sprintf("Step 4: Opening database: %s", dataDir))
	callLogCallback("INFO", "Calling storage.OpenDB()...")

	// Check if LOCK file exists (might indicate stale lock)
	lockFile := dataDir + "/LOCK"
	if _, err := os.Stat(lockFile); err == nil {
		callLogCallback("WARN", "LOCK file exists - database might be locked by another process")
		callLogCallback("WARN", "If no other process is using the database, this might be a stale lock")
	}

	db, err := storage.OpenDB(dataDir)
	if err != nil {
		callLogCallback("ERROR", fmt.Sprintf("Failed to open database: %v", err))
		callLogCallback("ERROR", "Database open failed - check if another process is using it or if database is corrupted")
		return fmt.Errorf("failed to open database: %w", err)
	}
	callLogCallback("INFO", "Database opened successfully, creating storage instances")

	blockStore := storage.NewBlockStorage(db)
	stateStore := storage.NewStateStorage(db)
	metaStore := storage.NewMetadataStorage(db)

	callLogCallback("INFO", "Database opened successfully")

	// Try to load existing state from disk
	callLogCallback("INFO", "Step 5: Loading existing state from database")
	var worldState *ledger.WorldState
	var cs *consensus.Consensus
	var chain []Block
	var currentHeight uint64
	var genesisChallenge [32]byte
	var genesisHash [32]byte

	callLogCallback("INFO", "Loading tip height from database")
	tipHeight, err := metaStore.LoadTipHeight()
	freshStart := err != nil
	if freshStart {
		callLogCallback("INFO", "Fresh start detected (no existing database)")
	} else {
		callLogCallback("INFO", fmt.Sprintf("Existing database found, tip height: %d", tipHeight))
	}

	// Load genesis file from bundled resources
	callLogCallback("INFO", "Step 6: Loading genesis file")
	callLogCallback("INFO", fmt.Sprintf("Attempting to load genesis from: %s", genesisPath))
	gen, err := config.LoadGenesis(genesisPath)
	if err != nil {
		// Fallback: try relative path or use default location
		callLogCallback("WARN", fmt.Sprintf("Failed to load genesis from %s: %v, trying alternatives", genesisPath, err))
		// Try to find genesis file in common locations
		possiblePaths := []string{
			"genesis/devnet.genesis.json",
			"../archivas/genesis/devnet.genesis.json",
			"../../archivas/genesis/devnet.genesis.json",
		}
		gen = nil
		for _, path := range possiblePaths {
			if g, err := config.LoadGenesis(path); err == nil {
				gen = g
				genesisPath = path
				break
			}
		}
		if gen == nil {
			return fmt.Errorf("failed to load genesis file from any location: %v", err)
		}
	}

	callLogCallback("INFO", fmt.Sprintf("Loaded genesis file from %s", genesisPath))

	// Store genesis path for fork recovery
	genesisPathMutex.Lock()
	currentGenesisPath = genesisPath
	genesisPathMutex.Unlock()

	// Calculate genesis document hash
	genesisDocHash := config.HashGenesis(gen)
	callLogCallback("INFO", fmt.Sprintf("Genesis document hash: %x", genesisDocHash[:8]))

	// Create genesis block exactly as Archivas node does
	// Note: Genesis block uses hardcoded difficulty 2^50, NOT gen.InitialDifficulty
	// The InitialDifficulty field is for consensus difficulty target, not genesis block difficulty
	genesisAllocs := config.GenesisAllocToMap(gen.Allocations)
	genesisChallenge = consensus.GenerateGenesisChallenge()

	// Genesis block difficulty is hardcoded to 2^50 (as in archivas-node/main.go)
	genesisBlockDifficulty := uint64(1125899906842624) // 2^50

	// Create genesis block with exact fields matching archivas-node/main.go
	genesisBlock := Block{
		Height:         0,
		TimestampUnix:  gen.Timestamp,          // Use FIXED timestamp from genesis.json
		PrevHash:       [32]byte{},             // Empty prev hash for genesis
		Difficulty:     genesisBlockDifficulty, // 2^50 (hardcoded, not from InitialDifficulty)
		Challenge:      genesisChallenge,
		Txs:            nil,
		Proof:          nil,
		FarmerAddr:     "",
		CumulativeWork: consensus.CalculateWork(genesisBlockDifficulty), // Genesis work
	}

	// Calculate genesis block hash (should match network's genesis hash)
	calculatedGenesisHash := hashBlock(&genesisBlock)
	callLogCallback("INFO", fmt.Sprintf("Calculated genesis block hash: %x", calculatedGenesisHash[:8]))

	// Expected network genesis hash (verified to match when using difficulty 2^50)
	expectedGenesisHashStr := "56588fa6d64be03437fcc05247e52aea5062c9f045c779cbad6ac3c21d7b65fe"
	expectedGenesisHashBytes, _ := hex.DecodeString(expectedGenesisHashStr)
	var expectedGenesisHash [32]byte
	copy(expectedGenesisHash[:], expectedGenesisHashBytes)

	// Verify calculated hash matches network's expected hash
	if calculatedGenesisHash != expectedGenesisHash {
		return fmt.Errorf("genesis block hash mismatch! Calculated %x, expected %x. This indicates incorrect genesis block creation", calculatedGenesisHash[:8], expectedGenesisHash[:8])
	}
	callLogCallback("INFO", fmt.Sprintf("Genesis block hash verified: %x (matches network)", calculatedGenesisHash[:8]))

	// Check if database has wrong genesis
	needsClear := false
	if !freshStart {
		savedGenesisHash, err := metaStore.LoadGenesisHash()
		if err == nil {
			// Check if saved genesis hash matches expected
			if savedGenesisHash != expectedGenesisHash {
				callLogCallback("WARN", fmt.Sprintf("Genesis hash mismatch! Database has %x, expected %x", savedGenesisHash[:8], expectedGenesisHash[:8]))
				callLogCallback("INFO", "Clearing database to fix genesis mismatch...")
				needsClear = true
			} else {
				callLogCallback("INFO", fmt.Sprintf("Genesis hash verified: %x", savedGenesisHash[:8]))
				genesisHash = savedGenesisHash
			}
		} else {
			callLogCallback("INFO", "No saved genesis hash found, will initialize with correct genesis")
			needsClear = true
		}
	}

	// Clear database if needed
	if freshStart || needsClear {
		if !freshStart {
			// Close database before clearing
			db.Close()

			// Remove database directory
			callLogCallback("INFO", fmt.Sprintf("Removing corrupted database: %s", dataDir))
			if err := os.RemoveAll(dataDir); err != nil {
				return fmt.Errorf("failed to remove corrupted database: %w", err)
			}

			// Recreate data directory
			if err := os.MkdirAll(dataDir, 0755); err != nil {
				return fmt.Errorf("failed to recreate data directory: %w", err)
			}

			// Reopen database
			db, err = storage.OpenDB(dataDir)
			if err != nil {
				return fmt.Errorf("failed to reopen database: %w", err)
			}
			blockStore = storage.NewBlockStorage(db)
			stateStore = storage.NewStateStorage(db)
			metaStore = storage.NewMetadataStorage(db)
		}

		// Initialize with correct genesis block
		callLogCallback("INFO", "Initializing chain with correct genesis block from file")
		worldState = ledger.NewWorldState(genesisAllocs)
		cs = consensus.NewConsensus()
		// Set initial difficulty from genesis file (not from genesis block difficulty)
		cs.DifficultyTarget = gen.InitialDifficulty
		chain = []Block{genesisBlock}
		currentHeight = 0
		genesisHash = calculatedGenesisHash

		// Genesis hash is calculated from genesis file (source of truth)
		callLogCallback("INFO", fmt.Sprintf("Genesis block hash from genesis file: %x", genesisHash[:8]))

		// Persist genesis block and state
		if err := blockStore.SaveBlock(0, genesisBlock); err != nil {
			return fmt.Errorf("failed to save genesis block: %w", err)
		}
		for addr, balance := range genesisAllocs {
			if err := stateStore.SaveAccount(addr, balance, 0); err != nil {
				return fmt.Errorf("failed to save genesis account %s: %w", addr, err)
			}
		}
		if err := metaStore.SaveTipHeight(0); err != nil {
			return fmt.Errorf("failed to save tip height: %w", err)
		}
		if err := metaStore.SaveDifficulty(cs.DifficultyTarget); err != nil {
			return fmt.Errorf("failed to save difficulty: %w", err)
		}
		if err := metaStore.SaveGenesisHash(genesisHash); err != nil {
			return fmt.Errorf("failed to save genesis hash: %w", err)
		}
		if err := metaStore.SaveNetworkID(networkID); err != nil {
			return fmt.Errorf("failed to save network ID: %w", err)
		}

		callLogCallback("INFO", fmt.Sprintf("Genesis block saved: height=0, hash=%x", genesisHash[:8]))
		callLogCallback("INFO", fmt.Sprintf("Initialized %d genesis accounts", len(gen.Allocations)))
	} else {
		// Load from disk
		callLogCallback("INFO", fmt.Sprintf("Loading existing state from tip height %d", tipHeight))

		// Load difficulty (but we'll update it from blocks during IBD)
		difficulty, err := metaStore.LoadDifficulty()
		if err != nil {
			// If no difficulty stored, use network difficulty if available
			networkDifficultyMutex.RLock()
			networkDiff := networkDifficulty
			networkDifficultyMutex.RUnlock()
			if networkDiff > 0 {
				difficulty = networkDiff
				callLogCallback("INFO", fmt.Sprintf("Using network difficulty %d (no stored difficulty)", difficulty))
			} else {
				// Fallback to genesis difficulty
				difficulty = gen.InitialDifficulty
				callLogCallback("INFO", fmt.Sprintf("Using genesis difficulty %d (no network difficulty available)", difficulty))
			}
		}
		cs = &consensus.Consensus{DifficultyTarget: difficulty}

		// Load blocks
		chain = make([]Block, 0, tipHeight+1)
		for h := uint64(0); h <= tipHeight; h++ {
			var blk Block
			if err := blockStore.LoadBlock(h, &blk); err != nil {
				return fmt.Errorf("failed to load block %d: %w", h, err)
			}
			chain = append(chain, blk)
		}

		// Update difficulty from tip block (most accurate)
		// BUT: Don't use genesis block difficulty (2^50) - use consensus difficulty instead
		// Genesis block difficulty is for the block itself, not for consensus
		if len(chain) > 0 {
			tipBlock := chain[len(chain)-1]
			// Only update from non-genesis blocks (genesis block difficulty is 2^50, not consensus difficulty)
			if tipBlock.Height > 0 && tipBlock.Difficulty > 0 && tipBlock.Difficulty != cs.DifficultyTarget {
				oldDiff := cs.DifficultyTarget
				cs.DifficultyTarget = tipBlock.Difficulty
				callLogCallback("INFO", fmt.Sprintf("Updated difficulty from tip block %d: %d → %d", tipBlock.Height, oldDiff, tipBlock.Difficulty))
				// Save updated difficulty
				if err := metaStore.SaveDifficulty(cs.DifficultyTarget); err != nil {
					callLogCallback("WARN", fmt.Sprintf("Failed to save updated difficulty: %v", err))
				}
			}
		}

		// Reconstruct world state
		worldState = &ledger.WorldState{
			Accounts: make(map[string]*ledger.AccountState),
		}

		// Load genesis hash and network ID
		savedGenesisHash, err := metaStore.LoadGenesisHash()
		if err != nil {
			return fmt.Errorf("failed to load genesis hash: %w", err)
		}

		// Verify genesis hash matches expected network genesis
		if savedGenesisHash != expectedGenesisHash {
			callLogCallback("WARN", fmt.Sprintf("Genesis hash mismatch! Database has %x, expected %x", savedGenesisHash[:8], expectedGenesisHash[:8]))
			callLogCallback("INFO", "Clearing database to fix genesis mismatch...")

			// Close database before clearing
			db.Close()

			// Remove database directory
			callLogCallback("INFO", fmt.Sprintf("Removing corrupted database: %s", dataDir))
			if err := os.RemoveAll(dataDir); err != nil {
				return fmt.Errorf("failed to remove corrupted database: %w", err)
			}

			// Recreate data directory
			if err := os.MkdirAll(dataDir, 0755); err != nil {
				return fmt.Errorf("failed to recreate data directory: %w", err)
			}

			// Reopen database
			db, err = storage.OpenDB(dataDir)
			if err != nil {
				return fmt.Errorf("failed to reopen database: %w", err)
			}
			blockStore = storage.NewBlockStorage(db)
			stateStore = storage.NewStateStorage(db)
			metaStore = storage.NewMetadataStorage(db)

			// Initialize with correct genesis block
			callLogCallback("INFO", "Initializing chain with correct genesis block from file")
			worldState = ledger.NewWorldState(genesisAllocs)
			cs = consensus.NewConsensus()
			// Set initial difficulty from genesis file (not from genesis block difficulty)
			cs.DifficultyTarget = gen.InitialDifficulty
			chain = []Block{genesisBlock}
			currentHeight = 0
			genesisHash = calculatedGenesisHash

			// Genesis hash is calculated from genesis file (source of truth)
			callLogCallback("INFO", fmt.Sprintf("Genesis block hash from genesis file: %x", genesisHash[:8]))

			// Persist genesis block and state
			if err := blockStore.SaveBlock(0, genesisBlock); err != nil {
				return fmt.Errorf("failed to save genesis block: %w", err)
			}
			for addr, balance := range genesisAllocs {
				if err := stateStore.SaveAccount(addr, balance, 0); err != nil {
					return fmt.Errorf("failed to save genesis account %s: %w", addr, err)
				}
			}
			if err := metaStore.SaveTipHeight(0); err != nil {
				return fmt.Errorf("failed to save tip height: %w", err)
			}
			if err := metaStore.SaveDifficulty(cs.DifficultyTarget); err != nil {
				return fmt.Errorf("failed to save difficulty: %w", err)
			}
			if err := metaStore.SaveGenesisHash(genesisHash); err != nil {
				return fmt.Errorf("failed to save genesis hash: %w", err)
			}
			if err := metaStore.SaveNetworkID(networkID); err != nil {
				return fmt.Errorf("failed to save network ID: %w", err)
			}

			callLogCallback("INFO", fmt.Sprintf("Genesis block saved: height=0, hash=%x", genesisHash[:8]))
			callLogCallback("INFO", fmt.Sprintf("Initialized %d genesis accounts", len(gen.Allocations)))
		} else {
			callLogCallback("INFO", fmt.Sprintf("Genesis hash verified: %x", savedGenesisHash[:8]))
			genesisHash = savedGenesisHash

			// Load accounts from genesis allocations (use the loaded genesis file)
			callLogCallback("INFO", "Loading accounts from genesis allocations")
			for addr, balance := range genesisAllocs {
				// Try to load account from database
				dbBalance, dbNonce, exists, err := stateStore.LoadAccount(addr)
				if err != nil {
					callLogCallback("WARN", fmt.Sprintf("Failed to load account %s: %v, using genesis balance", addr, err))
					// Use genesis balance if database load fails
					worldState.Accounts[addr] = &ledger.AccountState{
						Balance: balance,
						Nonce:   0,
					}
				} else if exists {
					// Use database balance (might have changed due to transactions)
					worldState.Accounts[addr] = &ledger.AccountState{
						Balance: dbBalance,
						Nonce:   dbNonce,
					}
				} else {
					// Account doesn't exist in database, use genesis balance
					worldState.Accounts[addr] = &ledger.AccountState{
						Balance: balance,
						Nonce:   0,
					}
				}
			}
			callLogCallback("INFO", fmt.Sprintf("Loaded %d accounts from genesis", len(worldState.Accounts)))

			// Load accounts from blocks (simplified - in production would scan all blocks)
			accountsFound := make(map[string]bool)
			for _, blk := range chain {
				if blk.FarmerAddr != "" && !accountsFound[blk.FarmerAddr] {
					balance, nonce, exists, err := stateStore.LoadAccount(blk.FarmerAddr)
					if err == nil && exists {
						worldState.Accounts[blk.FarmerAddr] = &ledger.AccountState{
							Balance: balance,
							Nonce:   nonce,
						}
						accountsFound[blk.FarmerAddr] = true
					}
				}
				for _, tx := range blk.Txs {
					for _, addr := range []string{tx.From, tx.To} {
						if addr != "" && !accountsFound[addr] {
							balance, nonce, exists, err := stateStore.LoadAccount(addr)
							if err == nil && exists {
								worldState.Accounts[addr] = &ledger.AccountState{
									Balance: balance,
									Nonce:   nonce,
								}
								accountsFound[addr] = true
							}
						}
					}
				}
			}

			currentHeight = tipHeight
			// Get current challenge from tip block
			if len(chain) > 0 {
				tipBlock := chain[len(chain)-1]
				// Generate challenge for next block based on tip
				newBlockHash := hashBlock(&tipBlock)
				genesisChallenge = consensus.GenerateChallenge(newBlockHash, currentHeight+1)
			} else {
				genesisChallenge = consensus.GenerateGenesisChallenge()
			}
		}
	}

	// Create node state
	nodeStateMutex.Lock()
	nodeState = &NodeState{
		Chain:            chain,
		WorldState:       worldState,
		Mempool:          mempool.NewMempool(),
		Consensus:        cs,
		CurrentHeight:    currentHeight,
		CurrentChallenge: genesisChallenge,
		DB:               db,
		BlockStore:       blockStore,
		StateStore:       stateStore,
		MetaStore:        metaStore,
		Health:           health.NewChainHealth(),
		ReorgDetector:    consensus.NewReorgDetector(),
		GenesisHash:      genesisHash,
		NetworkID:        networkID,
		DataDir:          dataDir, // Store dataDir for fork recovery
	}
	nodeStateMutex.Unlock()

	callLogCallback("INFO", "Node state initialized")

	// Start P2P network if bootnodes provided
	var p2pAddr string = ":9090" // Default P2P address
	if bootnodes != "" {
		// Extract P2P address from bootnodes or use default
		bootnodeList := strings.Split(bootnodes, ",")
		if len(bootnodeList) > 0 {
			// Use first bootnode's port as reference, or default
			p2pAddr = ":9090"
		}
	}

	if bootnodes != "" {
		callLogCallback("INFO", fmt.Sprintf("Starting P2P network on %s", p2pAddr))
		p2pNet := p2p.NewNetwork(p2pAddr, nodeState)

		p2pNet.SetGossipConfig(p2p.GossipConfig{
			NetworkID:      networkID,
			EnableGossip:   false, // Disable gossip to prevent connecting to wrong-chain peers
			Interval:       60 * time.Second,
			MaxPeers:       10, // Limit peers to reduce chance of connecting to wrong chain
			DialsPerMinute: 2,  // Reduce dial frequency
		})

		// Set up peer store
		peerStorePath := dataDir + "/peers.json"
		peerStore, err := p2p.NewFilePeerStore(peerStorePath)
		if err != nil {
			callLogCallback("WARN", fmt.Sprintf("Failed to create peer store: %v", err))
		} else {
			p2pNet.SetPeerStore(peerStore)
		}

		if err := p2pNet.Start(); err != nil {
			return fmt.Errorf("failed to start P2P: %w", err)
		}

		nodeStateMutex.Lock()
		nodeState.P2P = p2pNet
		nodeStateMutex.Unlock()

		callLogCallback("INFO", "P2P network started")

		// Monitor and reject Server B connections immediately at P2P level
		// We can't modify the P2P library's acceptLoop directly, but we can monitor
		// connected peers and disconnect Server B immediately
		go func() {
			ticker := time.NewTicker(2 * time.Second)
			defer ticker.Stop()
			for {
				select {
				case <-ctx.Done():
					return
				case <-ticker.C:
					nodeStateMutex.RLock()
					p2pNet := nodeState.P2P
					nodeStateMutex.RUnlock()

					if p2pNet != nil {
						connected, _ := p2pNet.GetPeerList()
						for _, peerAddr := range connected {
							// Check if peer is Server B
							if strings.Contains(peerAddr, "72.251.11.191") {
								callLogCallback("WARN", fmt.Sprintf("Rejecting connection from banned peer 72.251.11.191:9090"))
								// Close the connection by removing from peers map
								// Note: P2P library doesn't expose DisconnectPeer, but we can
								// mark it as banned and it will be ignored
								bannedPeersMutex.Lock()
								if bannedPeers == nil {
									bannedPeers = make(map[string]bool)
								}
								bannedPeers[peerAddr] = true
								bannedPeersMutex.Unlock()
							}
						}
					}
				}
			}
		}()

		// Connect to bootnodes (filter to only seed.archivas.ai)
		if bootnodes != "" {
			bootnodeList := strings.Split(bootnodes, ",")
			filteredBootnodes := []string{}
			for _, bootnode := range bootnodeList {
				bootnode = strings.TrimSpace(bootnode)
				// Only allow seed.archivas.ai as bootnode
				if bootnode != "" && strings.Contains(bootnode, "seed.archivas.ai") {
					filteredBootnodes = append(filteredBootnodes, bootnode)
				} else if bootnode != "" {
					callLogCallback("WARN", fmt.Sprintf("Filtered out bootnode %s (only seed.archivas.ai allowed)", bootnode))
				}
			}

			for _, bootnode := range filteredBootnodes {
				go func(addr string) {
					time.Sleep(500 * time.Millisecond)
					nodeStateMutex.RLock()
					p2p := nodeState.P2P
					nodeStateMutex.RUnlock()
					if p2p != nil {
						if err := p2p.ConnectPeer(addr); err != nil {
							callLogCallback("WARN", fmt.Sprintf("Failed to connect to peer %s: %v", addr, err))
						} else {
							callLogCallback("INFO", fmt.Sprintf("Connected to bootnode %s", addr))
						}
					}
				}(bootnode)
			}
			callLogCallback("INFO", fmt.Sprintf("Connecting to %d bootnode(s) (filtered from %d)", len(filteredBootnodes), len(bootnodeList)))
		}

		// Start IBD if we have peers (only use seed.archivas.ai)
		if bootnodes != "" {
			go func() {
				time.Sleep(2 * time.Second)
				bootnodeList := strings.Split(bootnodes, ",")
				peerURLs := []string{}
				// Only use seed.archivas.ai for IBD
				for _, bootnode := range bootnodeList {
					bootnode = strings.TrimSpace(bootnode)
					if bootnode == "" {
						continue
					}
					// Only allow seed.archivas.ai for IBD
					if strings.Contains(bootnode, "seed.archivas.ai") {
						peerURL := "https://seed.archivas.ai"
						peerURLs = append(peerURLs, peerURL)
						callLogCallback("INFO", fmt.Sprintf("Using %s for IBD", peerURL))
					}
				}

				if len(peerURLs) > 0 {
					// Wait a bit more to ensure node state is ready
					time.Sleep(1 * time.Second)

					nodeStateMutex.RLock()
					ns := nodeState
					currentHeight := uint64(0)
					if ns != nil {
						currentHeight = ns.GetCurrentHeight()
					}
					nodeStateMutex.RUnlock()

					if ns != nil {
						callLogCallback("INFO", fmt.Sprintf("Starting IBD from %d peer(s), current height: %d", len(peerURLs), currentHeight))

						// Configure IBD with lower threshold to ensure it runs from height 0
						ibdConfig := node.DefaultIBDConfig(dataDir)
						ibdConfig.IBDThreshold = 1                   // Always run IBD if behind (even by 1 block)
						ibdConfig.CatchUpThreshold = 10              // Consider synced when within 10 blocks
						ibdConfig.ProgressInterval = 2 * time.Second // Log progress every 2 seconds
						ibdConfig.BatchSize = 512                    // Fetch 512 blocks at a time

						callLogCallback("INFO", fmt.Sprintf("IBD config: threshold=%d, catchup=%d, batchSize=%d",
							ibdConfig.IBDThreshold, ibdConfig.CatchUpThreshold, ibdConfig.BatchSize))

						// Create IBD manager
						ibdManager := node.NewIBDManager(ibdConfig, ns)

						if err := ibdManager.LoadState(); err != nil {
							callLogCallback("WARN", fmt.Sprintf("Failed to load IBD state: %v", err))
						}

						callLogCallback("INFO", fmt.Sprintf("Running IBD with retry from %v", peerURLs))

						// Run IBD in goroutine with enhanced error logging and progress tracking
						go func() {
							// Capture dataDir for potential database clearing
							dbDataDir := dataDir
							callLogCallback("INFO", "IBD goroutine started - fetching blocks from network")

							// Test connection first and get remote tip and difficulty
							var remoteTip uint64 = 0
							var networkDiff uint64 = 0
							for _, peerURL := range peerURLs {
								testURL := fmt.Sprintf("%s/chainTip", peerURL)

								// Retry logic for 503 errors (Service Unavailable)
								var resp *http.Response
								var err error
								maxRetries := 5
								retryDelay := 2 * time.Second
								for attempt := 0; attempt < maxRetries; attempt++ {
									resp, err = http.Get(testURL)
									if err != nil {
										if attempt < maxRetries-1 {
											callLogCallback("WARN", fmt.Sprintf("Failed to connect to %s (attempt %d/%d): %v, retrying in %v...", peerURL, attempt+1, maxRetries, err, retryDelay))
											time.Sleep(retryDelay)
											retryDelay *= 2 // Exponential backoff
											continue
										}
										callLogCallback("ERROR", fmt.Sprintf("Failed to connect to %s after %d attempts: %v", peerURL, maxRetries, err))
										continue
									}

									if resp.StatusCode == 503 {
										resp.Body.Close()
										if attempt < maxRetries-1 {
											callLogCallback("WARN", fmt.Sprintf("Seed node %s returned 503 (Service Unavailable), attempt %d/%d, retrying in %v...", peerURL, attempt+1, maxRetries, retryDelay))
											time.Sleep(retryDelay)
											retryDelay *= 2 // Exponential backoff
											continue
										}
										callLogCallback("ERROR", fmt.Sprintf("Seed node %s returned 503 after %d attempts - service may be temporarily unavailable", peerURL, maxRetries))
										continue
									}

									if resp.StatusCode != 200 {
										resp.Body.Close()
										callLogCallback("ERROR", fmt.Sprintf("HTTP error from %s: %d", peerURL, resp.StatusCode))
										break // Exit retry loop for this peer
									}

									// Success - parse response and break retry loop
									var tipResp struct {
										Height     string `json:"height"`
										Difficulty string `json:"difficulty"`
									}
									if err := json.NewDecoder(resp.Body).Decode(&tipResp); err == nil {
										if h, err := fmt.Sscanf(tipResp.Height, "%d", &remoteTip); h == 1 && err == nil {
											callLogCallback("INFO", fmt.Sprintf("Connected to %s - remote tip: %d", peerURL, remoteTip))
										}
										// Parse difficulty
										if tipResp.Difficulty != "" {
											if d, err := fmt.Sscanf(tipResp.Difficulty, "%d", &networkDiff); d == 1 && err == nil {
												// Cache network difficulty
												networkDifficultyMutex.Lock()
												networkDifficulty = networkDiff
												networkDifficultyTime = time.Now()
												networkDifficultyMutex.Unlock()
												callLogCallback("INFO", fmt.Sprintf("Network difficulty: %d", networkDiff))

												// Immediately update consensus difficulty if node state exists
												nodeStateMutex.RLock()
												currentNodeState := nodeState
												nodeStateMutex.RUnlock()
												if currentNodeState != nil && currentNodeState.Consensus != nil {
													currentNodeState.Lock()
													oldDiff := currentNodeState.Consensus.DifficultyTarget
													if oldDiff != networkDiff {
														currentNodeState.Consensus.DifficultyTarget = networkDiff
														callLogCallback("INFO", fmt.Sprintf("Updated consensus difficulty from network: %d → %d", oldDiff, networkDiff))
														// Save updated difficulty
														if currentNodeState.MetaStore != nil {
															if err := currentNodeState.MetaStore.SaveDifficulty(currentNodeState.Consensus.DifficultyTarget); err != nil {
																callLogCallback("WARN", fmt.Sprintf("Failed to save network difficulty: %v", err))
															}
														}
													}
													currentNodeState.Unlock()
												}
											}
										}
									}
									resp.Body.Close()
									break // Success - exit retry loop
								}
							}

							// Verify genesis block exists
							nodeStateMutex.RLock()
							hasGenesis := ns != nil && len(ns.Chain) > 0 && ns.Chain[0].Height == 0
							startHeight := uint64(0)
							if ns != nil {
								startHeight = ns.GetCurrentHeight()
							}
							nodeStateMutex.RUnlock()

							if !hasGenesis {
								callLogCallback("ERROR", "Genesis block missing! Node initialization should have created it from genesis file.")
								callLogCallback("ERROR", "Cannot start IBD without genesis block. Please check logs above for initialization errors.")
								return
							}

							if remoteTip == 0 {
								callLogCallback("ERROR", "Failed to get remote tip height - cannot start IBD")
								return
							}

							// Redirect Go's log output to our callback to capture IBD progress
							log.SetOutput(logWriter{level: "INFO"})
							log.SetFlags(0) // Remove timestamps

							// Start IBD with progress monitoring
							callLogCallback("INFO", fmt.Sprintf("Starting IBD: current height=%d, target height=%d (%.2f%% complete)",
								startHeight, remoteTip, float64(startHeight)*100.0/float64(remoteTip)))

							// Initialize IBD progress tracking
							ibdProgressMutex.Lock()
							ibdLastAppliedHeight = startHeight
							ibdLastAppliedTime = time.Now()
							ibdAppliedBlocks = 0
							ibdProgressMutex.Unlock()

							// Track progress for reporting
							lastReportedHeight := startHeight
							lastProgressTime := time.Now()
							ibdStartTime := time.Now()

							// Heartbeat and progress monitor (logs every 10 seconds)
							progressTicker := time.NewTicker(10 * time.Second)
							defer progressTicker.Stop()

							progressDone := make(chan bool)
							go func() {
								for {
									select {
									case <-progressDone:
										return
									case <-progressTicker.C:
										nodeStateMutex.RLock()
										currentHeight := uint64(0)
										if ns != nil {
											currentHeight = ns.GetCurrentHeight()
										}
										nodeStateMutex.RUnlock()

										// Get IBD progress stats
										ibdProgressMutex.RLock()
										appliedBlocks := ibdAppliedBlocks
										lastApplied := ibdLastAppliedHeight
										lastAppliedTime := ibdLastAppliedTime
										ibdProgressMutex.RUnlock()

										if currentHeight > lastReportedHeight {
											// Progress made - log detailed progress
											blocksApplied := currentHeight - lastReportedHeight
											elapsed := time.Since(lastProgressTime)
											var rate float64
											if elapsed.Seconds() > 0 {
												rate = float64(blocksApplied) / elapsed.Seconds()
											}
											progress := float64(currentHeight) * 100.0 / float64(remoteTip)
											remaining := remoteTip - currentHeight
											var eta time.Duration
											if rate > 0 {
												eta = time.Duration(float64(remaining)/rate) * time.Second
											}

											callLogCallback("INFO", fmt.Sprintf("IBD progress: %d/%d (%.2f%%) - applied %d blocks in %v (%.1f blocks/sec, ETA: %v, total applied: %d)",
												currentHeight, remoteTip, progress, blocksApplied, elapsed.Round(time.Second), rate, eta.Round(time.Second), appliedBlocks))

											lastReportedHeight = currentHeight
											lastProgressTime = time.Now()
										} else {
											// No progress - log heartbeat with diagnostics
											elapsed := time.Since(ibdStartTime)
											timeSinceLastBlock := time.Since(lastAppliedTime)
											callLogCallback("INFO", fmt.Sprintf("IBD heartbeat: height=%d/%d (%.2f%%), last block: %d (%v ago), total applied: %d, elapsed: %v",
												currentHeight, remoteTip, float64(currentHeight)*100.0/float64(remoteTip), lastApplied, timeSinceLastBlock.Round(time.Second), appliedBlocks, elapsed.Round(time.Second)))

											// If stuck for more than 30 seconds, log warning
											if timeSinceLastBlock > 30*time.Second && currentHeight < remoteTip {
												callLogCallback("WARN", fmt.Sprintf("IBD appears stuck at height %d - no blocks applied in %v", currentHeight, timeSinceLastBlock.Round(time.Second)))
											}
										}
									}
								}
							}()

							// Run IBD with automatic retry on failure
							callLogCallback("INFO", fmt.Sprintf("Starting HTTP IBD from %d peer(s): %v", len(peerURLs), peerURLs))
							callLogCallback("INFO", fmt.Sprintf("IBD will fetch blocks via HTTP from /blocks/range endpoint"))

							// Automatic retry loop with exponential backoff
							maxRetryAttempts := 10         // Maximum retry attempts
							retryDelay := 30 * time.Second // Initial retry delay
							ibdSuccess := false

							for attempt := 1; attempt <= maxRetryAttempts; attempt++ {
								ibdErr := ibdManager.RunIBDWithRetry(peerURLs)
								if ibdErr != nil {
									// Get current height to see how far we got
									nodeStateMutex.RLock()
									currentHeight := uint64(0)
									if ns != nil {
										currentHeight = ns.GetCurrentHeight()
									}
									nodeStateMutex.RUnlock()

									// Extract detailed error information
									errStr := fmt.Sprintf("%+v", ibdErr)
									errStrLower := strings.ToLower(errStr)

									// Check if this is a forked chain error (check multiple indicators)
									isForkedChain := strings.Contains(errStrLower, "prev hash") ||
										strings.Contains(errStrLower, "height discontinuity") ||
										strings.Contains(errStrLower, "forked chain") ||
										strings.Contains(errStrLower, "mismatch") ||
										strings.Contains(errStrLower, "wrong chain") ||
										strings.Contains(errStrLower, "rejecting")

									// Categorize error (forked chain check first, as it's most critical)
									if isForkedChain {
										callLogCallback("ERROR", fmt.Sprintf("Fork detected at height %d: %v", currentHeight, ibdErr))
										callLogCallback("WARN", "Local chain is on a fork - auto-recovering by clearing database and resyncing from genesis")

										// Auto-recover: Clear database and reinitialize from genesis (without stopping)
										nodeStateMutex.RLock()
										recoveryNetworkID := "archivas-devnet-v4"
										recoveryDataDir := dbDataDir
										if nodeState != nil {
											if nodeState.NetworkID != "" {
												recoveryNetworkID = nodeState.NetworkID
											}
											if nodeState.DataDir != "" {
												recoveryDataDir = nodeState.DataDir
											}
										}
										nodeStateMutex.RUnlock()

										// Get genesis path from stored value
										genesisPathMutex.RLock()
										recoveryGenesisPath := currentGenesisPath
										genesisPathMutex.RUnlock()
										if recoveryGenesisPath == "" {
											recoveryGenesisPath = "genesis/devnet.genesis.json" // Fallback
										}

										// Recover from fork (clears DB, reinitializes from genesis, continues running)
										if err := recoverFromFork(recoveryDataDir, recoveryGenesisPath, recoveryNetworkID, rpcBindAddr); err != nil {
											callLogCallback("ERROR", fmt.Sprintf("Fork recovery failed: %v", err))
											break // Can't continue if recovery fails
										}

										// After fork recovery, restart IBD from height 0
										attempt = 0 // Reset attempt counter
										retryDelay = 10 * time.Second
										callLogCallback("INFO", "Fork recovery complete, restarting IBD from genesis...")
										continue
									}

									// Check if error is recoverable (network/server issues)
									isRecoverable := strings.Contains(errStrLower, "timeout") ||
										strings.Contains(errStrLower, "connection") ||
										strings.Contains(errStrLower, "network") ||
										strings.Contains(errStrLower, "http") ||
										strings.Contains(errStrLower, "502") ||
										strings.Contains(errStrLower, "503") ||
										strings.Contains(errStrLower, "unavailable") ||
										strings.Contains(errStrLower, "failed with all")

									if !isRecoverable {
										// Non-recoverable error (parsing, data corruption, etc.)
										callLogCallback("ERROR", fmt.Sprintf("IBD failed with non-recoverable error at height %d/%d: %v", currentHeight, remoteTip, ibdErr))
										break
									}

									// Recoverable error - retry with exponential backoff
									if attempt < maxRetryAttempts {
										callLogCallback("WARN", fmt.Sprintf("IBD failed at height %d/%d (attempt %d/%d): %v", currentHeight, remoteTip, attempt, maxRetryAttempts, ibdErr))
										callLogCallback("INFO", fmt.Sprintf("Retrying IBD in %v (server may be temporarily unavailable)...", retryDelay))
										time.Sleep(retryDelay)

										// Exponential backoff with max delay of 5 minutes
										retryDelay = retryDelay * 2
										if retryDelay > 5*time.Minute {
											retryDelay = 5 * time.Minute
										}

										// Note: Remote tip will be refreshed on next IBD attempt
										// If chain grew while we were retrying, we'll catch up
									} else {
										callLogCallback("ERROR", fmt.Sprintf("IBD failed after %d attempts: %v", maxRetryAttempts, ibdErr))
									}
								} else {
									// IBD succeeded
									ibdSuccess = true
									break
								}
							}

							// Mark IBD as complete (success or failure)
							ibdRunningMutex.Lock()
							ibdRunning = false
							ibdRunningMutex.Unlock()

							// Stop progress monitor
							close(progressDone)

							// Restore log output
							log.SetOutput(os.Stderr)
							log.SetFlags(log.LstdFlags)

							if ibdSuccess {
								nodeStateMutex.RLock()
								finalHeight := uint64(0)
								if ns != nil {
									finalHeight = ns.GetCurrentHeight()
								}
								nodeStateMutex.RUnlock()

								elapsed := time.Since(ibdStartTime)
								blocksSynced := finalHeight - startHeight
								rate := float64(blocksSynced) / elapsed.Seconds()

								callLogCallback("INFO", fmt.Sprintf("IBD completed successfully: synced to height %d/%d (%.2f%%) in %v (%.1f blocks/sec)",
									finalHeight, remoteTip, float64(finalHeight)*100.0/float64(remoteTip), elapsed.Round(time.Second), rate))
							} else {
								callLogCallback("ERROR", "IBD failed after all retry attempts - will retry on next node restart or when seed server recovers")
							}
						}()
					} else {
						callLogCallback("ERROR", "Node state is nil, cannot start IBD")
					}
				} else {
					callLogCallback("WARN", "No valid peers for IBD (seed.archivas.ai required)")
				}
			}()
		}
	}

	// Start RPC server
	callLogCallback("INFO", fmt.Sprintf("Starting RPC server on %s", rpcBindAddr))
	server := rpc.NewFarmingServer(nodeState.WorldState, nodeState.Mempool, nodeState)
	go func() {
		if err := server.Start(rpcBindAddr); err != nil {
			callLogCallback("ERROR", fmt.Sprintf("RPC server error: %v", err))
		}
	}()

	// Give RPC server a moment to start
	time.Sleep(500 * time.Millisecond)
	callLogCallback("INFO", "RPC server running")

	// Start metrics updater
	go func() {
		ticker := time.NewTicker(2 * time.Second)
		defer ticker.Stop()
		for {
			select {
			case <-ctx.Done():
				return
			case <-ticker.C:
				nodeStateMutex.RLock()
				if nodeState != nil {
					metrics.UpdateTipHeight(nodeState.CurrentHeight)
					if nodeState.P2P != nil {
						connected, _ := nodeState.P2P.GetPeerList()
						metrics.UpdatePeerCount(len(connected))
					}
					metrics.UpdateDifficulty(nodeState.Consensus.DifficultyTarget)
				}
				nodeStateMutex.RUnlock()
			}
		}
	}()

	// Start background block sync monitor (checks for new blocks after IBD completes)
	callLogCallback("INFO", "Starting background block sync monitor...")
	go func() {
		// Wait a bit before starting to let IBD complete first
		time.Sleep(5 * time.Second)
		callLogCallback("INFO", "Background block sync monitor started (checking every 30 seconds)")
		
		ticker := time.NewTicker(30 * time.Second) // Check every 30 seconds
		defer ticker.Stop()
		
		// Helper function to perform sync check
		performSyncCheck := func() {
			callLogCallback("DEBUG", "Background sync monitor: performing sync check...")
			
			// Skip if IBD is running
			ibdRunningMutex.RLock()
			ibdIsRunning := ibdRunning
			ibdRunningMutex.RUnlock()
			
			if ibdIsRunning {
				callLogCallback("DEBUG", "Background sync monitor: IBD is running, skipping check")
				return // Let IBD handle syncing
			}
			
			// Get current height
			nodeStateMutex.RLock()
			currentHeight := uint64(0)
			if nodeState != nil {
				currentHeight = nodeState.CurrentHeight
			}
			nodeStateMutex.RUnlock()
			
			if currentHeight == 0 {
				callLogCallback("DEBUG", "Background sync monitor: node not initialized yet (height=0), skipping")
				return // Not initialized yet
			}
			
			callLogCallback("DEBUG", fmt.Sprintf("Background sync monitor: checking network tip (local height: %d)", currentHeight))
			
			// Check network tip height
			seedURL := "https://seed.archivas.ai"
			tipURL := fmt.Sprintf("%s/chainTip", seedURL)
			
			// Use longer timeout for sync checks (60 seconds, same as IBD)
			client := &http.Client{Timeout: 60 * time.Second}
			
			// Retry up to 3 times with exponential backoff
			var resp *http.Response
			var err error
			maxRetries := 3
			for attempt := 0; attempt < maxRetries; attempt++ {
				if attempt > 0 {
					backoff := time.Duration(attempt) * 5 * time.Second
					callLogCallback("DEBUG", fmt.Sprintf("Sync check: retrying chainTip fetch (attempt %d/%d) after %v", attempt+1, maxRetries, backoff))
					time.Sleep(backoff)
				}
				
				resp, err = client.Get(tipURL)
				if err == nil && resp.StatusCode == 200 {
					break // Success
				}
				
				if resp != nil {
					resp.Body.Close()
				}
				
				if attempt < maxRetries-1 {
					callLogCallback("DEBUG", fmt.Sprintf("Sync check: chainTip fetch failed (attempt %d/%d): %v", attempt+1, maxRetries, err))
				}
			}
			
			if err != nil {
				callLogCallback("DEBUG", fmt.Sprintf("Sync check: failed to fetch chainTip after %d attempts: %v", maxRetries, err))
				return
			}
			
			if resp.StatusCode != 200 {
				callLogCallback("DEBUG", fmt.Sprintf("Sync check: chainTip returned status %d", resp.StatusCode))
				resp.Body.Close()
				return
			}
			
			// Parse response - height might be string or number
			var tipRespRaw map[string]interface{}
			if err := json.NewDecoder(resp.Body).Decode(&tipRespRaw); err != nil {
				resp.Body.Close()
				callLogCallback("DEBUG", fmt.Sprintf("Sync check: failed to decode chainTip response: %v", err))
				return
			}
			resp.Body.Close()
			
			// Parse height (can be string or number)
			var networkTip uint64
			if heightVal, ok := tipRespRaw["height"]; ok {
				switch v := heightVal.(type) {
				case float64:
					networkTip = uint64(v)
				case string:
					if parsed, err := strconv.ParseUint(v, 10, 64); err == nil {
						networkTip = parsed
					} else {
						callLogCallback("DEBUG", fmt.Sprintf("Sync check: failed to parse height as string: %v", err))
						return
					}
				case uint64:
					networkTip = v
				default:
					callLogCallback("DEBUG", fmt.Sprintf("Sync check: height has unexpected type: %T", v))
					return
				}
			} else {
				callLogCallback("DEBUG", "Sync check: height field missing from chainTip response")
				return
			}
			
			// Log sync check status
			if networkTip > currentHeight {
				gap := networkTip - currentHeight
				callLogCallback("INFO", fmt.Sprintf("Sync check: local=%d, network=%d (gap: %d blocks) - fetching", currentHeight, networkTip, gap))
			} else if networkTip < currentHeight {
				callLogCallback("DEBUG", fmt.Sprintf("Sync check: local=%d, network=%d (local ahead by %d blocks)", currentHeight, networkTip, currentHeight-networkTip))
			} else {
				callLogCallback("DEBUG", fmt.Sprintf("Sync check: local=%d, network=%d (synced)", currentHeight, networkTip))
			}
			
			// If we're behind by more than 0 blocks, fetch missing blocks
			if networkTip > currentHeight {
				gap := networkTip - currentHeight
				callLogCallback("INFO", fmt.Sprintf("Network is ahead: local=%d, network=%d (gap: %d blocks) - fetching missing blocks", currentHeight, networkTip, gap))
				
				// Fetch missing blocks using HTTP (same as IBD)
				batchSize := uint64(100)
				fromHeight := currentHeight + 1
				toHeight := currentHeight + batchSize
				if toHeight > networkTip {
					toHeight = networkTip
				}
				
				blocksURL := fmt.Sprintf("%s/blocks/range?from=%d&limit=%d", seedURL, fromHeight, toHeight-fromHeight+1)
				blocksResp, err := client.Get(blocksURL)
				if err != nil {
					callLogCallback("WARN", fmt.Sprintf("Failed to fetch blocks %d-%d: %v", fromHeight, toHeight, err))
					return
				}
				
				if blocksResp.StatusCode != 200 {
					blocksResp.Body.Close()
					callLogCallback("WARN", fmt.Sprintf("HTTP error fetching blocks %d-%d: %d", fromHeight, toHeight, blocksResp.StatusCode))
					return
				}
				
				var blocksRespData struct {
					Blocks []json.RawMessage `json:"blocks"`
				}
				
				if err := json.NewDecoder(blocksResp.Body).Decode(&blocksRespData); err != nil {
					blocksResp.Body.Close()
					callLogCallback("WARN", fmt.Sprintf("Failed to decode blocks response: %v", err))
					return
				}
				blocksResp.Body.Close()
				
				// Apply each block
				nodeStateMutex.RLock()
				ns := nodeState
				nodeStateMutex.RUnlock()
				
				if ns != nil {
					appliedCount := 0
					for _, blockData := range blocksRespData.Blocks {
						if err := ns.ApplyBlock(blockData); err != nil {
							callLogCallback("WARN", fmt.Sprintf("Failed to apply block: %v", err))
							return // Stop if we hit an error
						}
						appliedCount++
					}
					
					if appliedCount > 0 {
						nodeStateMutex.RLock()
						newHeight := uint64(0)
						if nodeState != nil {
							newHeight = nodeState.CurrentHeight
						}
						nodeStateMutex.RUnlock()
						callLogCallback("INFO", fmt.Sprintf("Applied %d new blocks, height now: %d", appliedCount, newHeight))
					}
				}
			}
		}
		
		// Do an immediate check after starting
		performSyncCheck()
		
		// Then check every 30 seconds
		for {
			select {
			case <-ctx.Done():
				callLogCallback("INFO", "Background block sync monitor stopped")
				return
			case <-ticker.C:
				performSyncCheck()
			}
		}
	}()

	callLogCallback("INFO", "Archivas node started successfully")
	callLogCallback("INFO", "Waiting for farmers to submit blocks...")

	// Heartbeat loop with IBD health check
	ticker := time.NewTicker(5 * time.Second)
	defer ticker.Stop()

	// IBD health check ticker (checks every 2 minutes if IBD is stuck)
	ibdHealthTicker := time.NewTicker(2 * time.Minute)
	defer ibdHealthTicker.Stop()
	lastIBDHeight := uint64(0)
	lastIBDHeightTime := time.Now()

	for {
		select {
		case <-ctx.Done():
			return nil
		case <-ticker.C:
			nodeStateMutex.RLock()
			if nodeState != nil {
				height := nodeState.CurrentHeight
				difficulty := nodeState.Consensus.DifficultyTarget
				callLogCallback("DEBUG", fmt.Sprintf("Node running: height=%d difficulty=%d", height, difficulty))
			}
			nodeStateMutex.RUnlock()
		case <-ibdHealthTicker.C:
			// Check if IBD is stuck (no progress in last 5 minutes)
			ibdRunningMutex.RLock()
			isRunning := ibdRunning
			ibdRunningMutex.RUnlock()

			if isRunning {
				nodeStateMutex.RLock()
				currentHeight := uint64(0)
				if nodeState != nil {
					currentHeight = nodeState.CurrentHeight
				}
				nodeStateMutex.RUnlock()

				// Check if height has changed
				if currentHeight == lastIBDHeight {
					timeSinceLastProgress := time.Since(lastIBDHeightTime)
					if timeSinceLastProgress > 5*time.Minute {
						callLogCallback("WARN", fmt.Sprintf("IBD appears stuck at height %d for %v - this may indicate server issues", currentHeight, timeSinceLastProgress.Round(time.Second)))
						// The automatic retry logic should handle this, but log a warning
					}
				} else {
					// Progress made - update tracking
					lastIBDHeight = currentHeight
					lastIBDHeightTime = time.Now()
				}
			} else {
				// IBD not running - reset tracking
				lastIBDHeight = 0
				lastIBDHeightTime = time.Now()
			}
		}
	}
}

// stopArchivasNode stops the node and cleans up resources
func stopArchivasNode() {
	nodeStateMutex.Lock()
	defer nodeStateMutex.Unlock()

	if nodeState == nil {
		return
	}

	callLogCallback("INFO", "Stopping Archivas node...")

	// Stop P2P network
	if nodeState.P2P != nil {
		nodeState.P2P.Stop()
	}

	// Close database
	if nodeState.DB != nil {
		nodeState.DB.Close()
	}

	nodeState = nil
	callLogCallback("INFO", "Archivas node stopped and cleaned up")
}

//export archivas_node_stop
func archivas_node_stop() {
	nodeMutex.Lock()
	defer nodeMutex.Unlock()

	if !nodeRunning {
		return
	}

	callLogCallback("INFO", "Stopping Archivas node...")

	if nodeCancel != nil {
		nodeCancel()
	}

	nodeRunning = false
}

//export archivas_node_is_running
func archivas_node_is_running() C.int {
	nodeMutex.RLock()
	defer nodeMutex.RUnlock()

	if nodeRunning {
		return 1
	}
	return 0
}

//export archivas_node_get_height
func archivas_node_get_height() C.int {
	nodeStateMutex.RLock()
	defer nodeStateMutex.RUnlock()

	if nodeState == nil {
		return 0
	}
	return C.int(nodeState.CurrentHeight)
}

//export archivas_node_get_tip_hash
func archivas_node_get_tip_hash() *C.char {
	nodeStateMutex.RLock()
	defer nodeStateMutex.RUnlock()

	if nodeState == nil || len(nodeState.Chain) == 0 {
		hash := "0000000000000000000000000000000000000000000000000000000000000000"
		return C.CString(hash)
	}

	// Get tip block
	tipBlock := nodeState.Chain[len(nodeState.Chain)-1]
	hash := hashBlock(&tipBlock)

	// Use cached network hash if available (for blocks without proof)
	networkHashMutex.RLock()
	if networkBlockHashes != nil {
		if cachedHash, ok := networkBlockHashes[tipBlock.Height]; ok {
			hash = cachedHash
		}
	}
	networkHashMutex.RUnlock()

	hashStr := hex.EncodeToString(hash[:])
	return C.CString(hashStr)
}

//export archivas_node_get_peer_count
func archivas_node_get_peer_count() C.int {
	nodeStateMutex.RLock()
	defer nodeStateMutex.RUnlock()

	if nodeState == nil || nodeState.P2P == nil {
		return 0
	}
	return C.int(nodeState.P2P.GetPeerCount())
}

//export archivas_node_set_log_callback
func archivas_node_set_log_callback(callback C.log_callback_t) {
	logCallbackMutex.Lock()
	defer logCallbackMutex.Unlock()
	logCallback = callback
}

// NodeState interface methods (required by p2p.NodeHandler, node.NodeIBDInterface, rpc.NodeState)

// GetStatus returns current height, difficulty, and tip hash (for p2p.NodeHandler)
func (ns *NodeState) GetStatus() (uint64, uint64, [32]byte) {
	ns.RLock()
	defer ns.RUnlock()

	if len(ns.Chain) == 0 {
		// If no chain, try to get network difficulty from cache
		networkDifficultyMutex.RLock()
		networkDiff := networkDifficulty
		networkDifficultyMutex.RUnlock()
		if networkDiff > 0 {
			return 0, networkDiff, [32]byte{}
		}
		return 0, ns.Consensus.DifficultyTarget, [32]byte{}
	}

	tipBlock := ns.Chain[len(ns.Chain)-1]
	tipHash := hashBlock(&tipBlock)

	// Use cached network hash if available (for blocks without proof)
	networkHashMutex.RLock()
	if networkBlockHashes != nil {
		if cachedHash, ok := networkBlockHashes[tipBlock.Height]; ok {
			tipHash = cachedHash
		}
	}
	networkHashMutex.RUnlock()

	// Use difficulty from tip block if available (most accurate)
	// Otherwise fall back to consensus difficulty or network difficulty
	tipDifficulty := ns.Consensus.DifficultyTarget

	// If we have a tip block, use its difficulty (this is the actual network difficulty at that height)
	if len(ns.Chain) > 0 {
		tipDifficulty = tipBlock.Difficulty
	}

	// If significantly behind network and no blocks yet, use network difficulty from cache
	if tipDifficulty == 0 || (len(ns.Chain) == 0 || ns.CurrentHeight < 100) {
		networkDifficultyMutex.RLock()
		networkDiff := networkDifficulty
		networkDiffTime := networkDifficultyTime
		networkDifficultyMutex.RUnlock()

		// Use network difficulty if:
		// 1. We have cached network difficulty
		// 2. Cache is recent (less than 5 minutes old)
		// 3. Local chain is empty or very early in sync
		if networkDiff > 0 && time.Since(networkDiffTime) < 5*time.Minute {
			return ns.CurrentHeight, networkDiff, tipHash
		}
	}

	return ns.CurrentHeight, tipDifficulty, tipHash
}

// OnNewBlock is called when a peer announces a new block (for p2p.NodeHandler)
func (ns *NodeState) OnNewBlock(height uint64, hash [32]byte, fromPeer string) {
	// Check if peer is banned - silently reject (no logging to reduce spam)
	if isBanned(fromPeer) {
		return
	}

	ns.RLock()
	currentHeight := ns.CurrentHeight
	ns.RUnlock()

	// Filter out peers that are not seed.archivas.ai (unless we're very behind)
	// This prevents connecting to forked chains
	if !strings.Contains(fromPeer, "seed.archivas.ai") {
		// Only ignore non-seed peers if height is way ahead (likely forked chain)
		// If height is way ahead of network tip (~480k), it's likely a forked chain
		if height > 200000 {
			// Only log once per peer to reduce spam
			bannedPeersMutex.Lock()
			if bannedPeers == nil {
				bannedPeers = make(map[string]bool)
			}
			wasBanned := bannedPeers[fromPeer]
			bannedPeers[fromPeer] = true
			// Save banned peers to disk
			if bannedPeersFile != "" {
				saveBannedPeers(bannedPeersFile)
			}
			bannedPeersMutex.Unlock()

			if !wasBanned {
				callLogCallback("WARN", fmt.Sprintf("Banned peer %s (height %d, likely forked chain) - permanently ignoring", fromPeer, height))
			}
			return
		}
	}

	// Don't log block announcements - progress is tracked by IBD progress monitor

	if height > currentHeight {
		// During IBD or when significantly behind, ONLY use IBD (HTTP), not P2P block requests
		// This prevents syncing from forked chains
		gap := height - currentHeight
		isSignificantlyBehind := gap > 10 // More than 10 blocks behind - use IBD only

		// Check if IBD is already running
		ibdRunningMutex.RLock()
		ibdAlreadyRunning := ibdRunning
		ibdRunningMutex.RUnlock()

		if isSignificantlyBehind {
			// When significantly behind, ONLY use seed.archivas.ai for IBD (HTTP)
			// DO NOT request blocks from P2P - let IBD handle everything via HTTP
			if strings.Contains(fromPeer, "seed.archivas.ai") {
				// Only start IBD if not already running
				if !ibdAlreadyRunning {
					ibdRunningMutex.Lock()
					ibdRunning = true
					ibdRunningMutex.Unlock()
					callLogCallback("INFO", fmt.Sprintf("Starting IBD from seed.archivas.ai (we're %d blocks behind)", gap))
					// Don't call P2P.StartIBD - we use HTTP-based IBD via RunIBDWithRetry instead
					// P2P.StartIBD uses batch requests which are getting empty responses
				}
			}
			// Silently ignore all block announcements when significantly behind
			// IBD will fetch blocks via HTTP, not P2P
		} else {
			// Close to tip (within 10 blocks) - can accept from any peer
			// BUT: Don't request blocks from P2P if IBD is running (let IBD handle it)
			if !ibdAlreadyRunning {
				if strings.Contains(fromPeer, "seed.archivas.ai") || gap < 10 {
					if ns.P2P != nil {
						ns.P2P.RequestBlock(height)
					}
				}
			}
		}
	}
}

// OnBlockRequest handles a request for a specific block (for p2p.NodeHandler)
func (ns *NodeState) OnBlockRequest(height uint64) (interface{}, error) {
	ns.RLock()
	defer ns.RUnlock()

	if int(height) < len(ns.Chain) {
		return ns.Chain[height], nil
	}

	if ns.BlockStore != nil {
		var block Block
		if err := ns.BlockStore.LoadBlock(height, &block); err == nil {
			return block, nil
		}
	}

	return nil, fmt.Errorf("block not found")
}

// OnBlocksRangeRequest serves a batch of blocks for IBD (for p2p.NodeHandler)
func (ns *NodeState) OnBlocksRangeRequest(fromHeight uint64, maxBlocks uint32) (blocks []json.RawMessage, tipHeight uint64, eof bool, err error) {
	ns.RLock()
	defer ns.RUnlock()

	tipHeight = ns.CurrentHeight

	if maxBlocks == 0 || maxBlocks > 512 {
		maxBlocks = 512
	}

	if fromHeight < 1 {
		fromHeight = 1
	}

	if fromHeight > tipHeight {
		return []json.RawMessage{}, tipHeight, true, nil
	}

	remaining := tipHeight - fromHeight + 1
	count := uint64(maxBlocks)
	if count > remaining {
		count = remaining
		eof = true
	}

	blocks = make([]json.RawMessage, 0, count)

	for h := fromHeight; h < fromHeight+count; h++ {
		var block Block

		if int(h) < len(ns.Chain) {
			block = ns.Chain[h]
		} else if ns.BlockStore != nil {
			if err := ns.BlockStore.LoadBlock(h, &block); err != nil {
				break
			}
		} else {
			break
		}

		blockJSON, err := json.Marshal(block)
		if err != nil {
			break
		}

		blocks = append(blocks, blockJSON)
	}

	if uint64(len(blocks)) < count {
		eof = false
	}

	return blocks, tipHeight, eof, nil
}

// GetCurrentVDF returns current VDF state (for p2p.NodeHandler)
func (ns *NodeState) GetCurrentVDF() (seed []byte, iterations uint64, output []byte, hasVDF bool) {
	ns.RLock()
	defer ns.RUnlock()
	return ns.VDFSeed, ns.VDFIterations, ns.VDFOutput, ns.HasVDF
}

// UpdateVDFState updates the VDF state from timelord (for p2p.NodeHandler)
func (ns *NodeState) UpdateVDFState(seed []byte, iterations uint64, output []byte) {
	ns.Lock()
	defer ns.Unlock()
	ns.VDFSeed = seed
	ns.VDFIterations = iterations
	ns.VDFOutput = output
	ns.HasVDF = true

	h := sha256.New()
	h.Write(output)
	binary.Write(h, binary.BigEndian, ns.CurrentHeight+1)
	ns.CurrentChallenge = sha256.Sum256(h.Sum(nil))
}

// LocalHeight returns current chain height (for p2p.NodeHandler)
func (ns *NodeState) LocalHeight() uint64 {
	ns.RLock()
	defer ns.RUnlock()
	return ns.CurrentHeight
}

// GetCurrentHeight returns the current chain height (for node.NodeIBDInterface)
func (ns *NodeState) GetCurrentHeight() uint64 {
	ns.RLock()
	defer ns.RUnlock()
	return ns.CurrentHeight
}

// GetCurrentChallenge returns the current challenge, difficulty, and next height (for rpc.NodeState)
func (ns *NodeState) GetCurrentChallenge() ([32]byte, uint64, uint64) {
	ns.RLock()
	defer ns.RUnlock()
	return ns.CurrentChallenge, ns.Consensus.DifficultyTarget, ns.CurrentHeight + 1
}

// GetHealthStats returns detailed health statistics (for rpc.NodeState)
func (ns *NodeState) GetHealthStats() interface{} {
	if ns.Health == nil {
		return map[string]interface{}{"status": "not initialized"}
	}

	stats := ns.Health.GetStats()

	return map[string]interface{}{
		"uptime":          stats.Uptime.String(),
		"uptimeSeconds":   int(stats.Uptime.Seconds()),
		"totalBlocks":     stats.TotalBlocks,
		"avgBlockTime":    stats.AverageBlockTime.String(),
		"avgBlockSeconds": stats.AverageBlockTime.Seconds(),
		"blocksPerHour":   stats.BlocksPerHour,
		"lastBlockTime":   stats.LastBlockTime.Format(time.RFC3339),
	}
}

// GetPeerCount returns number of connected peers (for rpc.NodeState)
// Note: This is separate from the p2p.NodeHandler GetPeerCount which returns int
// The rpc.NodeState interface may require a different signature
func (ns *NodeState) GetPeerCount() int {
	if ns.P2P == nil {
		return 0
	}
	return ns.P2P.GetPeerCount()
}

// GetRecentBlocks returns the most recent N blocks (for rpc.NodeState)
func (ns *NodeState) GetRecentBlocks(count int) interface{} {
	ns.RLock()
	defer ns.RUnlock()

	chainLen := len(ns.Chain)
	if count > chainLen {
		count = chainLen
	}

	start := chainLen - count
	recentBlocks := make([]map[string]interface{}, 0, count)

	for i := start; i < chainLen; i++ {
		block := ns.Chain[i]
		blockHash := hashBlock(&block)

		formattedTxs := make([]map[string]interface{}, len(block.Txs))
		for j, tx := range block.Txs {
			txType := "transfer"
			if tx.From == "coinbase" {
				txType = "coinbase"
			}

			formattedTxs[j] = map[string]interface{}{
				"type":   txType,
				"from":   tx.From,
				"to":     tx.To,
				"amount": tx.Amount,
				"fee":    tx.Fee,
				"nonce":  tx.Nonce,
			}
		}

		recentBlocks = append(recentBlocks, map[string]interface{}{
			"height":     block.Height,
			"hash":       hex.EncodeToString(blockHash[:]),
			"timestamp":  block.TimestampUnix,
			"difficulty": block.Difficulty,
			"farmerAddr": block.FarmerAddr,
			"txCount":    len(block.Txs),
			"txs":        formattedTxs,
		})
	}

	return recentBlocks
}

// HasBlock checks if we have a block at given height (for p2p.NodeHandler)
func (ns *NodeState) HasBlock(height uint64) bool {
	ns.RLock()
	defer ns.RUnlock()
	return int(height) < len(ns.Chain)
}

// GetGenesisHash returns the genesis hash (for p2p.NodeHandler)
func (ns *NodeState) GetGenesisHash() [32]byte {
	ns.RLock()
	defer ns.RUnlock()
	return ns.GenesisHash
}

// GetPeerList returns connected and known peer addresses (for p2p.NodeHandler)
func (ns *NodeState) GetPeerList() (connected []string, known []string) {
	if ns.P2P == nil {
		return []string{}, []string{}
	}
	return ns.P2P.GetPeerList()
}

// VerifyAndApplyBlock verifies and applies a block received from a peer (for p2p.NodeHandler)
func (ns *NodeState) VerifyAndApplyBlock(blockJSON json.RawMessage) error {
	// During IBD, don't apply blocks from P2P - only IBD's HTTP mechanism should apply blocks
	ibdRunningMutex.RLock()
	ibdIsRunning := ibdRunning
	ibdRunningMutex.RUnlock()

	if ibdIsRunning {
		// During IBD, reject P2P blocks - let IBD handle block application
		return fmt.Errorf("IBD in progress - rejecting P2P block (blocks must come from IBD)")
	}

	var block Block
	if err := json.Unmarshal(blockJSON, &block); err != nil {
		return fmt.Errorf("failed to unmarshal block: %w", err)
	}

	ns.Lock()
	defer ns.Unlock()

	if block.Height != ns.CurrentHeight+1 {
		return fmt.Errorf("block height %d doesn't match expected %d", block.Height, ns.CurrentHeight+1)
	}

	if len(ns.Chain) > 0 {
		prevBlock := ns.Chain[len(ns.Chain)-1]
		prevHash := hashBlock(&prevBlock)

		// If we have a cached network hash for the previous block (because proof was missing),
		// use that instead of the calculated hash
		networkHashMutex.RLock()
		if networkBlockHashes != nil {
			if cachedHash, ok := networkBlockHashes[prevBlock.Height]; ok {
				prevHash = cachedHash
			}
		}
		networkHashMutex.RUnlock()

		if block.PrevHash != prevHash {
			// Prev hash mismatch indicates wrong chain - this block should be rejected
			// The peer sending this is likely on a forked chain
			return fmt.Errorf("prev hash mismatch (likely forked chain)")
		}
	} else {
		// Check genesis hash for first block
		if block.Height == 0 {
			genesisHash := hashBlock(&block)
			if genesisHash != ns.GenesisHash {
				return fmt.Errorf("genesis hash mismatch (wrong chain)")
			}
		}
	}

	// Apply transactions
	for _, tx := range block.Txs {
		if tx.From == "coinbase" {
			receiver, ok := ns.WorldState.Accounts[tx.To]
			if !ok {
				receiver = &ledger.AccountState{Balance: 0, Nonce: 0}
				ns.WorldState.Accounts[tx.To] = receiver
			}
			receiver.Balance += tx.Amount
		} else {
			if err := ns.WorldState.ApplyTransaction(tx); err != nil {
				callLogCallback("WARN", fmt.Sprintf("Skipping invalid tx in block %d: %v", block.Height, err))
			}
		}
	}

	ns.Chain = append(ns.Chain, block)
	ns.CurrentHeight = block.Height

	if ns.BlockStore != nil {
		if err := ns.BlockStore.SaveBlock(block.Height, block); err != nil {
			return fmt.Errorf("failed to save block: %w", err)
		}
	}

	if ns.StateStore != nil {
		for addr, acc := range ns.WorldState.Accounts {
			if err := ns.StateStore.SaveAccount(addr, acc.Balance, acc.Nonce); err != nil {
				callLogCallback("WARN", fmt.Sprintf("Failed to save account %s: %v", addr, err))
			}
		}
	}

	if ns.MetaStore != nil {
		if err := ns.MetaStore.SaveTipHeight(block.Height); err != nil {
			callLogCallback("WARN", fmt.Sprintf("Failed to save tip height: %v", err))
		}
	}

	return nil
}

// GetBlockByHeight returns a specific block by height (for rpc.NodeState)
func (ns *NodeState) GetBlockByHeight(height uint64) (interface{}, error) {
	ns.RLock()
	defer ns.RUnlock()

	if int(height) >= len(ns.Chain) {
		return nil, fmt.Errorf("block %d not found (tip: %d)", height, len(ns.Chain)-1)
	}

	block := ns.Chain[height]
	blockHash := hashBlock(&block)

	formattedTxs := make([]map[string]interface{}, len(block.Txs))
	for i, tx := range block.Txs {
		txType := "transfer"
		if tx.From == "coinbase" {
			txType = "coinbase"
		}

		formattedTxs[i] = map[string]interface{}{
			"type":   txType,
			"from":   tx.From,
			"to":     tx.To,
			"amount": tx.Amount,
			"fee":    tx.Fee,
			"nonce":  tx.Nonce,
		}
	}

	return map[string]interface{}{
		"height":     block.Height,
		"hash":       hex.EncodeToString(blockHash[:]),
		"prevHash":   hex.EncodeToString(block.PrevHash[:]),
		"timestamp":  block.TimestampUnix,
		"difficulty": block.Difficulty,
		"challenge":  hex.EncodeToString(block.Challenge[:]),
		"farmerAddr": block.FarmerAddr,
		"txCount":    len(block.Txs),
		"txs":        formattedTxs,
	}, nil
}

// ApplyBlock applies a block received during IBD (for node.NodeIBDInterface)
// IMPORTANT: This is only called during IBD from seed.archivas.ai, so blocks are trusted
func (ns *NodeState) ApplyBlock(blockData json.RawMessage) error {
	// Log block receipt for debugging
	if len(blockData) == 0 {
		callLogCallback("WARN", "IBD: Received empty block data in ApplyBlock")
		return fmt.Errorf("empty block data")
	}

	var blockMap map[string]interface{}
	if err := json.Unmarshal(blockData, &blockMap); err != nil {
		callLogCallback("ERROR", fmt.Sprintf("Failed to unmarshal block data (length: %d): %v", len(blockData), err))
		return fmt.Errorf("failed to unmarshal block map: %w", err)
	}

	// Log received block info for debugging
	if height, ok := blockMap["height"].(float64); ok {
		hashFromNetwork, _ := blockMap["hash"].(string)
		prevHashFromNetwork, _ := blockMap["prevHash"].(string)
		difficultyFromNetwork, _ := blockMap["difficulty"].(float64)
		timestampFromNetwork, _ := blockMap["timestamp"].(float64)
		hashShort := hashFromNetwork
		if len(hashShort) > 16 {
			hashShort = hashShort[:16]
		}
		prevHashShort := prevHashFromNetwork
		if len(prevHashShort) > 16 {
			prevHashShort = prevHashShort[:16]
		}
		callLogCallback("DEBUG", fmt.Sprintf("IBD: Received block height %d from network: hash=%s, prevHash=%s, difficulty=%.0f, timestamp=%.0f",
			uint64(height), hashShort, prevHashShort, difficultyFromNetwork, timestampFromNetwork))
	}

	// Verify this block is from IBD (seed.archivas.ai) - reject if from other sources
	// During IBD, all blocks come from seed.archivas.ai, so we can trust them
	// But we still verify prev hash to detect forks

	height, ok := blockMap["height"].(float64)
	if !ok {
		callLogCallback("ERROR", fmt.Sprintf("Block data missing or invalid height field: %v", blockMap))
		return fmt.Errorf("block missing height field")
	}
	difficulty, _ := blockMap["difficulty"].(float64)
	timestamp, _ := blockMap["timestamp"].(float64)
	farmerAddr, _ := blockMap["farmerAddr"].(string)

	var prevHash, challenge [32]byte
	if prevHashStr, ok := blockMap["prevHash"].(string); ok {
		prevHashBytes, _ := hex.DecodeString(prevHashStr)
		copy(prevHash[:], prevHashBytes)
	}
	if challengeStr, ok := blockMap["challenge"].(string); ok {
		challengeBytes, _ := hex.DecodeString(challengeStr)
		copy(challenge[:], challengeBytes)
	}

	txs := []ledger.Transaction{}
	if txList, ok := blockMap["txs"].([]interface{}); ok {
		for _, txRaw := range txList {
			if txMap, ok := txRaw.(map[string]interface{}); ok {
				tx := ledger.Transaction{
					From:   getString(txMap, "from"),
					To:     getString(txMap, "to"),
					Amount: getInt64(txMap, "amount"),
					Fee:    getInt64(txMap, "fee"),
					Nonce:  getUint64(txMap, "nonce"),
				}
				txs = append(txs, tx)
			}
		}
	}

	// Parse Proof if present (needed for hash calculation)
	// The /blocks/range endpoint now includes the proof field with all required fields
	var proof *pospace.Proof = nil
	if proofRaw, hasProof := blockMap["proof"]; hasProof {
		if proofMap, ok := proofRaw.(map[string]interface{}); ok {
			proof = &pospace.Proof{}

			// Parse proof hash (required for block hash calculation)
			if hashStr, ok := proofMap["hash"].(string); ok {
				if hashBytes, err := hex.DecodeString(hashStr); err == nil && len(hashBytes) == 32 {
					copy(proof.Hash[:], hashBytes)
				} else {
					hashPreview := hashStr
					if len(hashPreview) > 16 {
						hashPreview = hashPreview[:16]
					}
					callLogCallback("ERROR", fmt.Sprintf("Block %d: invalid proof hash format (len=%d, err=%v): %s",
						uint64(height), len(hashBytes), err, hashPreview))
					return fmt.Errorf("block %d: invalid proof hash format", uint64(height))
				}
			} else {
				callLogCallback("ERROR", fmt.Sprintf("Block %d: proof.hash field missing", uint64(height)))
				return fmt.Errorf("block %d: proof.hash field missing", uint64(height))
			}

			// Parse quality (number)
			if quality, ok := proofMap["quality"].(float64); ok {
				proof.Quality = uint64(quality)
			}

			// Parse plotID (hex string -> [32]byte)
			if plotIDStr, ok := proofMap["plotID"].(string); ok {
				if plotIDBytes, err := hex.DecodeString(plotIDStr); err == nil && len(plotIDBytes) == 32 {
					copy(proof.PlotID[:], plotIDBytes)
				} else {
					callLogCallback("WARN", fmt.Sprintf("Block %d: invalid plotID format: %v", uint64(height), err))
				}
			}

			// Parse index (number)
			if index, ok := proofMap["index"].(float64); ok {
				proof.Index = uint64(index)
			}

			// Parse farmerPubKey (hex string -> [33]byte for secp256k1 compressed public key)
			if pubKeyStr, ok := proofMap["farmerPubKey"].(string); ok {
				if pubKeyBytes, err := hex.DecodeString(pubKeyStr); err == nil && len(pubKeyBytes) == 33 {
					copy(proof.FarmerPubKey[:], pubKeyBytes)
				} else {
					callLogCallback("WARN", fmt.Sprintf("Block %d: invalid farmerPubKey format (len=%d, err=%v)",
						uint64(height), len(pubKeyBytes), err))
				}
			}

			// Challenge is already parsed from block level, but proof also has it
			// Use the block's challenge for the proof
			proof.Challenge = challenge

			callLogCallback("DEBUG", fmt.Sprintf("Block %d: parsed proof (hash=%x, quality=%d, plotID=%x)",
				uint64(height), proof.Hash[:8], proof.Quality, proof.PlotID[:8]))
		} else {
			callLogCallback("ERROR", fmt.Sprintf("Block %d: proof field is not a map (type: %T)", uint64(height), proofRaw))
			return fmt.Errorf("block %d: proof field has invalid type", uint64(height))
		}
	} else {
		callLogCallback("ERROR", fmt.Sprintf("Block %d: proof field missing from /blocks/range response (endpoint should include proof)", uint64(height)))
		return fmt.Errorf("block %d: proof field missing (required for hash calculation)", uint64(height))
	}

	// Calculate cumulative work if not provided (needed for hash calculation)
	var cumulativeWork uint64 = 0
	if len(ns.Chain) > 0 {
		prevBlock := ns.Chain[len(ns.Chain)-1]
		cumulativeWork = prevBlock.CumulativeWork + consensus.CalculateWork(uint64(difficulty))
	} else {
		// Genesis block - use genesis cumulative work
		cumulativeWork = consensus.CalculateWork(uint64(difficulty))
	}

	block := Block{
		Height:         uint64(height),
		TimestampUnix:  int64(timestamp),
		PrevHash:       prevHash,
		Difficulty:     uint64(difficulty),
		Challenge:      challenge,
		Txs:            txs,
		Proof:          proof,
		FarmerAddr:     farmerAddr,
		CumulativeWork: cumulativeWork,
	}

	// Validate block hash before applying
	// The /blocks/range endpoint now includes the proof field, so we can calculate the exact hash
	expectedHashStr, hasHash := blockMap["hash"].(string)
	if hasHash {
		var expectedHash [32]byte
		if hashBytes, err := hex.DecodeString(expectedHashStr); err == nil && len(hashBytes) == 32 {
			copy(expectedHash[:], hashBytes)

			// Calculate hash with all fields including proof
			calculatedHash := hashBlock(&block)

			if calculatedHash != expectedHash {
				// Hash mismatch - log detailed error information
				callLogCallback("ERROR", fmt.Sprintf("Block %d hash mismatch! Expected: %x, Calculated: %x",
					block.Height, expectedHash[:8], calculatedHash[:8]))
				callLogCallback("ERROR", fmt.Sprintf("Block data: height=%d, difficulty=%d, timestamp=%d, prevHash=%x, challenge=%x",
					block.Height, block.Difficulty, block.TimestampUnix, block.PrevHash[:8], block.Challenge[:8]))
				if proof != nil {
					callLogCallback("ERROR", fmt.Sprintf("Proof: hash=%x, quality=%d, plotID=%x",
						proof.Hash[:8], proof.Quality, proof.PlotID[:8]))
				} else {
					callLogCallback("ERROR", "Proof: missing (this should not happen with updated /blocks/range endpoint)")
				}
				return fmt.Errorf("block %d hash mismatch: expected %x, got %x (block data may be corrupted or incomplete)",
					block.Height, expectedHash[:8], calculatedHash[:8])
			}
			callLogCallback("DEBUG", fmt.Sprintf("Block %d hash verified: %x", block.Height, calculatedHash[:8]))
		} else {
			callLogCallback("WARN", fmt.Sprintf("Block %d: invalid hash format in response", block.Height))
		}
	} else {
		callLogCallback("WARN", fmt.Sprintf("Block %d: hash field missing from /blocks/range response", block.Height))
	}

	ns.Lock()
	unlockNeeded := true
	defer func() {
		if unlockNeeded {
			ns.Unlock()
		}
	}()

	// Handle genesis block (height 0) specially
	if block.Height == 0 {
		// If we already have a genesis block, verify it matches
		if len(ns.Chain) > 0 && ns.Chain[0].Height == 0 {
			existingGenesis := ns.Chain[0]
			existingGenesisHash := hashBlock(&existingGenesis)
			newBlockHash := hashBlock(&block)
			if existingGenesisHash != newBlockHash {
				callLogCallback("WARN", fmt.Sprintf("Genesis block mismatch - existing: %x, received: %x",
					existingGenesisHash[:8], newBlockHash[:8]))
				return fmt.Errorf("genesis block mismatch (wrong chain)")
			}
			// Genesis already exists and matches, skip
			return nil
		}
		// No genesis block yet - accept this one
		genesisBlockHash := hashBlock(&block)
		callLogCallback("INFO", fmt.Sprintf("Applying genesis block (height 0, hash: %x)", genesisBlockHash[:8]))
	} else {
		// Non-genesis block - check if we have genesis first
		if len(ns.Chain) == 0 {
			return fmt.Errorf("cannot apply block %d: no genesis block exists (need to apply block 0 first)", block.Height)
		}

		// Verify height continuity
		expectedHeight := ns.CurrentHeight + 1
		if block.Height != expectedHeight {
			callLogCallback("ERROR", fmt.Sprintf("Height discontinuity at block application: expected %d, got %d (current height: %d, chain length: %d)",
				expectedHeight, block.Height, ns.CurrentHeight, len(ns.Chain)))
			return fmt.Errorf("height discontinuity: expected %d, got %d", expectedHeight, block.Height)
		}

		// Verify prev hash matches (prevents accepting blocks from forked chain)
		prevBlock := ns.Chain[len(ns.Chain)-1]
		prevHash := hashBlock(&prevBlock)

		// If we have a cached network hash for the previous block (because proof was missing),
		// use that instead of the calculated hash
		networkHashMutex.RLock()
		if networkBlockHashes != nil {
			if cachedHash, ok := networkBlockHashes[prevBlock.Height]; ok {
				prevHash = cachedHash
				callLogCallback("DEBUG", fmt.Sprintf("Using cached network hash for block %d: %x (prevHash check)", prevBlock.Height, prevHash[:8]))
			}
		}
		networkHashMutex.RUnlock()

		if block.PrevHash != prevHash {
			callLogCallback("ERROR", fmt.Sprintf("FORK DETECTED at height %d: prev hash mismatch", block.Height))
			callLogCallback("ERROR", fmt.Sprintf("  Local tip (height %d) hash: %x", ns.CurrentHeight, prevHash[:8]))
			callLogCallback("ERROR", fmt.Sprintf("  Received block prev hash: %x", block.PrevHash[:8]))
			callLogCallback("ERROR", "Local chain is forked - immediately clearing database and resyncing from genesis")

			// Get dataDir and network info from nodeState (we already hold ns.Lock(), so access directly)
			dataDirPath := ns.DataDir
			if dataDirPath == "" {
				dataDirPath = os.Getenv("HOME") + "/.archivas/data" // Fallback
			}
			networkID := ns.NetworkID
			if networkID == "" {
				networkID = "archivas-devnet-v4" // Fallback
			}

			// Release the nodeState lock before doing cleanup (to avoid deadlock)
			unlockNeeded = false
			ns.Unlock()

			// Get genesis path from stored value
			genesisPathMutex.RLock()
			recoveryGenesisPath := currentGenesisPath
			genesisPathMutex.RUnlock()
			if recoveryGenesisPath == "" {
				recoveryGenesisPath = "genesis/devnet.genesis.json" // Fallback
			}
			rpcBindAddr := "127.0.0.1:8080" // Default, will be updated if needed

			// Recover from fork (clears DB, reinitializes from genesis, continues running)
			if err := recoverFromFork(dataDirPath, recoveryGenesisPath, networkID, rpcBindAddr); err != nil {
				callLogCallback("ERROR", fmt.Sprintf("Fork recovery failed: %v", err))
				return fmt.Errorf("fork recovery failed: %w", err)
			}

			// Return error to stop current IBD attempt (it will restart automatically)
			return fmt.Errorf("prev hash mismatch at height %d (forked chain detected, database cleared and resyncing from genesis)", block.Height)
		}
	}

	for _, tx := range block.Txs {
		if tx.From == "coinbase" {
			receiver, ok := ns.WorldState.Accounts[tx.To]
			if !ok {
				receiver = &ledger.AccountState{Balance: 0, Nonce: 0}
				ns.WorldState.Accounts[tx.To] = receiver
			}
			receiver.Balance += tx.Amount
		} else {
			if err := ns.WorldState.ApplyTransaction(tx); err != nil {
				callLogCallback("WARN", fmt.Sprintf("Skipping invalid tx in block %d: %v", block.Height, err))
			}
		}
	}

	// CRITICAL: Check if we already have a block at this height (prevent duplicates)
	if int(block.Height) < len(ns.Chain) {
		existingBlock := ns.Chain[block.Height]
		existingHash := hashBlock(&existingBlock)
		newHash := hashBlock(&block)
		if existingHash != newHash {
			callLogCallback("ERROR", fmt.Sprintf("Block %d already exists with different hash! Existing: %x, New: %x",
				block.Height, existingHash[:8], newHash[:8]))
			return fmt.Errorf("block %d already exists with different hash (duplicate block creation detected)", block.Height)
		}
		// Same block, skip
		callLogCallback("DEBUG", fmt.Sprintf("Block %d already exists (hash: %x), skipping duplicate", block.Height, existingHash[:8]))
		return nil
	}

	// Verify we're not skipping heights
	if int(block.Height) != len(ns.Chain) {
		callLogCallback("ERROR", fmt.Sprintf("Height mismatch: chain length=%d, block height=%d", len(ns.Chain), block.Height))
		return fmt.Errorf("height mismatch: expected %d, got %d", len(ns.Chain), block.Height)
	}

	// Apply block to chain
	// Use network hash if available (for blocks without proof), otherwise calculate
	blockHash := hashBlock(&block)
	networkHashMutex.RLock()
	if networkBlockHashes != nil {
		if cachedHash, ok := networkBlockHashes[block.Height]; ok {
			blockHash = cachedHash
			callLogCallback("DEBUG", fmt.Sprintf("Applying block %d (using network hash: %x, prevHash: %x)", block.Height, blockHash[:8], block.PrevHash[:8]))
		} else {
			callLogCallback("DEBUG", fmt.Sprintf("Applying block %d (calculated hash: %x, prevHash: %x)", block.Height, blockHash[:8], block.PrevHash[:8]))
		}
	} else {
		callLogCallback("DEBUG", fmt.Sprintf("Applying block %d (calculated hash: %x, prevHash: %x)", block.Height, blockHash[:8], block.PrevHash[:8]))
	}
	networkHashMutex.RUnlock()
	ns.Chain = append(ns.Chain, block)
	ns.CurrentHeight = block.Height

	// Update consensus difficulty from block's difficulty
	// This ensures we use the network's actual difficulty, not the stored local value
	if block.Difficulty > 0 {
		oldDiff := ns.Consensus.DifficultyTarget
		ns.Consensus.DifficultyTarget = block.Difficulty
		if oldDiff != block.Difficulty && block.Height > 0 {
			callLogCallback("DEBUG", fmt.Sprintf("Updated difficulty from block %d: %d → %d", block.Height, oldDiff, block.Difficulty))
		}
		// Save updated difficulty to disk
		if ns.MetaStore != nil {
			if err := ns.MetaStore.SaveDifficulty(ns.Consensus.DifficultyTarget); err != nil {
				callLogCallback("WARN", fmt.Sprintf("Failed to save difficulty: %v", err))
			}
		}
	}

	// Update IBD progress tracking
	ibdProgressMutex.Lock()
	ibdLastAppliedHeight = block.Height
	ibdLastAppliedTime = time.Now()
	ibdAppliedBlocks++
	appliedCount := ibdAppliedBlocks
	ibdProgressMutex.Unlock()

	// Update genesis hash if this is the genesis block
	if block.Height == 0 {
		genesisHash := hashBlock(&block)
		ns.GenesisHash = genesisHash
		if ns.MetaStore != nil {
			if err := ns.MetaStore.SaveGenesisHash(genesisHash); err != nil {
				callLogCallback("WARN", fmt.Sprintf("Failed to save genesis hash: %v", err))
			}
		}
		callLogCallback("INFO", fmt.Sprintf("Genesis block saved (hash: %x)", genesisHash[:8]))
	}

	// Save block to disk
	if ns.BlockStore != nil {
		if err := ns.BlockStore.SaveBlock(block.Height, block); err != nil {
			callLogCallback("ERROR", fmt.Sprintf("Failed to save block %d to disk: %v", block.Height, err))
			return fmt.Errorf("failed to save block %d: %w", block.Height, err)
		}
	}

	// Update tip height
	if ns.MetaStore != nil {
		if err := ns.MetaStore.SaveTipHeight(block.Height); err != nil {
			callLogCallback("WARN", fmt.Sprintf("Failed to save tip height: %v", err))
		}
	}

	// Progress logging is handled by IBD progress monitor
	// Only log milestones to reduce spam
	if block.Height%10000 == 0 || (block.Height <= 1000 && block.Height%100 == 0) {
		callLogCallback("INFO", fmt.Sprintf("Applied block %d (total applied: %d)", block.Height, appliedCount))
	}

	return nil
}

// AcceptBlock is called by RPC when a farmer submits a block (for rpc.NodeState)
func (ns *NodeState) AcceptBlock(proof *pospace.Proof, farmerAddr string, farmerPubKey []byte) error {
	// CRITICAL: Reject block submissions during IBD - node must be read-only
	ibdRunningMutex.RLock()
	ibdIsRunning := ibdRunning
	ibdRunningMutex.RUnlock()

	if ibdIsRunning {
		return fmt.Errorf("IBD in progress - cannot accept blocks (node is read-only during sync)")
	}

	ns.Lock()
	defer ns.Unlock()

	nextHeight := ns.CurrentHeight + 1

	if err := ns.Consensus.VerifyProofOfSpace(proof, proof.Challenge); err != nil {
		return fmt.Errorf("invalid proof: %w", err)
	}

	pending := ns.Mempool.Pending()
	callLogCallback("INFO", fmt.Sprintf("Creating block %d with %d pending transactions", nextHeight, len(pending)))

	coinbase := ledger.Transaction{
		From:         "coinbase",
		To:           farmerAddr,
		Amount:       config.InitialBlockReward,
		Fee:          0,
		Nonce:        0,
		SenderPubKey: nil,
		Signature:    nil,
	}

	allTxs := []ledger.Transaction{coinbase}

	receiver, ok := ns.WorldState.Accounts[farmerAddr]
	if !ok {
		receiver = &ledger.AccountState{Balance: 0, Nonce: 0}
		ns.WorldState.Accounts[farmerAddr] = receiver
	}
	receiver.Balance += config.InitialBlockReward

	validTxs := []ledger.Transaction{}
	for _, tx := range pending {
		err := ns.WorldState.ApplyTransaction(tx)
		if err != nil {
			callLogCallback("WARN", fmt.Sprintf("Skipping invalid tx: %v", err))
		} else {
			validTxs = append(validTxs, tx)
		}
	}

	allTxs = append(allTxs, validTxs...)

	var prevHash [32]byte
	if nextHeight > 0 {
		prevBlock := ns.Chain[len(ns.Chain)-1]
		prevHash = hashBlock(&prevBlock)
	}

	newBlock := Block{
		Height:        nextHeight,
		TimestampUnix: time.Now().Unix(),
		PrevHash:      prevHash,
		Difficulty:    ns.Consensus.DifficultyTarget,
		Challenge:     ns.CurrentChallenge,
		Txs:           allTxs,
		Proof:         proof,
		FarmerAddr:    farmerAddr,
	}

	ns.Chain = append(ns.Chain, newBlock)
	ns.CurrentHeight = nextHeight
	ns.Mempool.Clear()

	newBlockHash := hashBlock(&newBlock)
	ns.CurrentChallenge = consensus.GenerateChallenge(newBlockHash, nextHeight+1)

	if ns.Consensus.DifficultyTarget > 1_000_000 {
		oldDiff := ns.Consensus.DifficultyTarget
		ns.Consensus.DifficultyTarget = ns.Consensus.DifficultyTarget / 2
		if ns.Consensus.DifficultyTarget < 1_000_000 {
			ns.Consensus.DifficultyTarget = 1_000_000
		}
		callLogCallback("INFO", fmt.Sprintf("Difficulty dropped: %d → %d", oldDiff, ns.Consensus.DifficultyTarget))
	}

	if ns.BlockStore != nil {
		if err := ns.BlockStore.SaveBlock(nextHeight, newBlock); err != nil {
			callLogCallback("ERROR", fmt.Sprintf("Failed to save block: %v", err))
		}
	}

	if ns.StateStore != nil {
		for addr, acc := range ns.WorldState.Accounts {
			if err := ns.StateStore.SaveAccount(addr, acc.Balance, acc.Nonce); err != nil {
				callLogCallback("ERROR", fmt.Sprintf("Failed to save account %s: %v", addr, err))
			}
		}
	}

	if ns.MetaStore != nil {
		if err := ns.MetaStore.SaveTipHeight(nextHeight); err != nil {
			callLogCallback("ERROR", fmt.Sprintf("Failed to save tip height: %v", err))
		}
		if err := ns.MetaStore.SaveDifficulty(ns.Consensus.DifficultyTarget); err != nil {
			callLogCallback("ERROR", fmt.Sprintf("Failed to save difficulty: %v", err))
		}
	}

	callLogCallback("INFO", fmt.Sprintf("Block %d accepted, new height: %d", nextHeight, ns.CurrentHeight))
	return nil
}

// Helper functions for ApplyBlock
func getString(m map[string]interface{}, key string) string {
	if v, ok := m[key].(string); ok {
		return v
	}
	return ""
}

func getInt64(m map[string]interface{}, key string) int64 {
	if v, ok := m[key].(float64); ok {
		return int64(v)
	}
	return 0
}

func getUint64(m map[string]interface{}, key string) uint64 {
	if v, ok := m[key].(float64); ok {
		return uint64(v)
	}
	return 0
}

// Dummy main function required for c-archive build mode
func main() {}
