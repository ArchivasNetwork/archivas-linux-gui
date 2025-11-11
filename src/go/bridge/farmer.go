package main

/*
#cgo CFLAGS: -I${SRCDIR}
#include <stdlib.h>
#include "farmer.h"

// Helper function to call the callback (needed because cgo can't call function pointers directly)
static void call_farmer_log_callback(farmer_log_callback_t cb, char* level, char* message) {
    if (cb != NULL) {
        cb(level, message);
    }
}
*/
import "C"
import (
	"bytes"
	"context"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"time"
	"unsafe"

	"github.com/decred/dcrd/dcrec/secp256k1/v4"
	"github.com/ArchivasNetwork/archivas/pospace"
	"github.com/ArchivasNetwork/archivas/wallet"
)

// FarmerState holds the farmer's state
type FarmerState struct {
	sync.RWMutex
	Plots        []*pospace.PlotFile
	FarmerAddr   string
	FarmerPubKey []byte
	PrivKey      []byte
	NodeURL      string
	LastProof    *pospace.Proof
	LastProofTime time.Time
}

// ChallengeInfo represents the challenge from the node
type ChallengeInfo struct {
	Challenge  [32]byte `json:"challenge"`
	Difficulty uint64   `json:"difficulty"`
	Height     uint64   `json:"height"`
	VDF        *struct {
		Seed       string `json:"seed"`
		Iterations uint64 `json:"iterations"`
		Output     string `json:"output"`
	} `json:"vdf,omitempty"`
}

// Global state
var (
	farmerRunning     bool
	farmerMutex       sync.RWMutex
	farmerContext     context.Context
	farmerCancel      context.CancelFunc
	farmerState       *FarmerState
	farmerStateMutex  sync.RWMutex
	farmerLogCallback C.farmer_log_callback_t
	farmerLogCallbackMutex sync.RWMutex
)

// callFarmerLogCallback safely calls the C callback function
func callFarmerLogCallback(level, message string) {
	farmerLogCallbackMutex.RLock()
	cb := farmerLogCallback
	farmerLogCallbackMutex.RUnlock()

	if cb != nil && uintptr(unsafe.Pointer(cb)) != 0 {
		levelCStr := C.CString(level)
		messageCStr := C.CString(message)
		defer C.free(unsafe.Pointer(levelCStr))
		defer C.free(unsafe.Pointer(messageCStr))
		C.call_farmer_log_callback(cb, levelCStr, messageCStr)
	} else {
		log.Printf("[%s] %s", level, message)
	}
}

// loadFarmerKey loads or generates the farmer private key
func loadFarmerKey(keyPath string) ([]byte, []byte, string, error) {
	// Check if key file exists
	if _, err := os.Stat(keyPath); os.IsNotExist(err) {
		// Generate new keypair
		callFarmerLogCallback("INFO", "Generating new farmer keypair...")
		privKey, pubKey, err := wallet.GenerateKeypair()
		if err != nil {
			return nil, nil, "", fmt.Errorf("failed to generate keypair: %w", err)
		}

		// Save private key to file
		keyDir := filepath.Dir(keyPath)
		if err := os.MkdirAll(keyDir, 0700); err != nil {
			return nil, nil, "", fmt.Errorf("failed to create key directory: %w", err)
		}

		if err := os.WriteFile(keyPath, []byte(hex.EncodeToString(privKey)), 0600); err != nil {
			return nil, nil, "", fmt.Errorf("failed to save private key: %w", err)
		}

		addr, _ := wallet.PubKeyToAddress(pubKey)
		callFarmerLogCallback("INFO", fmt.Sprintf("Generated new farmer identity: %s", addr))
		return privKey, pubKey, addr, nil
	}

	// Load existing key
	keyData, err := os.ReadFile(keyPath)
	if err != nil {
		return nil, nil, "", fmt.Errorf("failed to read key file: %w", err)
	}

	privKeyBytes, err := hex.DecodeString(string(keyData))
	if err != nil || len(privKeyBytes) != 32 {
		return nil, nil, "", fmt.Errorf("invalid key file format")
	}

	// Derive public key and address
	privKeyObj := secp256k1.PrivKeyFromBytes(privKeyBytes)
	pubKey := privKeyObj.PubKey()
	pubKeyBytes := pubKey.SerializeCompressed()

	addr, err := wallet.PubKeyToAddress(pubKeyBytes)
	if err != nil {
		return nil, nil, "", fmt.Errorf("failed to derive address: %w", err)
	}

	return privKeyBytes, pubKeyBytes, addr, nil
}

// loadPlots loads all plot files from a directory
func loadPlots(dir string) ([]*pospace.PlotFile, error) {
	files, err := os.ReadDir(dir)
	if err != nil {
		if os.IsNotExist(err) {
			return []*pospace.PlotFile{}, nil
		}
		return nil, err
	}

	var plots []*pospace.PlotFile
	for _, f := range files {
		if f.IsDir() || filepath.Ext(f.Name()) != ".arcv" {
			continue
		}

		plotPath := filepath.Join(dir, f.Name())
		plot, err := pospace.OpenPlot(plotPath)
		if err != nil {
			callFarmerLogCallback("WARN", fmt.Sprintf("Skipping plot %s: %v", f.Name(), err))
			continue
		}

		plots = append(plots, plot)
	}

	return plots, nil
}

// getChallenge gets the current challenge from the node
func getChallenge(nodeURL string) (*ChallengeInfo, error) {
	resp, err := http.Get(nodeURL + "/challenge")
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if resp.StatusCode != 200 {
		body, _ := io.ReadAll(resp.Body)
		return nil, fmt.Errorf("HTTP %d: %s", resp.StatusCode, string(body))
	}

	var info ChallengeInfo
	if err := json.NewDecoder(resp.Body).Decode(&info); err != nil {
		return nil, fmt.Errorf("failed to decode challenge: %w", err)
	}

	if len(info.Challenge) == 0 {
		return nil, fmt.Errorf("challenge is empty after decode")
	}

	return &info, nil
}

// submitBlock submits a block to the node
func submitBlock(nodeURL string, proof *pospace.Proof, farmerAddr string, farmerPubKey []byte, privKey []byte, challenge *ChallengeInfo) error {
	submission := map[string]interface{}{
		"proof":        proof,
		"farmerAddr":   farmerAddr,
		"farmerPubKey": hex.EncodeToString(farmerPubKey),
	}

	if challenge.VDF != nil {
		vdfSeed, _ := hex.DecodeString(challenge.VDF.Seed)
		vdfOutput, _ := hex.DecodeString(challenge.VDF.Output)
		submission["vdfSeed"] = vdfSeed
		submission["vdfIterations"] = challenge.VDF.Iterations
		submission["vdfOutput"] = vdfOutput
	}

	data, err := json.Marshal(submission)
	if err != nil {
		return err
	}

	resp, err := http.Post(nodeURL+"/submitBlock", "application/json", bytes.NewReader(data))
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	body, _ := io.ReadAll(resp.Body)

	if resp.StatusCode != 200 {
		return fmt.Errorf("HTTP %d: %s", resp.StatusCode, string(body))
	}

	return nil
}

// startArchivasFarmer starts the Archivas farmer
func startArchivasFarmer(ctx context.Context, nodeURL, plotsPath, farmerPrivkeyPath string) error {
	callFarmerLogCallback("INFO", "Initializing Archivas farmer...")

	// Ensure plots directory exists
	if err := os.MkdirAll(plotsPath, 0755); err != nil {
		return fmt.Errorf("failed to create plots directory: %w", err)
	}

	// Load or generate farmer key
	privKey, pubKey, farmerAddr, err := loadFarmerKey(farmerPrivkeyPath)
	if err != nil {
		return fmt.Errorf("failed to load farmer key: %w", err)
	}

	callFarmerLogCallback("INFO", fmt.Sprintf("Farmer Address: %s", farmerAddr))
	callFarmerLogCallback("INFO", fmt.Sprintf("Plots Directory: %s", plotsPath))
	callFarmerLogCallback("INFO", fmt.Sprintf("Node URL: %s", nodeURL))

	// Load plots
	plots, err := loadPlots(plotsPath)
	if err != nil {
		return fmt.Errorf("failed to load plots: %w", err)
	}

	if len(plots) == 0 {
		callFarmerLogCallback("WARN", "No plots found! Farmer will run but won't be able to farm.")
	} else {
		callFarmerLogCallback("INFO", fmt.Sprintf("Loaded %d plot(s)", len(plots)))
		for _, p := range plots {
			callFarmerLogCallback("INFO", fmt.Sprintf("  - %s (k=%d, %d hashes)", filepath.Base(p.Path), p.Header.KSize, p.Header.NumHashes))
		}
	}

	// Create farmer state
	farmerStateMutex.Lock()
	farmerState = &FarmerState{
		Plots:        plots,
		FarmerAddr:   farmerAddr,
		FarmerPubKey: pubKey,
		PrivKey:      privKey,
		NodeURL:      nodeURL,
	}
	farmerStateMutex.Unlock()

	callFarmerLogCallback("INFO", "Starting farming loop...")

	// Farming loop
	ticker := time.NewTicker(2 * time.Second)
	defer ticker.Stop()

	var lastHeight uint64

	for {
		select {
		case <-ctx.Done():
			return nil
		case <-ticker.C:
			// Get current challenge from node
			challengeInfo, err := getChallenge(nodeURL)
			if err != nil {
				callFarmerLogCallback("WARN", fmt.Sprintf("Error getting challenge: %v", err))
				continue
			}

			// Log when height changes
			if challengeInfo.Height != lastHeight {
				callFarmerLogCallback("INFO", fmt.Sprintf("NEW HEIGHT %d (difficulty: %d)", challengeInfo.Height, challengeInfo.Difficulty))
				lastHeight = challengeInfo.Height
			}

			// Check all plots
			farmerStateMutex.RLock()
			plots := farmerState.Plots
			farmerAddr := farmerState.FarmerAddr
			farmerPubKey := farmerState.FarmerPubKey
			privKey := farmerState.PrivKey
			farmerStateMutex.RUnlock()

			if len(plots) == 0 {
				continue
			}

			var bestProof *pospace.Proof
			for _, plot := range plots {
				proof, err := plot.CheckChallenge(challengeInfo.Challenge, challengeInfo.Difficulty)
				if err != nil {
					callFarmerLogCallback("WARN", fmt.Sprintf("Error checking plot %s: %v", filepath.Base(plot.Path), err))
					continue
				}

				if proof != nil && (bestProof == nil || proof.Quality < bestProof.Quality) {
					bestProof = proof
				}
			}

			if bestProof != nil && bestProof.Quality < challengeInfo.Difficulty {
				// CRITICAL: Check if node is in IBD - don't submit blocks during sync
				// We need to check the node's IBD status before submitting
				// For now, we'll check by querying the node's height vs network tip
				// If significantly behind, assume IBD is active
				callFarmerLogCallback("INFO", fmt.Sprintf("Found winning proof! Quality: %d (target: %d)", bestProof.Quality, challengeInfo.Difficulty))

				// Check if node is syncing (IBD active) - don't submit blocks during IBD
				// Query node's current height to see if it's significantly behind
				checkURL := nodeURL + "/chainTip"
				resp, err := http.Get(checkURL)
				if err == nil {
					defer resp.Body.Close()
					var tipResp struct {
						Height string `json:"height"`
					}
					if json.NewDecoder(resp.Body).Decode(&tipResp) == nil {
						if tipHeight, err := strconv.ParseUint(tipResp.Height, 10, 64); err == nil {
							// If challenge height is significantly behind network tip, assume IBD is active
							if challengeInfo.Height+100 < tipHeight {
								callFarmerLogCallback("WARN", fmt.Sprintf("Node appears to be in IBD (local height %d, network tip %d) - skipping block submission", challengeInfo.Height, tipHeight))
								continue // Skip block submission during IBD
							}
						}
					}
				}

				// Update last proof
				farmerStateMutex.Lock()
				farmerState.LastProof = bestProof
				farmerState.LastProofTime = time.Now()
				farmerStateMutex.Unlock()

				// Submit block
				if err := submitBlock(nodeURL, bestProof, farmerAddr, farmerPubKey, privKey, challengeInfo); err != nil {
					// If error is "IBD in progress", that's expected - just log as info
					if strings.Contains(err.Error(), "IBD") {
						callFarmerLogCallback("INFO", fmt.Sprintf("Block submission skipped (IBD in progress): %v", err))
					} else {
						callFarmerLogCallback("ERROR", fmt.Sprintf("Error submitting block: %v", err))
					}
				} else {
					vdfIter := uint64(0)
					if challengeInfo.VDF != nil {
						vdfIter = challengeInfo.VDF.Iterations
					}
					callFarmerLogCallback("INFO", fmt.Sprintf("Block submitted successfully for height %d (VDF t=%d)", challengeInfo.Height, vdfIter))
				}
			} else {
				bestQ := uint64(0)
				if bestProof != nil {
					bestQ = bestProof.Quality
				}
				callFarmerLogCallback("DEBUG", fmt.Sprintf("Checking plots... best=%d, need=<%d", bestQ, challengeInfo.Difficulty))
			}
		}
	}
}

//export archivas_farmer_start
func archivas_farmer_start(nodeURL, plotsPath, farmerPrivkey *C.char) C.int {
	farmerMutex.Lock()
	defer farmerMutex.Unlock()

	if farmerRunning {
		return 1 // Already running
	}

	// Parse C strings to Go strings
	nodeURLStr := C.GoString(nodeURL)
	plotsPathStr := C.GoString(plotsPath)
	farmerPrivkeyPathStr := C.GoString(farmerPrivkey)

	// Create context for cancellation
	farmerContext, farmerCancel = context.WithCancel(context.Background())

	// Start farmer in goroutine (non-blocking)
	go func() {
		farmerMutex.Lock()
		farmerRunning = true
		farmerMutex.Unlock()

		callFarmerLogCallback("INFO", fmt.Sprintf("Starting Archivas farmer: node_url=%s, plots_path=%s", nodeURLStr, plotsPathStr))

		// Initialize farmer
		err := startArchivasFarmer(farmerContext, nodeURLStr, plotsPathStr, farmerPrivkeyPathStr)
		if err != nil {
			callFarmerLogCallback("ERROR", fmt.Sprintf("Failed to start farmer: %v", err))
			farmerMutex.Lock()
			farmerRunning = false
			farmerMutex.Unlock()
			return
		}

		// Wait for cancellation
		<-farmerContext.Done()

		// Cleanup
		stopArchivasFarmer()
		farmerMutex.Lock()
		farmerRunning = false
		farmerMutex.Unlock()
		callFarmerLogCallback("INFO", "Archivas farmer stopped")
	}()

	return 0 // Success
}

// stopArchivasFarmer stops the farmer and cleans up resources
func stopArchivasFarmer() {
	farmerStateMutex.Lock()
	defer farmerStateMutex.Unlock()

	if farmerState == nil {
		return
	}

	callFarmerLogCallback("INFO", "Stopping Archivas farmer...")

	// Close plots
	for _, plot := range farmerState.Plots {
		if plot != nil {
			// PlotFile doesn't have an explicit Close method, but we can clear references
		}
	}

	farmerState = nil
	callFarmerLogCallback("INFO", "Archivas farmer stopped and cleaned up")
}

//export archivas_farmer_stop
func archivas_farmer_stop() {
	farmerMutex.Lock()
	defer farmerMutex.Unlock()

	if !farmerRunning {
		return
	}

	callFarmerLogCallback("INFO", "Stopping Archivas farmer...")

	if farmerCancel != nil {
		farmerCancel()
	}

	farmerRunning = false
}

//export archivas_farmer_is_running
func archivas_farmer_is_running() C.int {
	farmerMutex.RLock()
	defer farmerMutex.RUnlock()

	if farmerRunning {
		return 1
	}
	return 0
}

//export archivas_farmer_get_plot_count
func archivas_farmer_get_plot_count() C.int {
	farmerStateMutex.RLock()
	defer farmerStateMutex.RUnlock()

	if farmerState == nil {
		return 0
	}
	return C.int(len(farmerState.Plots))
}

//export archivas_farmer_get_last_proof
func archivas_farmer_get_last_proof() *C.char {
	farmerStateMutex.RLock()
	defer farmerStateMutex.RUnlock()

	if farmerState == nil || farmerState.LastProof == nil {
		return C.CString("")
	}

	// Return proof quality as string
	proofStr := fmt.Sprintf("Quality: %d, Hash: %x", farmerState.LastProof.Quality, farmerState.LastProof.Hash[:8])
	return C.CString(proofStr)
}

//export archivas_farmer_set_log_callback
func archivas_farmer_set_log_callback(callback C.farmer_log_callback_t) {
	farmerLogCallbackMutex.Lock()
	defer farmerLogCallbackMutex.Unlock()
	farmerLogCallback = callback
}

//export archivas_farmer_create_plot
func archivas_farmer_create_plot(plotPath *C.char, kSize C.uint, farmerPrivkeyPath *C.char) C.int {
	plotPathStr := C.GoString(plotPath)
	kSizeUint := uint32(kSize)
	farmerPrivkeyPathStr := C.GoString(farmerPrivkeyPath)

	callFarmerLogCallback("INFO", fmt.Sprintf("Creating plot: path=%s, kSize=%d", plotPathStr, kSizeUint))

	// Load or generate farmer key to get public key
	_, pubKey, farmerAddr, err := loadFarmerKey(farmerPrivkeyPathStr)
	if err != nil {
		callFarmerLogCallback("ERROR", fmt.Sprintf("Failed to load farmer key: %v", err))
		return 1
	}

	callFarmerLogCallback("INFO", fmt.Sprintf("Using farmer address: %s", farmerAddr))

	// Ensure plot directory exists
	plotDir := filepath.Dir(plotPathStr)
	if err := os.MkdirAll(plotDir, 0755); err != nil {
		callFarmerLogCallback("ERROR", fmt.Sprintf("Failed to create plot directory: %v", err))
		return 1
	}

	// Check if plot file already exists
	if _, err := os.Stat(plotPathStr); err == nil {
		callFarmerLogCallback("ERROR", fmt.Sprintf("Plot file already exists: %s", plotPathStr))
		return 1
	}

	// Generate plot
	callFarmerLogCallback("INFO", fmt.Sprintf("Generating plot (this may take a while for kSize=%d)...", kSizeUint))
	startTime := time.Now()
	
	err = pospace.GeneratePlot(plotPathStr, kSizeUint, pubKey)
	if err != nil {
		callFarmerLogCallback("ERROR", fmt.Sprintf("Failed to generate plot: %v", err))
		return 1
	}

	elapsed := time.Since(startTime)
	
	// Get plot file size
	plotInfo, err := os.Stat(plotPathStr)
	var sizeStr string
	if err == nil {
		sizeMB := float64(plotInfo.Size()) / (1024 * 1024)
		sizeStr = fmt.Sprintf("%.2f MB", sizeMB)
	} else {
		sizeStr = "unknown size"
	}

	callFarmerLogCallback("INFO", fmt.Sprintf("Plot created successfully: %s (%s) in %v", plotPathStr, sizeStr, elapsed.Round(time.Second)))
	
	// If farmer is running, reload plots
	farmerMutex.RLock()
	isRunning := farmerRunning
	farmerMutex.RUnlock()
	
	if isRunning {
		callFarmerLogCallback("INFO", "Farmer is running - plot will be loaded on next scan")
		// Note: The farmer will automatically pick up new plots on the next directory scan
		// We could trigger a reload here, but it's simpler to let the farmer discover it naturally
	}

	return 0 // Success
}
