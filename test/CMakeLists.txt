# Enable testing
enable_testing()

# Include FetchContent for downloading Catch2
include(FetchContent)

# Fetch Catch2
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.3
)
FetchContent_MakeAvailable(Catch2)

# Include Catch2's CMake scripts
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

# Create a single PCH library that will be used by all tests
add_library(test_pch INTERFACE)
target_precompile_headers(test_pch
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/test_pch.hpp
)
target_include_directories(test_pch
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Create a common interface library for test dependencies
add_library(test_common INTERFACE)
target_include_directories(test_common
    INTERFACE 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        /usr/include
)
target_link_libraries(test_common
    INTERFACE
        feather
        Catch2::Catch2WithMain
        test_pch  # Link the PCH library
)

# Create test executables for each test file
set(TEST_TARGETS
    core_test
    router_test
    controller_test
    server_test
)

# Create all test executables
foreach(test_target ${TEST_TARGETS})
    add_executable(${test_target} ${test_target}.cpp)
    
    # Link against the common interface library
    target_link_libraries(${test_target}
        PRIVATE
            test_common
    )
    
    # Register test with CTest
    catch_discover_tests(${test_target})
endforeach() 