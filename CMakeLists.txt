cmake_minimum_required(VERSION 3.16)
set(PROJECT_NAME stemsmith)
project(stemsmith LANGUAGES CXX)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(external/demucs.cpp)

add_library(${PROJECT_NAME}
        src/separation_engine.cpp
        main.cpp
)

target_include_directories(${PROJECT_NAME}
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty
)

target_link_libraries(${PROJECT_NAME} PUBLIC demucs.cpp.lib)

# OpenMP (demucs.cpp uses Eigen+OpenMP for speed)
find_package(OpenMP)
if (OpenMP_CXX_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE OpenMP::OpenMP_CXX)
endif()

# Testing
add_executable(${PROJECT_NAME}_test main.cpp)

target_link_libraries(${PROJECT_NAME}_test
        PRIVATE
        ${PROJECT_NAME}
)

target_include_directories(${PROJECT_NAME}_test
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)