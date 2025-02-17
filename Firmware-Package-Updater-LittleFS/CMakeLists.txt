cmake_minimum_required(VERSION 3.5)

############################################################
# The following version captures are not required, but useful for debugging.

# Capture Build Timestamp
execute_process(
    COMMAND date "+%Y-%m-%d %H:%M:%S"
    OUTPUT_VARIABLE BUILD_TIMESTAMP
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Capture Git version
execute_process(
    COMMAND git describe --always --dirty
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
add_definitions(-DGIT_VERSION="${GIT_VERSION}")

# Capture ESP-IDF version
execute_process(
    COMMAND idf.py --version
    OUTPUT_VARIABLE ESP_IDF_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Capture the first line of the Compiler version
execute_process(
    COMMAND xtensa-esp32-elf-gcc --version
    OUTPUT_VARIABLE COMPILER_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(REGEX REPLACE "\n.*" "" COMPILER_VERSION "${COMPILER_VERSION}") # Keep only the first line

# Capture OS information (only PRETTY_NAME)
execute_process(
    COMMAND sh -c "grep PRETTY_NAME /etc/os-release | cut -d= -f2 | tr -d '\"'"
    OUTPUT_VARIABLE OS_INFO
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Write the build info to a version-controlled file
file(WRITE ${CMAKE_SOURCE_DIR}/version_info/build_info.txt
    "Build Info:\n"
    "Build Timestamp: ${BUILD_TIMESTAMP}\n"
    "Git Version: ${GIT_VERSION}\n"
    "ESP-IDF Version: ${ESP_IDF_VERSION}\n"
    "Compiler Version: ${COMPILER_VERSION}\n"
    "OS Information: ${OS_INFO}\n"
)
############################################################

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(Firmware-Package-Updater-LittleFS)

# Copies files from the html folder to the LittleFS partition during the build process
littlefs_create_partition_image(web html FLASH_IN_PROJECT)
