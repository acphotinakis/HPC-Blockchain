cmake_minimum_required(VERSION 3.16)
project(SBMPI LANGUAGES CXX)

# -------------------------------------------------
# Global settings
# -------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -------------------------------------------------
# Dependencies
# -------------------------------------------------
find_package(MPI REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(OpenMP)

# -------------------------------------------------
# Core library (everything except main)
# -------------------------------------------------
file(GLOB_RECURSE SBM_MPI_SOURCES CONFIGURE_DEPENDS
    src/consensus/*.cpp
    src/core/*.cpp
    src/network/*.cpp
    src/util/*.cpp
)

add_library(sbmpi_core STATIC ${SBM_MPI_SOURCES})

target_include_directories(sbmpi_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/json/single_include
    ${CMAKE_SOURCE_DIR}/secp256k1/include
)

target_link_libraries(sbmpi_core PUBLIC
    MPI::MPI_CXX
    OpenSSL::Crypto
)

if(OpenMP_CXX_FOUND)
    target_link_libraries(sbmpi_core PUBLIC OpenMP::OpenMP_CXX)
endif()

# secp256k1 static library
target_link_libraries(sbmpi_core PUBLIC
    ${CMAKE_SOURCE_DIR}/secp256k1/local/lib/libsecp256k1.a
)

# -------------------------------------------------
# Main executable
# -------------------------------------------------
add_executable(sbmpi src/main.cpp)
target_link_libraries(sbmpi PRIVATE sbmpi_core)

# -------------------------------------------------
# Tests
# -------------------------------------------------
option(BUILD_TESTS "Build the tests" OFF)

if(BUILD_TESTS)
    enable_testing()

    add_executable(run_tests
        tests/test_pbft.cpp
        tests/test_sharding.cpp
    )

    target_link_libraries(run_tests PRIVATE sbmpi_core)

    add_test(
        NAME SBM_MPI_Tests
        COMMAND mpiexec -n 4 $<TARGET_FILE:run_tests>
    )
endif()

# -------------------------------------------------
# Install
# -------------------------------------------------
install(TARGETS sbmpi DESTINATION bin)
