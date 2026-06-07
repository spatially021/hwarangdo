cmake_minimum_required(VERSION 3.21)

project(HwaranGeom VERSION 0.1.0 LANGUAGES C CXX)

# ============================================================
# Global C++ Policy
# ============================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================
# Warnings
# ============================================================
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(
    -Wall -Wextra -Wpedantic
    -Wshadow -Wnon-virtual-dtor
    -Wold-style-cast -Wcast-align
    -Woverloaded-virtual
    -Wconversion -Wsign-conversion
  )
endif()

# ============================================================
# External Library
# ============================================================
add_subdirectory(external/magic_enum)

# ============================================================
# Core Library
# ============================================================
add_library(hgm_core STATIC
  src/lexer/lexer.cpp
  src/parser/Parser.cpp
  src/SemanticAnalyzer/SemanticAnalyzer.cpp

  src/parser/parserDecl.cpp
  src/parser/parserExpr.cpp
  src/parser/parserStmt.cpp
  src/parser/parserUtil.cpp

  src/SemanticAnalyzer/Builder.cpp

  src/SemanticAnalyzer/Resolver/Resolver.cpp
  src/SemanticAnalyzer/Resolver/ResolverDecl.cpp
  src/SemanticAnalyzer/Resolver/ResolverExpr.cpp
  src/SemanticAnalyzer/Resolver/ResolverStmt.cpp
  src/SemanticAnalyzer/Resolver/ResolverUtil.cpp
  src/SemanticAnalyzer/Resolver/ResolveNum.cpp
  src/SemanticAnalyzer/Resolver/ResolveChar.cpp
  src/SemanticAnalyzer/Resolver/ResolveCall.cpp


  src/SemanticAnalyzer/SymbolTable.cpp
  src/SemanticAnalyzer/Linker.cpp
  src/SemanticAnalyzer/Verifier.cpp
  src/SemanticAnalyzer/symbol/TypeSymbol.cpp
  src/SemanticAnalyzer/Scope.cpp

  src/IR/HIR/HIRBuilder.cpp
  src/IR/HIR/HIRBuilderDecl.cpp
  src/IR/HIR/HIRBuilderExpr.cpp
  src/IR/HIR/HIRBuilderStmt.cpp
  src/IR/HIR/HIRVisitor.cpp
  src/IR/HIR/HIRUtil.cpp
  src/IR/HIR/HIRBuilderType.cpp
  src/IR/HIR/HIRBuilderPlace.cpp
  src/IR/HIR/HIRVerifier.cpp

src/IR/HIR/HIRHelper.cpp

  src/IR/HIR/HIRLinker.cpp

  src/util/Printor.cpp
  src/util/Error.cpp
  src/util/Helper.cpp

  src/Debugger/ParserDebugger.cpp
  src/Debugger/ResolverDebugger.cpp
  src/Debugger/BuilderDebugger.cpp
  src/IR/MIR/MIRBuilder.cpp
  src/IR/MIR/MIRUtil.cpp
  src/IR/MIR/MIRStmtBuilder.cpp
  
)

target_include_directories(hgm_core PUBLIC
  ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(hgm_core PUBLIC magic_enum::magic_enum)
# ============================================================
# Executable
# ============================================================
add_executable(hgm src/main.cpp)
target_link_libraries(hgm PRIVATE hgm_core)

# ============================================================
# Debug Configuration
# ============================================================


# ============================================================
# Linux runtime symbol export
# ============================================================
if(UNIX AND NOT APPLE)
  target_link_options(hgm PRIVATE -rdynamic)
endif()

# ============================================================
# Output naming
# ============================================================

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set_target_properties(hgm PROPERTIES
    OUTPUT_NAME "hwarangdo"
  )
elseif(HGM_BUILD_MODE STREQUAL "ASAN")
  set_target_properties(hgm PROPERTIES
    OUTPUT_NAME "hrd_asan"
  )
elseif(HGM_BUILD_MODE STREQUAL "IDE")
  set_target_properties(hgm PROPERTIES
    OUTPUT_NAME "hrd_ide"
  )
else()
  set_target_properties(hgm PROPERTIES
    OUTPUT_NAME "hrd_debug"
  )
endif()

# ============================================================
# LLVM
# ============================================================
find_package(LLVM REQUIRED CONFIG)

add_library(llvm_interface INTERFACE)
target_link_libraries(llvm_interface INTERFACE LLVM)
target_include_directories(llvm_interface INTERFACE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(llvm_interface INTERFACE ${LLVM_DEFINITIONS})

target_link_libraries(hgm PRIVATE llvm_interface)

