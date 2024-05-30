cmake_minimum_required(VERSION 3.27)

project(DevLibs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_DEMO "Build demo" ON)

# Enable to build shared libraries.
option(BUILD_SHARED_LIBS "Create shared libraries." OFF)

# Use solution folders to organize projects
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Make sure DLL and EXE targets go to the same directory.
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib) # Output directory for static lib (.LIB)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin) # Output directory for shared lib (.DLL)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin) # Output directory for executables (.EXE)

# Enable multithreaded builds
if (MSVC)
    add_compile_options(/MP)
endif()

add_subdirectory(DX12Lib)

if (BUILD_DEMO)
    add_subdirectory(Demo)
    # Set the startup project.
    set_directory_properties(PROPERTIES VS_STARTUP_PROJECT Demo)
endif()
