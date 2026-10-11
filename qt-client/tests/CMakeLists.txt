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
target_link_libraries(lzstring_test PRIVATE Qt6::Test Qt6::Core)
add_test(NAME lzstring_test COMMAND lzstring_test)
