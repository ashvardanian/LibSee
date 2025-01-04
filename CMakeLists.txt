cmake_minimum_required(VERSION 3.14)
project(libsee
    VERSION 1.1.0
    DESCRIPTION "See where you use LibC the most. Trace calls failing tests. Then - roast!")

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

# Define library source file
set(SOURCE_FILES libsee.c)
set(OUTPUT_LIB_NAME "libsee")

# Detect OS and set library extension and compilation options accordingly
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_compile_options(-fPIC -nostdlib -nostartfiles)
    set(LINK_OPTIONS "-shared")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    add_compile_options(-fPIC -nostdlib -nostartfiles)
    set(LINK_OPTIONS "-dynamiclib")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_compile_options(-nostdlib -nostartfiles)
    set(LINK_OPTIONS "-shared")
endif()

# Add library
add_library(${OUTPUT_LIB_NAME} SHARED ${SOURCE_FILES})

# Set properties for different build types
set_target_properties(${OUTPUT_LIB_NAME} PROPERTIES PREFIX "")

# Disable using the built-in functions
add_compile_options(-fno-builtin)

# Debug or Release flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(DEBUG=1)
    add_compile_options(-g)
else()
    add_compile_options(-O2)
endif()

# Link options
target_link_options(${OUTPUT_LIB_NAME} PRIVATE ${LINK_OPTIONS})

# Add clean target for generated library
set_property(DIRECTORY PROPERTY ADDITIONAL_MAKE_CLEAN_FILES ${OUTPUT_LIB_NAME})

# Provide option for building in debug mode (can be passed via `-DCMAKE_BUILD_TYPE=Debug` during configuration)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Building in debug mode")
else()
    message(STATUS "Building in release mode")
endif()
