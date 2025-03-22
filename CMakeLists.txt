cmake_minimum_required(VERSION 3.20)  # Using modern CMake features

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

# Force CMake to use g++-14 before project() command
set(CMAKE_CXX_COMPILER "/usr/bin/g++-14" CACHE STRING "C++ compiler" FORCE)

# Project name and language
project(feather 
    VERSION 0.1.0
    DESCRIPTION "A modern C++ web framework"
    LANGUAGES CXX
)

# Enable CTest
include(CTest)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED OFF)  # Allow falling back to older standards
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_UNITY_BUILD ON)

# Set compiler flags but without -Werror to allow mixed standard code
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
endif()

# Add subdirectories
add_subdirectory(test)

# Create header-only library target
add_library(feather INTERFACE)

# Set include directories
target_include_directories(feather 
    INTERFACE 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        /usr/include  # For system-installed header-only libraries
)

# Find required packages
find_package(Boost REQUIRED)

# Find cpp-httplib
find_package(httplib QUIET)
if(NOT httplib_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        cpp-httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG v0.14.1
    )
    FetchContent_MakeAvailable(cpp-httplib)
endif()

# Find websocketpp and set it up manually since it doesn't provide proper targets
find_package(websocketpp QUIET)
if(NOT websocketpp_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        websocketpp
        GIT_REPOSITORY https://github.com/zaphoyd/websocketpp.git
        GIT_TAG 0.8.2
    )
    FetchContent_GetProperties(websocketpp)
    if(NOT websocketpp_POPULATED)
        FetchContent_Populate(websocketpp)
    endif()
    
    # Create interface library for websocketpp
    add_library(websocketpp INTERFACE)
    add_library(websocketpp::websocketpp ALIAS websocketpp)
    
    target_include_directories(websocketpp INTERFACE 
        $<BUILD_INTERFACE:${websocketpp_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
    )

    # Install websocketpp headers
    install(DIRECTORY ${websocketpp_SOURCE_DIR}/websocketpp
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.hpp"
    )

    # Export websocketpp target
    install(TARGETS websocketpp
        EXPORT feather-deps-targets
    )
endif()

# Link dependencies to the interface library
target_link_libraries(feather 
    INTERFACE 
        Boost::boost
)

# Only link httplib if found or fetched successfully
if(TARGET httplib::httplib)
    target_link_libraries(feather INTERFACE httplib::httplib)
endif()

# Link websocketpp
if(TARGET websocketpp::websocketpp)
    target_link_libraries(feather INTERFACE websocketpp::websocketpp)
endif()

# Installation rules
include(GNUInstallDirs)

# Install and export dependencies first
install(EXPORT feather-deps-targets
    FILE feather-deps-targets.cmake
    NAMESPACE feather::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/feather
)

# Install main library
install(TARGETS feather
    EXPORT feather-targets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp"
)

# Export main library targets
install(EXPORT feather-targets
    FILE feather-targets.cmake
    NAMESPACE feather::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/feather
)

# Create and install the config file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/feather-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/feather-config.cmake
    @ONLY
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/feather-config.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/feather
) 