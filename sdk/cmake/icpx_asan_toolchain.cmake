if (UNIX)
  set(CMAKE_C_COMPILER icx)
  set(CMAKE_CXX_COMPILER icpx)
endif()

set(CMAKE_CXX_FLAGS_DEBUG_INIT "-fsanitize=address,undefined -fno-omit-frame-pointer -fno-optimize-sibling-calls")
set(CMAKE_C_FLAGS_DEBUG_INIT "-fsanitize=address,undefined")
