if (UNIX)
  set(CMAKE_C_COMPILER icx)
  set(CMAKE_CXX_COMPILER icpx)
endif()

if (WIN32)
  set(CMAKE_C_COMPILER icx)
  set(CMAKE_CXX_COMPILER icx)
endif()

set(CMAKE_CXX_FLAGS_DEBUG_INIT "-fno-omit-frame-pointer -Xarch_host -fsanitize=thread -fsanitize-ignorelist=${CMAKE_CURRENT_LIST_DIR}/tsan-ignore.txt -fsanitize-recover=all")
set(CMAKE_C_FLAGS_DEBUG_INIT " -fno-omit-frame-pointer -Xarch_host -fsanitize=thread -fsanitize-recover=all")

set(PTI_SANITIZER ON CACHE BOOL "Build uses sanitizers")
