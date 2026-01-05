if(UNIX)
  set(CMAKE_C_COMPILER icx)
  set(CMAKE_CXX_COMPILER icpx)

  # Work around for not having libubsan on some platforms.
  find_library(
    Ubsan_LIBRARY
    NAMES ubsan libubsan.so.1
    HINTS ENV LIBRARY_PATH
    PATHS ${CMAKE_LIBRARY_PATH}
          ENV LD_LIBRARY_PATH
          /usr/lib/x86_64-linux-gnu
          /usr/lib64
          /usr/lib
    PATH_SUFFIXES lib
                  lib/${CMAKE_LIBRARY_ARCHITECTURE}
                  lib/x86_64-linux-gnu
                  lib64
                  lib64/${CMAKE_LIBRARY_ARCHITECTURE}
                  lib64/x86_64-linux-gnu
    NO_CMAKE_FIND_ROOT_PATH
  )

  if (Ubsan_LIBRARY)
    # This seems related? https://github.com/google/oss-fuzz/issues/713
    set(CMAKE_C_STANDARD_LIBRARIES "-lubsan")
  endif()
endif()

if(WIN32)
  set(CMAKE_C_COMPILER icx)
  set(CMAKE_CXX_COMPILER icx)
endif()

set(CMAKE_CXX_FLAGS_DEBUG_INIT
    "-Xarch_host -fsanitize=address -Xarch_host -fsanitize=undefined -fno-omit-frame-pointer -fno-optimize-sibling-calls -fsanitize-ignorelist=${PROJECT_SOURCE_DIR}/test/fuzz/ubsan-ignore.txt"
)
set(CMAKE_C_FLAGS_DEBUG_INIT
    "-Xarch_host -fsanitize=address -Xarch_host -fsanitize=undefined")
