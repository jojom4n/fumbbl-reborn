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
# Mockup F dice (official-style block dice + red d6 — generated via
# tools/mockup/dice_queue.py + dice_process.py). This Qt build does not
# auto-process .qrc target sources, so rcc runs explicitly.
set(DICE_RCC ${CMAKE_CURRENT_BINARY_DIR}/qrc_dice.cpp)
# This Qt build does not export Qt6_rcc_EXECUTABLE — locate rcc explicitly.
if(NOT Qt6_rcc_EXECUTABLE)
    find_program(DICE_RCC_EXECUTABLE
        NAMES rcc
        PATHS /usr/lib/qt6 /usr/lib/qt6/bin /usr/bin
        NO_DEFAULT_PATH
    )
    if(NOT DICE_RCC_EXECUTABLE)
        message(FATAL_ERROR "rcc not found (needed for assets/dice.qrc)")
    endif()
else()
    set(DICE_RCC_EXECUTABLE ${Qt6_rcc_EXECUTABLE})
endif()
add_custom_command(
    OUTPUT ${DICE_RCC}
    COMMAND ${DICE_RCC_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice.qrc
            -o ${DICE_RCC}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice.qrc
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/block1.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/block2.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/block3.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/block4.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/block5.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/block6.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/d6_1.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/d6_2.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/d6_3.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/d6_4.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/d6_5.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/d6_6.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/block1_s.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/block2_s.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/block3_s.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/block4_s.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/block5_s.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/block6_s.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/d6_1_s.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/d6_2_s.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/d6_3_s.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/d6_4_s.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/d6_5_s.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/d6_6_s.png
    VERBATIM
)

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
    src/ui/rainsim.cpp
    src/ui/actionlog.h
    src/ui/actionlog.cpp
    src/ui/chatwidget.h
    src/ui/chatwidget.cpp
    src/ui/headerview.h
    src/ui/headerview.cpp
    src/ui/playerinfowidget.h
    src/ui/playerinfowidget.cpp
    src/ui/panel.h
    src/ui/panel.cpp

    ${DICE_RCC}
)

target_include_directories(fumbbl-qt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Mockup F dice (official-style block dice + red d6 — generated via
# tools/mockup/dice_queue.py + dice_process.py). This Qt build does not
# auto-process .qrc target sources, so rcc runs explicitly.
set(DICE_RCC ${CMAKE_CURRENT_BINARY_DIR}/qrc_dice.cpp)
# This Qt build does not export Qt6_rcc_EXECUTABLE — locate rcc explicitly.
if(NOT Qt6_rcc_EXECUTABLE)
    find_program(DICE_RCC_EXECUTABLE
        NAMES rcc
        PATHS /usr/lib/qt6 /usr/lib/qt6/bin /usr/bin
        NO_DEFAULT_PATH
    )
    if(NOT DICE_RCC_EXECUTABLE)
        message(FATAL_ERROR "rcc not found (needed for assets/dice.qrc)")
    endif()
else()
    set(DICE_RCC_EXECUTABLE ${Qt6_rcc_EXECUTABLE})
endif()
add_custom_command(
    OUTPUT ${DICE_RCC}
    COMMAND ${DICE_RCC_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice.qrc
            -o ${DICE_RCC}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice.qrc
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/block1.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/block2.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/block3.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/block4.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/block5.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/block6.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/d6_1.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/d6_2.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/d6_3.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/d6_4.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/d6_5.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/d6_6.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/block1_s.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/block2_s.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/block3_s.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/block4_s.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/block5_s.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/block6_s.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/d6_1_s.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/d6_2_s.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/d6_3_s.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/d6_4_s.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/d6_5_s.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/dice/d6_6_s.png
    VERBATIM
)
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

# Renders a captured .jsonl (reports_capturer output) through the REAL
# parser + model: a quick way to verify reportparser field alignment.
add_executable(reports_render
    tools/reports_render.cpp
    src/core/gamemodel.cpp
    src/core/reportparser.cpp
)
target_include_directories(reports_render PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(reports_render PRIVATE Qt6::Core Qt6::Gui)

# Headless LIVE raw-report capture (Mockup F round 9): spectates a match and
# dumps every raw serverModelSync report (one JSON line) + the full
# serverGameState. Ground truth for reportparser field alignment.
add_executable(reports_capturer
    tools/reports_capturer.cpp
    src/core/lzstring.cpp
    src/core/fumbblauth.cpp
    src/core/fumbblws.cpp
    src/core/spectatorsession.cpp
)
target_include_directories(reports_capturer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(reports_capturer PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::WebSockets
)

# Round 13 offscreen visual verification (grass texture + vector grid/TD,
# full-height dugouts + gap, move trail, 44px spaced dice, header glow).
add_executable(round13_preview
    tools/round13_preview.cpp
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
    src/ui/rainsim.cpp
    src/ui/actionlog.cpp
    src/ui/chatwidget.cpp
    src/ui/headerview.cpp
    src/ui/playerinfowidget.cpp
    src/ui/panel.cpp

    ${DICE_RCC}
)
target_include_directories(round13_preview PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(round13_preview PRIVATE
    FUMBBL_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
target_link_libraries(round13_preview PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::WebSockets
)

# Round 14 offscreen visual verification (between-cells move numbers:
# 3-step move, 1-square move, blitz).
add_executable(round14_preview
    tools/round14_preview.cpp
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
    src/ui/rainsim.cpp
    src/ui/actionlog.cpp
    src/ui/chatwidget.cpp
    src/ui/headerview.cpp
    src/ui/playerinfowidget.cpp
    src/ui/panel.cpp

    ${DICE_RCC}
)
target_include_directories(round14_preview PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(round14_preview PRIVATE
    FUMBBL_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
target_link_libraries(round14_preview PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::WebSockets
)

# Offscreen FieldView renderer for visual verification of the Mockup F
# design (feeds a real serverGameState fixture, saves a PNG).
add_executable(field_preview
    tools/field_preview.cpp
    src/core/assets.cpp
    src/ui/fieldview.cpp
    src/ui/rainsim.cpp
)
target_include_directories(field_preview PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(field_preview PRIVATE
    FUMBBL_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
target_link_libraries(field_preview PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
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
