# =============================================================================
# FUMBBL Reborn (Qt6) — unit tests
# =============================================================================
find_package(Qt6 REQUIRED COMPONENTS Test)

# -----------------------------------------------------------------------------
# lzstring_test — LZString UTF16 codec (the FUMBBL WS compression)
# -----------------------------------------------------------------------------
add_executable(lzstring_test
    lzstring_test.cpp
    ${CMAKE_SOURCE_DIR}/src/core/lzstring.cpp
)
target_include_directories(lzstring_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(lzstring_test PRIVATE Qt6::Test Qt6::Core)
add_test(NAME lzstring_test COMMAND $<TARGET_FILE:lzstring_test>)
set_tests_properties(lzstring_test PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests)

# -----------------------------------------------------------------------------
# gamemodel_test — GameModel (port of the TS fumbblGameModel.ts)
# -----------------------------------------------------------------------------
add_executable(gamemodel_test
    gamemodel_test.cpp
    ${CMAKE_SOURCE_DIR}/src/core/gamemodel.cpp
)
target_include_directories(gamemodel_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(gamemodel_test PRIVATE Qt6::Test Qt6::Core)
add_test(NAME gamemodel_test COMMAND $<TARGET_FILE:gamemodel_test>)
set_tests_properties(gamemodel_test PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests)

# -----------------------------------------------------------------------------
# ui_wiring_test — core -> UI wiring (offscreen)
# -----------------------------------------------------------------------------
add_executable(ui_wiring_test
    ui_wiring_test.cpp
    ${CMAKE_SOURCE_DIR}/src/core/lzstring.cpp
    ${CMAKE_SOURCE_DIR}/src/core/fumbblauth.cpp
    ${CMAKE_SOURCE_DIR}/src/core/fumbblws.cpp
    ${CMAKE_SOURCE_DIR}/src/core/spectatorsession.cpp
    ${CMAKE_SOURCE_DIR}/src/core/gamemodel.cpp
    ${CMAKE_SOURCE_DIR}/src/core/reportparser.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/mainwindow.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/connectdialog.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/fieldview.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/headerbar.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/rosterwidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/dicelogwidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/chatwidget.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/playerinfowidget.cpp
)
target_include_directories(ui_wiring_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ui_wiring_test PRIVATE
    Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::WebSockets
    ads::qtadvanceddocking
)
add_test(NAME ui_wiring_test COMMAND $<TARGET_FILE:ui_wiring_test>)
set_tests_properties(ui_wiring_test PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
