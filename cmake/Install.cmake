# ============================================================
# SDK Architecture Detection
# ============================================================

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" HRD_ARCH)

if(HRD_ARCH MATCHES "^(x86_64|amd64)$")
    set(HRD_ARCH "x86_64")
elseif(HRD_ARCH MATCHES "^(aarch64|arm64)$")
    set(HRD_ARCH "aarch64")
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
        set(HRD_TARGET_ABI "msvc")
    else()
        set(HRD_TARGET_ABI "gnu")
    endif()

    if(HRD_ARCH STREQUAL "x86_64")
        set(HRD_TARGET_NAME "x86_64-pc-windows-${HRD_TARGET_ABI}")
    elseif(HRD_ARCH STREQUAL "aarch64")
        set(HRD_TARGET_NAME "aarch64-pc-windows-${HRD_TARGET_ABI}")
    endif()

elseif(APPLE)

    if(HRD_ARCH STREQUAL "x86_64")
        set(HRD_TARGET_NAME "x86_64-apple-darwin")
    elseif(HRD_ARCH STREQUAL "aarch64")
        set(HRD_TARGET_NAME "aarch64-apple-darwin")
    endif()

elseif(UNIX)

    if(HRD_ARCH STREQUAL "x86_64")
        set(HRD_TARGET_NAME "x86_64-unknown-linux-gnu")
    elseif(HRD_ARCH STREQUAL "aarch64")
        set(HRD_TARGET_NAME "aarch64-unknown-linux-gnu")
    endif()

else()

    message(FATAL_ERROR
        "Unsupported HwarangDo SDK platform: ${CMAKE_SYSTEM_NAME}"
    )

endif()

if(NOT DEFINED HRD_TARGET_NAME)
    message(FATAL_ERROR
        "Failed to determine HwarangDo SDK target"
    )
endif()

message(STATUS "HwarangDo SDK target: ${HRD_TARGET_NAME}")

# ============================================================
# SDK Install Paths
# ============================================================

set(
    HRD_INSTALL_ROOT
    "${CMAKE_INSTALL_LIBDIR}/hwarangdo"
)

set(
    HRD_INSTALL_TARGET_DIR
    "${HRD_INSTALL_ROOT}/targets/${HRD_TARGET_NAME}"
)

set(
    HRD_INSTALL_SHARE_DIR
    "${CMAKE_INSTALL_DATADIR}/hwarangdo"
)

set(
    HRD_INSTALL_INCLUDE_DIR
    "${CMAKE_INSTALL_INCLUDEDIR}/hwarangdo"
)

# ============================================================
# Runtime Library Filename
# ============================================================

if(WIN32)
    set(HRD_RUNTIME_LIBRARY_NAME "hrd_runtime.lib")
else()
    set(HRD_RUNTIME_LIBRARY_NAME "libhrd_runtime.a")
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
    TARGETS hrd
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ============================================================
# Install Runtime
# ============================================================

install(
    TARGETS hrd_runtime

    ARCHIVE DESTINATION ${HRD_INSTALL_TARGET_DIR}
    LIBRARY DESTINATION ${HRD_INSTALL_TARGET_DIR}
    RUNTIME DESTINATION ${HRD_INSTALL_TARGET_DIR}
)

# ============================================================
# Install Runtime ABI Headers
# ============================================================

install(
    FILES
        ${PROJECT_SOURCE_DIR}/runtime/include/hrd_runtime.h

    DESTINATION
        ${HRD_INSTALL_INCLUDE_DIR}
)

# ============================================================
# Install SDK Manifest
# ============================================================

install(
    FILES
        ${PROJECT_BINARY_DIR}/sdk.json

    DESTINATION
        ${HRD_INSTALL_SHARE_DIR}
)

# ============================================================
# Installation Summary
# ============================================================

message(STATUS "HwarangDo SDK installation layout:")
message(STATUS "  compiler: ${CMAKE_INSTALL_BINDIR}")
message(STATUS "  runtime:  ${HRD_INSTALL_TARGET_DIR}")
message(STATUS "  headers:  ${HRD_INSTALL_INCLUDE_DIR}")
message(STATUS "  manifest: ${HRD_INSTALL_SHARE_DIR}/sdk.json")