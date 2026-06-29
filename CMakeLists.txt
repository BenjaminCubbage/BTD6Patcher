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

# Holy backslash batman
string(ASCII 27 Esc)
message("Using game directory: ${Esc}[44m${BLOONSTD6_ROOT}${Esc}[0m.")
message("This can be changed in CMakePresets.json")

add_library(payload SHARED src/payload.cpp)
target_link_libraries(payload PUBLIC kernel32)
target_precompile_headers(payload PUBLIC <windows.h>)

add_executable(bootstrapper ./src/bootstrapper.cpp)
target_link_libraries(bootstrapper PRIVATE payload kernel32)
target_precompile_headers(bootstrapper PRIVATE <windows.h>)
target_compile_definitions(bootstrapper PRIVATE
    "BLOONSTD6_ROOT=R\"(${BLOONSTD6_ROOT})\""
    "PAYLOAD_DLL=R\"($<TARGET_FILE:payload>)\""
    "KERNEL32_DLL=R\"($ENV{SystemRoot}\\System32\\kernel32.dll)\"")