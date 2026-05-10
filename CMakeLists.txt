cmake_minimum_required(VERSION 3.24)

project(ThePriceOfLoyalty VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CommonLibSSE CONFIG REQUIRED)
find_package(directxtk CONFIG REQUIRED)
find_package(DirectXMath CONFIG REQUIRED)

# Use standard library creation instead of macro to avoid generated file issues
add_library(${PROJECT_NAME} SHARED
    src/main.cpp
    src/InteractionHandler.cpp
    src/TraitManager.cpp
    src/BehaviorManager.cpp
    src/EffectManager.cpp
    src/SerializationManager.cpp
    src/SurrenderHandler.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE include)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    CommonLibSSE::CommonLibSSE 
    Microsoft::DirectXTK 
    Microsoft::DirectXMath
)

target_compile_options(${PROJECT_NAME} PRIVATE /FI"FixLibrary.hpp")

set_target_properties(${PROJECT_NAME} PROPERTIES 
    INTERPROCEDURAL_OPTIMIZATION ON
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    PREFIX ""
    SUFFIX ".dll"
)
