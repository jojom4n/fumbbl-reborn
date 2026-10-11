# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "RelWithDebInfo")
  file(REMOVE_RECURSE
  "CMakeFiles/fumbbl-qt_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/fumbbl-qt_autogen.dir/ParseCache.txt"
  "CMakeFiles/spectator_smoke_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/spectator_smoke_autogen.dir/ParseCache.txt"
  "fumbbl-qt_autogen"
  "spectator_smoke_autogen"
  "tests/CMakeFiles/lzstring_test_autogen.dir/AutogenUsed.txt"
  "tests/CMakeFiles/lzstring_test_autogen.dir/ParseCache.txt"
  "tests/lzstring_test_autogen"
  )
endif()
