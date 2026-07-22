cmake_minimum_required(VERSION 3.21)

project(HwaranGeom VERSION 0.0.0 LANGUAGES C CXX)

# GNU 표준 설치 디렉토리:
# CMAKE_INSTALL_BINDIR, LIBDIR, INCLUDEDIR, DATADIR 등을 정의한다.
include(GNUInstallDirs)

# ============================================================
# Global C++ Policy
# ============================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# compile_commands.json 생성
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================
# Project Modules
# ============================================================
include(cmake/Warnings.cmake)
include(cmake/Dependencies.cmake)
include(cmake/Runtime.cmake)
include(cmake/Compiler.cmake)
include(cmake/Install.cmake)