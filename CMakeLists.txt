cmake_minimum_required(VERSION 3.16)
project(separation_engine LANGUAGES CXX)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(external/demucs.cpp)

add_library(separation_engine
        src/SeparationEngine.cpp
)

target_include_directories(separation_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(separation_engine PRIVATE demucs.cpp.lib)

# OpenMP (demucs.cpp uses Eigen+OpenMP for speed)
find_package(OpenMP)
if (OpenMP_CXX_FOUND)
    target_link_libraries(separation_engine PRIVATE OpenMP::OpenMP_CXX)
endif()
