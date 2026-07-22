# ============================================================
# HwarangDo Runtime ABI Headers
# ============================================================

# 실제 바이너리를 만들지 않고 include 경로만 전달하는 타깃이다.
add_library(hrd_runtime_headers INTERFACE)

target_include_directories(hrd_runtime_headers
  INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/runtime/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/hwarangdo>
)

# ============================================================
# HwarangDo Runtime Library
# ============================================================

add_library(hrd_runtime STATIC
  ${PROJECT_SOURCE_DIR}/runtime/world/hrd_runtime_world.cpp
  ${PROJECT_SOURCE_DIR}/runtime/log/hrd_runtime_log.cpp
  ${PROJECT_SOURCE_DIR}/runtime/string/hrd_runtime_string.cpp
)

# 현재 생성되는 이름:
#
# Linux/macOS:
#   libhrd_runtime.a
#
# Windows:
#   hrd_runtime.lib
set_target_properties(hrd_runtime PROPERTIES
  OUTPUT_NAME "hrd_runtime"
)

# 런타임 구현이 hrd_runtime.h를 찾을 수 있도록 한다.
target_link_libraries(hrd_runtime
  PRIVATE
    hrd_runtime_headers
)