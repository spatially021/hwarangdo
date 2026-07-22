# ============================================================
# External Libraries
# ============================================================

add_subdirectory(
  ${PROJECT_SOURCE_DIR}/external/magic_enum
  ${PROJECT_BINARY_DIR}/external/magic_enum
)

# ============================================================
# LLVM
# ============================================================

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVM config directory: ${LLVM_DIR}")

# LLVM 관련 설정을 하나의 INTERFACE 타깃으로 묶는다.
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