cmake_minimum_required(VERSION 3.10)
project(SBMPI LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(MPI REQUIRED)
find_package(OpenSSL REQUIRED)

# -------------------------------
# Gather all source files EXCEPT main.cpp
# -------------------------------
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "src/consensus/*.cpp"
    "src/core/*.cpp"
    "src/core/blocks/*.cpp"
    "src/core/mempool/*.cpp"
    "src/core/state/*.cpp"
    "src/network/*.cpp"
    "src/network/committee/*.cpp"
    "src/util/*.cpp"
)

# -------------------------------
# Main executable
# -------------------------------
add_executable(sbmpi
    src/main.cpp
    ${SOURCES}
)

# Correct include directory
target_include_directories(sbmpi PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Link MPI + OpenSSL
target_link_libraries(sbmpi PRIVATE
    MPI::MPI_CXX
    OpenSSL::Crypto
)

# -------------------------------
# Optional: Tests
# -------------------------------
option(BUILD_TESTS "Build the tests" OFF)

if(BUILD_TESTS)
    enable_testing()

    add_executable(run_tests
        tests/test_pbft.cpp
        tests/test_sharding.cpp
        ${SOURCES}
    )

    target_include_directories(run_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )

    target_link_libraries(run_tests PRIVATE
        MPI::MPI_CXX
        OpenSSL::Crypto
    )

    add_test(NAME RunSBMPI_Tests
             COMMAND mpiexec -n 4 $<TARGET_FILE:run_tests>)
endif()

# -------------------------------
# Install binary
# -------------------------------
install(TARGETS sbmpi DESTINATION bin)
