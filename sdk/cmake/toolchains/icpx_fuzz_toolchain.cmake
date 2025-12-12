if(UNIX)
  set(CMAKE_C_COMPILER icx)
  set(CMAKE_CXX_COMPILER icpx)
  # This seems related? https://github.com/google/oss-fuzz/issues/713
  set(CMAKE_C_STANDARD_LIBRARIES "-lubsan")
endif()

if(WIN32)
  set(CMAKE_C_COMPILER icx)
  set(CMAKE_CXX_COMPILER icx)
endif()

set(CMAKE_CXX_FLAGS_DEBUG_INIT
    "-fsanitize=address -Xarch_host -fsanitize=undefined -fno-omit-frame-pointer -fno-optimize-sibling-calls -fsanitize-ignorelist=${PROJECT_SOURCE_DIR}/test/fuzz/ubsan-ignore.txt"
)
set(CMAKE_C_FLAGS_DEBUG_INIT
    "-fsanitize=address -Xarch_host -fsanitize=undefined")
