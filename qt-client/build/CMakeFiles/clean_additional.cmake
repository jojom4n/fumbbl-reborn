# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "RelWithDebInfo")
  file(REMOVE_RECURSE
  "CMakeFiles/fumbbl-qt_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/fumbbl-qt_autogen.dir/ParseCache.txt"
  "fumbbl-qt_autogen"
  )
endif()
