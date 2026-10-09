cmake_minimum_required(VERSION 3.24)

project(AsterEngine VERSION 0.1.0 LANGUAGES CXX)

option(ASTER_BUILD_TESTS "Build Aster Engine tests" ON)
option(ASTER_BUILD_EXAMPLES "Build Aster Engine examples" ON)

add_library(AsterEngine STATIC
    src/Aster/Scene/Scene.cpp
)
add_library(Aster::Engine ALIAS AsterEngine)

target_compile_features(AsterEngine PUBLIC cxx_std_20)
target_include_directories(AsterEngine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

if(MSVC)
    target_compile_options(AsterEngine PRIVATE /W4 /permissive-)
else()
    target_compile_options(AsterEngine PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

if(ASTER_BUILD_EXAMPLES)
    add_executable(AsterSandbox examples/Sandbox.cpp)
    target_link_libraries(AsterSandbox PRIVATE Aster::Engine)
endif()

if(ASTER_BUILD_TESTS)
    enable_testing()
    add_executable(AsterEngineTests tests/EngineTests.cpp)
    target_link_libraries(AsterEngineTests PRIVATE Aster::Engine)
    add_test(NAME AsterEngineTests COMMAND AsterEngineTests)
endif()