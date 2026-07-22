# ============================================================
# SDK Architecture Detection
# ============================================================

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" HGM_ARCH)

if(HGM_ARCH MATCHES "^(x86_64|amd64)$")
  set(HGM_ARCH "x86_64")
elseif(HGM_ARCH MATCHES "^(aarch64|arm64)$")
  set(HGM_ARCH "aarch64")
else()
  message(FATAL_ERROR
    "Unsupported HwarangDo SDK architecture: ${CMAKE_SYSTEM_PROCESSOR}"
  )
endif()

# ============================================================
# SDK Target Detection
# ============================================================

if(WIN32)
  if(MSVC)
    set(HGM_TARGET_ABI "msvc")
  else()
    set(HGM_TARGET_ABI "gnu")
  endif()

  if(HGM_ARCH STREQUAL "x86_64")
    set(HGM_TARGET_NAME "x86_64-pc-windows-${HGM_TARGET_ABI}")
  elseif(HGM_ARCH STREQUAL "aarch64")
    set(HGM_TARGET_NAME "aarch64-pc-windows-${HGM_TARGET_ABI}")
  endif()

elseif(APPLE)
  if(HGM_ARCH STREQUAL "x86_64")
    set(HGM_TARGET_NAME "x86_64-apple-darwin")
  elseif(HGM_ARCH STREQUAL "aarch64")
    set(HGM_TARGET_NAME "aarch64-apple-darwin")
  endif()

elseif(UNIX)
  if(HGM_ARCH STREQUAL "x86_64")
    set(HGM_TARGET_NAME "x86_64-unknown-linux-gnu")
  elseif(HGM_ARCH STREQUAL "aarch64")
    set(HGM_TARGET_NAME "aarch64-unknown-linux-gnu")
  endif()

else()
  message(FATAL_ERROR
    "Unsupported HwarangDo SDK platform: ${CMAKE_SYSTEM_NAME}"
  )
endif()

if(NOT DEFINED HGM_TARGET_NAME)
  message(FATAL_ERROR
    "Failed to determine HwarangDo SDK target"
  )
endif()

message(STATUS "HwarangDo SDK target: ${HGM_TARGET_NAME}")

# ============================================================
# SDK Install Paths
# ============================================================

set(
  HGM_INSTALL_ROOT
  "${CMAKE_INSTALL_LIBDIR}/hwarangdo"
)

set(
  HGM_INSTALL_TARGET_DIR
  "${HGM_INSTALL_ROOT}/targets/${HGM_TARGET_NAME}"
)

set(
  HGM_INSTALL_SHARE_DIR
  "${CMAKE_INSTALL_DATADIR}/hwarangdo"
)

set(
  HGM_INSTALL_INCLUDE_DIR
  "${CMAKE_INSTALL_INCLUDEDIR}/hwarangdo"
)

# ============================================================
# Runtime Library Filename
# ============================================================

if(WIN32)
  set(HGM_RUNTIME_LIBRARY_NAME "hrd_runtime.lib")
else()
  set(HGM_RUNTIME_LIBRARY_NAME "libhrd_runtime.a")
endif()

# ============================================================
# SDK Manifest Generation
# ============================================================

configure_file(
  ${PROJECT_SOURCE_DIR}/cmake/sdk.json.in
  ${PROJECT_BINARY_DIR}/sdk.json
  @ONLY
)

# ============================================================
# Install Compiler
# ============================================================

install(
  TARGETS hgm

  RUNTIME DESTINATION
    ${CMAKE_INSTALL_BINDIR}
)

# ============================================================
# Install Runtime
# ============================================================

install(
  TARGETS hrd_runtime

  ARCHIVE DESTINATION
    ${HGM_INSTALL_TARGET_DIR}

  LIBRARY DESTINATION
    ${HGM_INSTALL_TARGET_DIR}

  RUNTIME DESTINATION
    ${HGM_INSTALL_TARGET_DIR}
)

# ============================================================
# Install Runtime ABI Headers
# ============================================================

install(
  FILES
    ${PROJECT_SOURCE_DIR}/runtime/include/hrd_runtime.h

  DESTINATION
    ${HGM_INSTALL_INCLUDE_DIR}
)

# ============================================================
# Install SDK Manifest
# ============================================================

install(
  FILES
    ${PROJECT_BINARY_DIR}/sdk.json

  DESTINATION
    ${HGM_INSTALL_SHARE_DIR}
)

# ============================================================
# Installation Summary
# ============================================================

message(STATUS "HwarangDo SDK installation layout:")
message(STATUS "  compiler: ${CMAKE_INSTALL_BINDIR}")
message(STATUS "  runtime:  ${HGM_INSTALL_TARGET_DIR}")
message(STATUS "  headers:  ${HGM_INSTALL_INCLUDE_DIR}")
message(STATUS "  manifest: ${HGM_INSTALL_SHARE_DIR}/sdk.json")