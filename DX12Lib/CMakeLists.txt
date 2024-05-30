cmake_minimum_required(VERSION 3.27)

set(TARGET_NAME DX12Lib)

file(GLOB_RECURSE HEADER_FILES "./include/DX12Lib/*.h")
file(GLOB_RECURSE SOURCE_FILES "./src/*.cpp")

source_group("Header Files" FILES ${HEADER_FILES})
source_group("Source Files" FILES ${SOURCE_FILES})

add_library(${TARGET_NAME} STATIC
    ${HEADER_FILES}
    ${SOURCE_FILES}
)

# Enable C++17 compiler features.
target_compile_features(${TARGET_NAME} PUBLIC cxx_std_17)

target_compile_options(${TARGET_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
    # $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
    # $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)

target_compile_definitions(${TARGET_NAME} PUBLIC UNICODE _UNICODE)

target_include_directories(${TARGET_NAME}
    PUBLIC include
    PRIVATE include/DX12Lib
)

target_link_libraries(${TARGET_NAME}
    PUBLIC d3d12.lib
    PUBLIC dxgi.lib
    PUBLIC dxguid.lib
    PUBLIC d3dcompiler.lib
)
