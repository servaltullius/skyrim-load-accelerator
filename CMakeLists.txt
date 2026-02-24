cmake_minimum_required(VERSION 3.25)

project(
    SkyrimLoadAccelerator
    VERSION 1.0.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF)

# CommonLibSSE-NG
find_package(CommonLibSSE CONFIG REQUIRED)

# Dependencies
find_package(libdeflate CONFIG REQUIRED)
find_package(lz4 CONFIG REQUIRED)
find_package(mimalloc CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_path(SIMPLEINI_INCLUDE_DIRS "SimpleIni.h")
find_package(TBB CONFIG REQUIRED)

# Source files
file(GLOB_RECURSE SOURCES
    src/*.cpp
    src/*.h
)

# PCH
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/PCH.h")

source_group(
    TREE ${CMAKE_CURRENT_SOURCE_DIR}/src
    PREFIX "Source Files"
    FILES ${SOURCES}
)

add_commonlibsse_plugin(
    ${PROJECT_NAME}
    AUTHOR "SkyrimLoadAccelerator Team"
    SOURCES ${SOURCES}
)

target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${SIMPLEINI_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    $<IF:$<TARGET_EXISTS:libdeflate::libdeflate_static>,libdeflate::libdeflate_static,libdeflate::libdeflate_shared>
    lz4::lz4
    $<IF:$<TARGET_EXISTS:mimalloc-static>,mimalloc-static,mimalloc>
    spdlog::spdlog
    TBB::tbb
)

# Copy output DLL and INI
if(DEFINED ENV{SKYRIM_MODS_PATH})
    set(MOD_OUTPUT_DIR "$ENV{SKYRIM_MODS_PATH}/SkyrimLoadAccelerator/SKSE/Plugins")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MOD_OUTPUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${PROJECT_NAME}>"
            "${MOD_OUTPUT_DIR}/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/dist/Data/SKSE/Plugins/SkyrimLoadAccelerator.ini"
            "${MOD_OUTPUT_DIR}/"
    )
endif()

include(cmake/pluginpackage.cmake)
