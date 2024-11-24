cmake_minimum_required(VERSION 3.10)

# Project setup
project(Toy LANGUAGES CXX)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

# Find LLVM (assumes llvm-config is in PATH)
find_program(LLVM_CONFIG llvm-config REQUIRED)

# Get LLVM flags
execute_process(COMMAND ${LLVM_CONFIG} --cxxflags OUTPUT_VARIABLE LLVM_CXXFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${LLVM_CONFIG} --ldflags OUTPUT_VARIABLE LLVM_LDFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${LLVM_CONFIG} --system-libs --libs core OUTPUT_VARIABLE LLVM_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${LLVM_CONFIG} --includedir OUTPUT_VARIABLE LLVM_INCLUDE_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)

# Split space-separated flags
string(REPLACE " " ";" LLVM_CXXFLAGS_LIST ${LLVM_CXXFLAGS})
string(REPLACE " " ";" LLVM_LDFLAGS_LIST ${LLVM_LDFLAGS})
string(REPLACE " " ";" LLVM_LIBS_LIST ${LLVM_LIBS})

# Add the executable
add_executable(toy Parser.cpp AST.cpp ErrorHandler.cpp Lexer.cpp)

# Include LLVM directories and libraries
include_directories(${LLVM_INCLUDE_DIR})

# Apply LLVM flags
target_compile_options(toy PRIVATE ${LLVM_CXXFLAGS_LIST} -g -O3)
target_link_options(toy PRIVATE ${LLVM_LDFLAGS_LIST})
target_link_libraries(toy PRIVATE ${LLVM_LIBS_LIST})
