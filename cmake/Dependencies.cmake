# ============================================================
# External Libraries
# ============================================================

add_subdirectory(
  ${PROJECT_SOURCE_DIR}/external/magic_enum
  ${PROJECT_BINARY_DIR}/external/magic_enum
)

include(FetchContent)

# ============================================================
# toml++
# ============================================================

FetchContent_Declare(
  tomlplusplus
  GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
  GIT_TAG v3.4.0
  GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(tomlplusplus)

# ============================================================
# miniz
# ============================================================

FetchContent_Declare(
  miniz
  GIT_REPOSITORY https://github.com/richgel999/miniz.git
  GIT_TAG 3.1.0
  GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(miniz)

# ============================================================
# LLVM
# ============================================================

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVM config directory: ${LLVM_DIR}")

add_library(llvm_interface INTERFACE)

target_include_directories(llvm_interface
  INTERFACE
    ${LLVM_INCLUDE_DIRS}
)

target_compile_definitions(llvm_interface
  INTERFACE
    ${LLVM_DEFINITIONS}
)

target_link_libraries(llvm_interface
  INTERFACE
    LLVM
)