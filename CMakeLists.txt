cmake_minimum_required(VERSION 3.16)
project(ExpandedPresetsPlugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(EXP_PRESETS_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

if(MSVC)
    add_compile_options(/permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(EXP_PRESETS_WARNINGS_AS_ERRORS)
        add_compile_options(-Werror)
    endif()
endif()

set(BM_SDK_DIR "${CMAKE_SOURCE_DIR}/bakkesmod_sdk" CACHE PATH "Path to the BakkesMod SDK checkout")
if(NOT EXISTS "${BM_SDK_DIR}/include/bakkesmod/plugin/bakkesmodplugin.h")
    message(FATAL_ERROR "Could not find BakkesMod SDK headers. Set BM_SDK_DIR to a valid SDK checkout containing the include directory.")
endif()

file(GLOB_RECURSE EXP_PRESETS_SOURCES CONFIGURE_DEPENDS
    "src/*.cpp"
    "src/*.cxx"
    "src/*.cc"
)

add_library(${PROJECT_NAME} SHARED ${EXP_PRESETS_SOURCES})

if(MSVC)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    ${BM_SDK_DIR}/include
)

# BakkesMod ships prebuilt binaries for 32-bit and 64-bit, expose a cache entry so
# the developer can point to the appropriate library directory.
set(BM_SDK_LIB_DIR "${BM_SDK_DIR}/lib" CACHE PATH "Path to the directory containing bakkesmod.lib")
if(WIN32)
    find_library(BM_LIBRARY NAMES bakkesmod PATHS ${BM_SDK_LIB_DIR} REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${BM_LIBRARY})
endif()

# Place the compiled binary in the BakkesMod plugins folder when using Visual Studio's
# default behavior of running from the build tree.
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "ExpandedPresets"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)

