cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME BinaryClockV5)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    ${CMAKE_SOURCE_DIR}/Core/Src/shift595.c
    ${CMAKE_SOURCE_DIR}/Core/Src/clockface.c
    ${CMAKE_SOURCE_DIR}/Core/Src/clocktime.c
    ${CMAKE_SOURCE_DIR}/Core/Src/ds3231.c
    ${CMAKE_SOURCE_DIR}/Core/Src/buttons.c
    ${CMAKE_SOURCE_DIR}/Core/Src/clockapp.c
    ${CMAKE_SOURCE_DIR}/Core/Src/bh1750.c
    ${CMAKE_SOURCE_DIR}/Core/Src/brightness.c
    ${CMAKE_SOURCE_DIR}/Core/Src/bme280.c
    ${CMAKE_SOURCE_DIR}/Core/Src/buzzer.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)

# ---- Прошивка через openocd (WCH-Link у режимі CMSIS-DAP) ----
find_program(OPENOCD_EXECUTABLE openocd
    HINTS /opt/homebrew/bin /usr/local/bin
)

if(OPENOCD_EXECUTABLE)
    add_custom_target(flash
        COMMAND ${OPENOCD_EXECUTABLE}
                -f interface/cmsis-dap.cfg
                -f target/stm32l0.cfg
                -c "program $<TARGET_FILE:${CMAKE_PROJECT_NAME}> verify reset exit"
        DEPENDS ${CMAKE_PROJECT_NAME}
        USES_TERMINAL
        COMMENT "Прошивка ${CMAKE_PROJECT_NAME} через openocd"
    )
else()
    message(STATUS "openocd не знайдено — ціль 'flash' недоступна")
endif()
