# CMakeLists.txt for Go bridge with cgo

# Find Go compiler
find_program(GO_EXECUTABLE go REQUIRED)

# Set CGO environment
set(ENV{CGO_ENABLED} "1")

# Get Go version to ensure compatibility
execute_process(
    COMMAND ${GO_EXECUTABLE} version
    OUTPUT_VARIABLE GO_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "Found Go: ${GO_VERSION}")

# Output files from Go build
set(GO_BRIDGE_ARCHIVE "${CMAKE_CURRENT_BINARY_DIR}/libarchivas_bridge.a")
set(GO_BRIDGE_HEADER "${CMAKE_CURRENT_BINARY_DIR}/archivas_bridge.h")

# Go source files
set(GO_SOURCES
    node.go
    farmer.go
)

# Header files needed by cgo
set(C_HEADERS
    node.h
    farmer.h
)

# Build Go code with cgo as C archive
add_custom_command(
    OUTPUT ${GO_BRIDGE_ARCHIVE} ${GO_BRIDGE_HEADER}
    COMMAND ${GO_EXECUTABLE} build -buildmode=c-archive -o ${GO_BRIDGE_ARCHIVE} .
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${GO_SOURCES} ${C_HEADERS} go.mod
    COMMENT "Building Go bridge with cgo"
)

# Create imported library target (must be done after the custom command)
add_library(archivas_go_bridge STATIC IMPORTED GLOBAL)
set_target_properties(archivas_go_bridge PROPERTIES
    IMPORTED_LOCATION ${GO_BRIDGE_ARCHIVE}
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR};${CMAKE_CURRENT_SOURCE_DIR}"
    IMPORTED_LINK_INTERFACE_LANGUAGES "C"
)

# Custom target to ensure build
add_custom_target(archivas_go_bridge_target
    DEPENDS ${GO_BRIDGE_ARCHIVE} ${GO_BRIDGE_HEADER}
)

