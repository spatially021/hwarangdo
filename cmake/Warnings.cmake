# ============================================================
# Compiler Warnings
# ============================================================

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Wcast-align
    -Woverloaded-virtual
    -Wconversion
    -Wsign-conversion
  )
elseif(MSVC)
  add_compile_options(
    /W4
    /permissive-
  )
endif()