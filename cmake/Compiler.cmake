# ============================================================
# HwarangDo Compiler Core Sources
# ============================================================

file(GLOB_RECURSE HGM_CORE_SOURCES
  CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/src/*.cpp"
)

# main.cpp는 실행 파일 전용이므로 core에서 제외한다.
list(REMOVE_ITEM HGM_CORE_SOURCES
  "${PROJECT_SOURCE_DIR}/src/main.cpp"
)

# ============================================================
# HwarangDo Compiler Core
# ============================================================

add_library(hgm_core STATIC
  ${HGM_CORE_SOURCES}
)

target_include_directories(hgm_core
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(hgm_core
  PUBLIC
    magic_enum::magic_enum
    llvm_interface

  PRIVATE
    hrd_runtime_headers
)

# ============================================================
# Compiler Executable
# ============================================================

add_executable(hgm
  ${PROJECT_SOURCE_DIR}/src/main.cpp
)

target_link_libraries(hgm
  PRIVATE
    hgm_core
)

# ============================================================
# Platform-specific Link Options
# ============================================================

# 현재 컴파일러가 런타임 심볼을 직접 노출해야 하는 구조를 위해 유지한다.
if(UNIX AND NOT APPLE)
  target_link_options(hgm
    PRIVATE
      -rdynamic
  )
endif()

# ============================================================
# Compiler Output Name
# ============================================================

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(HGM_COMPILER_OUTPUT_NAME "hwarangdo")
elseif(HGM_BUILD_MODE STREQUAL "ASAN")
  set(HGM_COMPILER_OUTPUT_NAME "hrd_asan")
elseif(HGM_BUILD_MODE STREQUAL "IDE")
  set(HGM_COMPILER_OUTPUT_NAME "hrd_ide")
else()
  set(HGM_COMPILER_OUTPUT_NAME "hrd_debug")
endif()

set_target_properties(hgm PROPERTIES
  OUTPUT_NAME "${HGM_COMPILER_OUTPUT_NAME}"
)

message(STATUS "HwarangDo compiler output: ${HGM_COMPILER_OUTPUT_NAME}")