cmake_minimum_required(VERSION 3.16)
project(ArchivasCore VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable cgo
set(ENV{CGO_ENABLED} "1")

# Qt6 is preferred, but fallback to Qt5
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Network)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Build Go bridge with cgo
add_subdirectory(src/go/bridge)

# Resource files
set(QT_RESOURCES
    resources/icons/icon.qrc
    resources/genesis/genesis.qrc
)

# Source files
set(QT_SOURCES
    src/qt/main.cpp
    src/qt/archivasapplication.cpp
    src/qt/mainwindow.cpp
    src/qt/archivasnodemanager.cpp
    src/qt/archivasrpcclient.cpp
    src/qt/overviewpage.cpp
    src/qt/nodepage.cpp
    src/qt/farmerpage.cpp
    src/qt/blockspage.cpp
    src/qt/transactionspage.cpp
    src/qt/logspage.cpp
    src/qt/configmanager.cpp
    src/qt/settingsdialog.cpp
)

set(QT_HEADERS
    include/qt/archivasapplication.h
    include/qt/mainwindow.h
    include/qt/archivasnodemanager.h
    include/qt/archivasrpcclient.h
    include/qt/overviewpage.h
    include/qt/nodepage.h
    include/qt/farmerpage.h
    include/qt/blockspage.h
    include/qt/transactionspage.h
    include/qt/logspage.h
    include/qt/configmanager.h
    include/qt/settingsdialog.h
)

# Executable
add_executable(archivas-qt
    ${QT_SOURCES}
    ${QT_HEADERS}
    ${QT_RESOURCES}
)

target_link_libraries(archivas-qt
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    archivas_go_bridge
)

# Platform-specific libraries
if(APPLE)
    # macOS doesn't need explicit pthread or dl linking
    # These are automatically linked
elseif(UNIX)
    # Linux
    target_link_libraries(archivas-qt
        pthread
        dl
    )
endif()

# Ensure Go bridge is built before archivas-qt
add_dependencies(archivas-qt archivas_go_bridge_target)

# Include directories
target_include_directories(archivas-qt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qt
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/go/bridge
    ${CMAKE_CURRENT_BINARY_DIR}/src/go/bridge
)

# Install rules
install(TARGETS archivas-qt
    RUNTIME DESTINATION bin
)

