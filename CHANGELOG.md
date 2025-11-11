# Changelog

All notable changes to Archivas Core GUI will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Initial release of Archivas Core GUI
- Node management (start/stop/restart)
- Farmer management (start/stop/restart)
- Plot creation functionality
- Real-time chain status monitoring
- Block and transaction browsing
- Background block sync monitor
- Automatic IBD (Initial Block Download)
- Fork recovery mechanism
- Genesis file bundled as Qt resource
- Cross-platform data directory support (QStandardPaths)

### Changed
- Updated README.md with current status
- Improved build documentation

### Fixed
- Fixed proof parsing from `/blocks/range` endpoint
- Fixed hash calculation to include proof hash
- Fixed difficulty initialization from network
- Fixed background sync monitor to handle string heights
- Fixed plot creation to use correct farmer public key

## [1.0.0] - 2025-11-12

### Added
- First stable release
- Complete node and farmer integration
- Plot creation from GUI
- Automatic blockchain synchronization
- Real-time farming with proof submission

