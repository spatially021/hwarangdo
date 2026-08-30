include(GNUInstallDirs)

add_library(hrd_runtime_headers INTERFACE)

target_include_directories(hrd_runtime_headers
    INTERFACE
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/runtime/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/hwarangdo>
)

add_library(hrd_runtime STATIC
    ${PROJECT_SOURCE_DIR}/runtime/world/hrd_runtime_world.cpp
    ${PROJECT_SOURCE_DIR}/runtime/log/hrd_runtime_log.cpp
    ${PROJECT_SOURCE_DIR}/runtime/string/hrd_s8.cpp
    ${PROJECT_SOURCE_DIR}/runtime/string/hrd_s16.cpp
    ${PROJECT_SOURCE_DIR}/runtime/string/hrd_s32.cpp
    ${PROJECT_SOURCE_DIR}/runtime/string/cast.cpp
    ${PROJECT_SOURCE_DIR}/runtime/runtimeSafety.cpp
)

set_target_properties(hrd_runtime PROPERTIES
    OUTPUT_NAME "hrd_runtime"
    INTERPROCEDURAL_OPTIMIZATION FALSE
)

target_link_libraries(hrd_runtime
    PRIVATE
        hrd_runtime_headers
)

install(
    TARGETS hrd_runtime
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(
    DIRECTORY ${PROJECT_SOURCE_DIR}/runtime/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)