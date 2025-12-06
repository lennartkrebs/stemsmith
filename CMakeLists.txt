cmake_minimum_required(VERSION 3.16)

project(stemsmith LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(ENABLE_OPENMP "Enable OpenMP for faster inference" ON)
option(STEMSMITH_BUILD_EXAMPLES "Build Stemsmith examples" ON)

set(STEMSMITH_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(STEMSMITH_INCLUDE_DIR ${STEMSMITH_ROOT}/include)
set(STEMSMITH_THIRDPARTY_DIR ${STEMSMITH_ROOT}/thirdparty)

find_package(PkgConfig QUIET)
find_package(CURL REQUIRED)

if (CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif ()

include(FetchContent)

# Third-party dependencies
FetchContent_Declare(asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-28-0
)
FetchContent_MakeAvailable(asio)

FetchContent_Declare(libsamplerate
    GIT_REPOSITORY https://github.com/libsndfile/libsamplerate.git
    GIT_TAG 0.2.2
)
FetchContent_MakeAvailable(libsamplerate)

add_subdirectory(external/demucs.cpp)

function(_stemsmith_silence target)
    if (TARGET ${target})
        target_compile_options(${target} PRIVATE ${ARGN})
    endif ()
endfunction()

_stemsmith_silence(demucs.cpp.lib -w)
_stemsmith_silence(Eigen3 -w)
_stemsmith_silence(libnyquist -w)

file(GLOB SOURCES "${STEMSMITH_ROOT}/src/*.cpp")
add_library(${PROJECT_NAME} ${SOURCES})
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

add_library(stemsmith_http
    src/http/server.cpp
)

add_library(miniz STATIC
    thirdparty/miniz/miniz.c
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${STEMSMITH_INCLUDE_DIR}
        ${STEMSMITH_THIRDPARTY_DIR}
        ${STEMSMITH_ROOT}/external/demucs.cpp/src
        ${STEMSMITH_ROOT}/external/demucs.cpp/vendor/libnyquist/include
    PRIVATE
        ${STEMSMITH_ROOT}/src
)

target_include_directories(stemsmith_http
    PUBLIC
        ${STEMSMITH_ROOT}/src
        ${STEMSMITH_THIRDPARTY_DIR}
        ${asio_SOURCE_DIR}/asio/include
        ${STEMSMITH_INCLUDE_DIR}
)

target_link_libraries(stemsmith_http PRIVATE miniz ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME}
    SYSTEM PUBLIC
        ${STEMSMITH_ROOT}/external/demucs.cpp/vendor/eigen
)

target_compile_definitions(${PROJECT_NAME}
    PUBLIC
        STEMSMITH_DATA_DIR="${STEMSMITH_ROOT}/data"
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        demucs.cpp.lib
        libnyquist
        samplerate
        CURL::libcurl
)

if (ENABLE_OPENMP)
    if (APPLE)
        set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp")
        set(OpenMP_CXX_LIB_NAMES "omp")
        set(OpenMP_omp_LIBRARY "/opt/homebrew/opt/libomp/lib/libomp.dylib")  # ARM Mac
        set(OpenMP_CXX_INCLUDE_DIR "/opt/homebrew/opt/libomp/include")
    endif ()

    find_package(OpenMP)
    if (OpenMP_CXX_FOUND)
        target_link_libraries(${PROJECT_NAME} PUBLIC OpenMP::OpenMP_CXX)
        if (OpenMP_CXX_INCLUDE_DIR)
            target_include_directories(${PROJECT_NAME} PRIVATE ${OpenMP_CXX_INCLUDE_DIR})
        endif ()
    endif ()
endif ()

enable_testing()

if (NOT TARGET GTest::gtest)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
    )
    FetchContent_MakeAvailable(googletest)
else ()
    message(STATUS "Using existing GoogleTest targets provided by a subproject/vendor")
endif ()

_stemsmith_silence(gtest -Wno-nan-infinity-disabled -Wno-deprecated)
_stemsmith_silence(gtest_main -Wno-nan-infinity-disabled -Wno-deprecated)

file(GLOB_RECURSE TEST_SOURCES "${STEMSMITH_ROOT}/tests/*.cpp")
add_executable(${PROJECT_NAME}_test ${TEST_SOURCES})

target_link_libraries(${PROJECT_NAME}_test
    PRIVATE
        ${PROJECT_NAME}
        stemsmith_http
        GTest::gtest
)

target_include_directories(${PROJECT_NAME}_test
    PRIVATE
        ${STEMSMITH_INCLUDE_DIR}
        ${STEMSMITH_ROOT}/src
        ${STEMSMITH_ROOT}/tests/support
)

target_compile_definitions(${PROJECT_NAME}_test
    PRIVATE
        STEMSMITH_TEST_DATA_DIR="${STEMSMITH_ROOT}/tests/fixtures"
)

add_test(NAME stemsmith_test COMMAND ${PROJECT_NAME}_test)

add_executable(stemsmithd src/http/stemsmithd.cpp)
target_link_libraries(stemsmithd PRIVATE stemsmith_http)
target_include_directories(stemsmithd PRIVATE
    ${STEMSMITH_INCLUDE_DIR}
    ${STEMSMITH_ROOT}/src
)

if (STEMSMITH_BUILD_EXAMPLES)
    add_executable(simple_separation_example examples/simple_separation.cpp)
    target_link_libraries(simple_separation_example PRIVATE ${PROJECT_NAME})
    target_include_directories(simple_separation_example PRIVATE
        ${STEMSMITH_INCLUDE_DIR}
        ${STEMSMITH_ROOT}/examples
    )
    target_compile_definitions(simple_separation_example PRIVATE STEMSMITH_SOURCE_DIR="${STEMSMITH_ROOT}")

    add_executable(observer_separation_example examples/observer_separation.cpp)
    target_link_libraries(observer_separation_example PRIVATE ${PROJECT_NAME})
    target_include_directories(observer_separation_example PRIVATE
        ${STEMSMITH_INCLUDE_DIR}
        ${STEMSMITH_ROOT}/examples
    )
    target_compile_definitions(observer_separation_example PRIVATE STEMSMITH_SOURCE_DIR="${STEMSMITH_ROOT}")

    add_executable(http_server_example examples/http_server_example.cpp)
    target_link_libraries(http_server_example PRIVATE ${PROJECT_NAME} stemsmith_http CURL::libcurl)
    target_compile_definitions(http_server_example PRIVATE STEMSMITH_SOURCE_DIR="${STEMSMITH_ROOT}")
endif ()
