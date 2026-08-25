# ============================================================
# HwarangDo Compiler Core Sources
# ============================================================

file(GLOB_RECURSE HRD_CORE_SOURCES
    CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
)

# main.cpp는 실행 파일 전용이므로 core에서 제외한다.
list(REMOVE_ITEM HRD_CORE_SOURCES
    "${PROJECT_SOURCE_DIR}/src/main.cpp"
)

# ============================================================
# HwarangDo Compiler Core
# ============================================================

add_library(hrd_core STATIC
    ${HRD_CORE_SOURCES}
)

target_include_directories(hrd_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(hrd_core
    PUBLIC
        magic_enum::magic_enum
        llvm_interface

    PRIVATE
        hrd_runtime_headers
        tomlplusplus::tomlplusplus
)

# ============================================================
# HwarangDo Compiler Executable
# ============================================================

add_executable(hrd
    ${PROJECT_SOURCE_DIR}/src/main.cpp
)

target_link_libraries(hrd
    PRIVATE
        hrd_core
)

# ============================================================
# Compiler Output
# ============================================================

set_target_properties(hrd PROPERTIES
    OUTPUT_NAME "hrd"
)

message(STATUS "HwarangDo compiler output: hrd")