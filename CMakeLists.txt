# =============================================================================
# FUMBBL Reborn — Qt6 C++ client
# Native desktop client for the FUMBBL online platform.
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
    src/core/fumbblauth.h
    src/core/fumbblauth.cpp
    src/core/fumbblws.h
    src/core/fumbblws.cpp
    src/core/spectatorsession.h
    src/core/spectatorsession.cpp
    src/core/gamemodel.h
    src/core/gamemodel.cpp
    src/core/reportparser.h
    src/core/reportparser.cpp
    src/core/assets.h
    src/core/assets.cpp
    src/core/matchlist.h
    src/core/matchlist.cpp

    # UI
    src/ui/mainwindow.h
    src/ui/mainwindow.cpp
    src/ui/spectatedialog.h
    src/ui/spectatedialog.cpp
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
    src/ui/panel.h
    src/ui/panel.cpp
)

target_include_directories(fumbbl-qt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
# Game assets: cartoon faces (assets/faces, per roster piece) + procedural
# skill/card icons (see gen_assets). Committed under <source>/assets.
target_compile_definitions(fumbbl-qt PRIVATE
    FUMBBL_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")

target_link_libraries(fumbbl-qt PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::WebSockets
)

# -----------------------------------------------------------------------------
# Headless spectator smoke tool (live network verification)
# -----------------------------------------------------------------------------
add_executable(spectator_smoke
    tools/spectator_smoke.cpp
    src/core/lzstring.cpp
    src/core/fumbblauth.cpp
    src/core/fumbblws.cpp
    src/core/spectatorsession.cpp
)
target_include_directories(spectator_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(spectator_smoke PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::WebSockets
)

# Headless reproduction of the REAL UI spectate path (MainWindow +
# clearMatchUi + session->connectAsSpectator) — diagnostic for join problems.
add_executable(ui_connect_probe
    tools/ui_connect_probe.cpp
    src/core/lzstring.cpp
    src/core/fumbblauth.cpp
    src/core/fumbblws.cpp
    src/core/spectatorsession.cpp
    src/core/gamemodel.cpp
    src/core/reportparser.cpp
    src/core/assets.cpp
    src/core/matchlist.cpp
    src/ui/mainwindow.cpp
    src/ui/spectatedialog.cpp
    src/ui/fieldview.cpp
    src/ui/rosterwidget.cpp
    src/ui/dicelogwidget.cpp
    src/ui/chatwidget.cpp
    src/ui/headerbar.cpp
    src/ui/playerinfowidget.cpp
    src/ui/panel.cpp
)
target_include_directories(ui_connect_probe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(ui_connect_probe PRIVATE
    FUMBBL_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
target_link_libraries(ui_connect_probe PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::WebSockets
)

# -----------------------------------------------------------------------------
# gen_assets — procedural icon generator (skill/card PNGs with QPainter;
# no external art). Output: <source>/assets/{skills,cards}. Player faces are
# in <source>/assets/faces (one-off generator, committed).
# Run: cmake --build build --target gen_assets && ./build/gen_assets [dir]
# -----------------------------------------------------------------------------
add_executable(gen_assets tools/gen_assets.cpp)
target_link_libraries(gen_assets PRIVATE Qt6::Core Qt6::Gui)
add_custom_target(generate_assets
    COMMAND $<TARGET_FILE:gen_assets> ${CMAKE_CURRENT_SOURCE_DIR}/assets
    DEPENDS gen_assets
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating fantasy PNG assets into assets/")

# -----------------------------------------------------------------------------
# Tests
# -----------------------------------------------------------------------------
option(BUILD_TESTING "Build unit tests" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
