# =============================================================================
# FUMBBL Reborn — Qt6 C++ client
# Native desktop client for the FUMBBL online Blood Bowl platform.
# =============================================================================
cmake_minimum_required(VERSION 3.21)

project(fumbbl-qt
    VERSION 0.1.0
    DESCRIPTION "FUMBBL Reborn — native Qt6 client"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

# Qt6CoreMacros may later force CMAKE_RUNTIME_OUTPUT_DIRECTORY; pin it up front
# so both the app and the tests land in predictable locations.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network WebSockets)

# -----------------------------------------------------------------------------
# Application
# -----------------------------------------------------------------------------
add_executable(fumbbl-qt
    src/main.cpp

    # Core (protocol layer)
    src/core/lzstring.h
    src/core/lzstring.cpp

    # UI
    src/ui/mainwindow.h
    src/ui/mainwindow.cpp
    src/ui/fieldview.h
    src/ui/fieldview.cpp
    src/ui/rosterwidget.h
    src/ui/rosterwidget.cpp
    src/ui/dicelogwidget.h
    src/ui/dicelogwidget.cpp
    src/ui/chatwidget.h
    src/ui/chatwidget.cpp
    src/ui/headerbar.h
    src/ui/headerbar.cpp
    src/ui/playerinfowidget.h
    src/ui/playerinfowidget.cpp
)

target_include_directories(fumbbl-qt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(fumbbl-qt PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::WebSockets
)

# -----------------------------------------------------------------------------
# Tests
# -----------------------------------------------------------------------------
option(BUILD_TESTING "Build unit tests" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
