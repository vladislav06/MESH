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
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif ()

# Set the project name
set(CMAKE_PROJECT_NAME MESH-code)

# Include toolchain file
include("cmake/gcc-arm-none-eabi.cmake")

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Enable CMake support for ASM and C languages
enable_language(C ASM)

#add_link_options( -u _printf_float)
# use tinyprintf
add_link_options(-Wl,--wrap,printf -Wl,--wrap,fprintf -Wl,--wrap,sprintf -Wl,--wrap,asprintf
        -Wl,--wrap,vprintf -Wl,--wrap,vfprintf -Wl,--wrap,vsprintf -Wl,--wrap,vasprintf
        -Wl,--wrap,puts -Wl,--wrap,putchar -Wl,--wrap,snprintf -Wl,--wrap,fwrite
        -Wl,--wrap,fputs -Wl,--wrap,fputc -Wl,--wrap,vsnprintf -Wl,--wrap,fflush
        -Wl,--wrap,fiprintf)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

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
        src/appMain.c
        src/wirelessComms.c
        src/actuator.c
        src/cc1101/cc1101.c
        src/ld2410b/ld2410b.c
        src/utils/utils.c
        src/hw/hw.c
        src/tinyprintf/printf-c.cc
        src/expr/expr.c
        src/eeprom/eeprom.c
        src/protocol/routing.c
        src/protocol/protocol.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
        # Add user defined include paths
        src/cc1101
        src/ld2410b
        src
        src/utils
        src/hw
        src/expr
        src/eeprom
        src/protocol
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        # Add user defined symbols
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
        stm32cubemx

        # Add user defined libraries
)
