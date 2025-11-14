cmake_minimum_required(VERSION 3.16)
set(PROJECT_NAME stemsmith)
project(stemsmith LANGUAGES CXX)

if (CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif ()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)
FetchContent_Declare(
        asio
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
        GIT_TAG asio-1-28-0
)
FetchContent_MakeAvailable(asio)

FetchContent_Declare(
        libsamplerate
        GIT_REPOSITORY https://github.com/libsndfile/libsamplerate.git
        GIT_TAG 0.2.2
)
FetchContent_MakeAvailable(libsamplerate)

add_subdirectory(external/demucs.cpp)

file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_library(${PROJECT_NAME} ${SOURCES})
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME}
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty
        ${asio_SOURCE_DIR}/asio/include
        ${CMAKE_CURRENT_SOURCE_DIR}/external/demucs.cpp/src
        ${CMAKE_CURRENT_SOURCE_DIR}/external/demucs.cpp/vendor/libnyquist/include
)

target_compile_definitions(${PROJECT_NAME}
        PUBLIC
        STEMSMITH_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)

target_link_libraries(${PROJECT_NAME}
        PUBLIC
        demucs.cpp.lib
        libnyquist
        samplerate
)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wno-deprecated-declarations)
endif()

option(ENABLE_OPENMP "Enable OpenMP for faster inference" ON)
if (ENABLE_OPENMP)
    if (APPLE)
        set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp")
        set(OpenMP_CXX_LIB_NAMES "omp")
        set(OpenMP_omp_LIBRARY "/opt/homebrew/opt/libomp/lib/libomp.dylib")  # ARM Mac
        set(OpenMP_CXX_INCLUDE_DIR "/opt/homebrew/opt/libomp/include")
    endif ()

    # OpenMP (demucs.cpp uses Eigen+OpenMP for speed)
    find_package(OpenMP)
    if (OpenMP_CXX_FOUND)
        target_link_libraries(${PROJECT_NAME} PUBLIC OpenMP::OpenMP_CXX)
        if (OpenMP_CXX_INCLUDE_DIR)
            target_include_directories(${PROJECT_NAME} PRIVATE ${OpenMP_CXX_INCLUDE_DIR})
        endif ()
    endif ()
endif ()

enable_testing()

# Only fetch googletest if no gtest targets already exist (some subprojects may provide googletest)
if (NOT TARGET GTest::gtest)
    FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.17.0
    )

    FetchContent_MakeAvailable(googletest)
else()
    message(STATUS "Using existing GoogleTest targets provided by a subproject/vendor")
endif()

file(GLOB_RECURSE TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
add_executable(${PROJECT_NAME}_test ${TEST_SOURCES})

target_link_libraries(${PROJECT_NAME}_test
        PRIVATE
        ${PROJECT_NAME}
        GTest::gtest
)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${PROJECT_NAME}_test PRIVATE -Wno-deprecated-declarations)
endif()

target_include_directories(${PROJECT_NAME}_test
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/support
)

target_compile_definitions(${PROJECT_NAME}_test
        PRIVATE
        STEMSMITH_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
)

add_test(NAME stemsmith-test COMMAND ${PROJECT_NAME}_test)
