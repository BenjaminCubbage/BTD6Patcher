cmake_minimum_required(VERSION 3.23)

project(BTD6Patcher LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT WIN32)
    message(FATAL_ERROR "This is a Windows application. System name: \"${CMAKE_SYSTEM_NAME}\"")
endif()

if(NOT BLOONSTD6_ROOT)
    message(FATAL_ERROR "Game root was not specified, did you forget --preset default?")
endif()

# Normalize path
string(REGEX REPLACE "\\\\" "/" BLOONSTD6_ROOT "${BLOONSTD6_ROOT}")

string(ASCII 27 Esc)
message("Using game directory: ${Esc}[44m${BLOONSTD6_ROOT}${Esc}[0m.")
message("This can be changed in CMakePresets.json")

add_executable(main ./src/main.cpp)
target_link_libraries(main PUBLIC kernel32)
target_precompile_headers(main PRIVATE <windows.h>)
target_compile_definitions(main PRIVATE BLOONSTD6_ROOT="${BLOONSTD6_ROOT}")