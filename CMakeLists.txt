# =============================================================================
# FUMBBL Reborn — Qt6 C++ client
# Native desktop client for the FUMBBL online platform.
# =============================================================================
cmake_minimum_required(VERSION 3.21)

project(fumbbl-qt
    VERSION 0.1.1
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
# This Qt build does not export Qt6_rcc_EXECUTABLE — locate rcc explicitly:
# first the Qt install's own bin (Qt6_DIR is <qt>/lib/cmake/Qt6, so the
# tools are at <qt>/bin = three levels up), then the system paths.
if(NOT Qt6_rcc_EXECUTABLE)
    # Qt6_DIR is <qt>/lib/cmake/Qt6. The tools live in <qt>/bin for
    # official/aqt installs and in <qt>/libexec for source builds.
    find_program(DICE_RCC_EXECUTABLE
        NAMES rcc rcc.exe
        HINTS "${Qt6_DIR}/../../../bin" "${Qt6_DIR}/../../../libexec"
        PATHS /usr/lib/qt6 /usr/lib/qt6/bin /usr/bin
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

# The shipped binary is named FUMBBL-Reborn (the CMake target stays
# fumbbl-qt).
set_target_properties(fumbbl-qt PROPERTIES OUTPUT_NAME "FUMBBL-Reborn")

target_include_directories(fumbbl-qt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Game assets: cartoon faces (assets/faces, per roster piece) + procedural
# skill/card icons (see gen_assets). Committed under <source>/assets.
# FUMBBL_VERSION: project version, shown in the window title and About box.
target_compile_definitions(fumbbl-qt PRIVATE
    FUMBBL_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
    FUMBBL_VERSION="${PROJECT_VERSION}")

target_link_libraries(fumbbl-qt PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::WebSockets
)

# Release bundle: the executable finds the Qt libraries next to it
# (tools/make_bundle.sh installs into the bundle folder). Windows resolves
# DLLs from the exe directory natively — no rpath needed.
if(NOT WIN32)
    # DT_RPATH (not DT_RUNPATH): the search path propagates to the Qt
    # libraries' own transitive dependencies (libQt6DBus, libQt6XcbQpa, ...),
    # so the bundle resolves everything from $ORIGIN.
    set_target_properties(fumbbl-qt PROPERTIES INSTALL_RPATH "$ORIGIN")
    target_link_options(fumbbl-qt PRIVATE "LINKER:--disable-new-dtags")
endif()

# Install rules (release bundles via tools/make_bundle.sh).
install(TARGETS fumbbl-qt RUNTIME DESTINATION .)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/assets/ DESTINATION assets)

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

# Headless LIVE raw-report capture: spectates a match and dumps every raw
# serverModelSync report + model change (one JSON line each) + the full
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

# Offscreen HUD verification: drives a real MainWindow offscreen (live game
# fixture + synthetic reports/changes) and saves PNG crops — window, field,
# dugouts, header, action log, block glows/markers, move trail, ball flight,
# kickoff, rain.
add_executable(hud_preview
    tools/hud_preview.cpp
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
target_include_directories(hud_preview PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(hud_preview PRIVATE
    FUMBBL_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
target_link_libraries(hud_preview PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::WebSockets
)

# Offscreen verification of the move-trail rendering (between-cells step
# numbers, block marks, go-for-it red trail, log line wrapping): a full
# MainWindow fed with the exact wire flows of the ffb-server block/move
# sequences.
add_executable(move_numbers_preview
    tools/move_numbers_preview.cpp
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
target_include_directories(move_numbers_preview PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(move_numbers_preview PRIVATE
    FUMBBL_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
target_link_libraries(move_numbers_preview PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::WebSockets
)

# Offscreen FieldView renderer: feeds a raw serverGameState fixture into a
# bare FieldView and saves a PNG (quick check of field/stadium/dugout look).
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
