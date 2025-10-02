cmake_minimum_required(VERSION 3.16)
set(PROJECT_NAME stemsmith)
project(stemsmith LANGUAGES CXX)

# Find ASIO (header-only, so you can use FetchContent)
include(FetchContent)
FetchContent_Declare(
        asio
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
        GIT_TAG asio-1-28-0
)
FetchContent_MakeAvailable(asio)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(external/demucs.cpp)

add_library(${PROJECT_NAME}
        src/api_server.cpp
        src/job_queue.cpp
        src/job_manager.cpp
)

target_include_directories(${PROJECT_NAME}
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty
        ${asio_SOURCE_DIR}/asio/include
)

target_link_libraries(${PROJECT_NAME}
        PUBLIC
        demucs.cpp.lib)

option(ENABLE_OPENMP "Enable OpenMP for faster inference" ON)
if (ENABLE_OPENMP)
    if(APPLE)
        set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp")
        set(OpenMP_CXX_LIB_NAMES "omp")
        set(OpenMP_omp_LIBRARY "/opt/homebrew/opt/libomp/lib/libomp.dylib")  # ARM Mac
    endif()

    # OpenMP (demucs.cpp uses Eigen+OpenMP for speed)
    find_package(OpenMP)
    if (OpenMP_CXX_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE OpenMP::OpenMP_CXX)
    endif()
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