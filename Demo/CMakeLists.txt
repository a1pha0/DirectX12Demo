cmake_minimum_required(VERSION 3.27)

set(TARGET_NAME Demo)

set(SOURCE_FILES
    src/Main.cpp
)

add_executable(${TARGET_NAME} WIN32
    ${SOURCE_FILES}
    ${SHADER_FILES}
)

target_link_libraries(${TARGET_NAME}
    PRIVATE DX12Lib
)

target_compile_definitions(${TARGET_NAME} PRIVATE UNICODE _UNICODE)

set_target_properties(${TARGET_NAME}
    PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
