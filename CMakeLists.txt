# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(APP_FIRMWARE_HPP
    "${CMAKE_SOURCE_DIR}/components/app_cmn/include/app/cfg/firmware.hpp"
)

# If firmware.hpp is modified, the build system will re-run CMake to
# re-configure the file and generate the build system again.
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${APP_FIRMWARE_HPP}"
)

file(STRINGS
    "${APP_FIRMWARE_HPP}"
    APP_FIRMWARE_VERSION_LINE
    REGEX "FIRMWARE_VERSION[ \t]*=[ \t]*\"[^\"]+\""
)

string(REGEX REPLACE
    ".*FIRMWARE_VERSION[ \t]*=[ \t]*\"([^\"]+)\".*"
    "\\1"
    APP_FIRMWARE_VERSION
    "${APP_FIRMWARE_VERSION_LINE}"
)

if(NOT APP_FIRMWARE_VERSION)
    message(FATAL_ERROR "Failed to parse FIRMWARE_VERSION from ${APP_FIRMWARE_HPP}")
endif()

message(STATUS "App firmware version: ${APP_FIRMWARE_VERSION}")

set(APP_FIRMWARE_BIN "firmware-v${APP_FIRMWARE_VERSION}.bin")
set(PROJECT_VER "${APP_FIRMWARE_VERSION}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32-aws-iot-capstone)

set(POST_BUILD_COMMANDS
    COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin"
    "${CMAKE_BINARY_DIR}/${APP_FIRMWARE_BIN}"
    COMMAND ${CMAKE_COMMAND} -E echo 
    "Copied build binary to ${APP_FIRMWARE_BIN}"
)

set(OTA_SERVER_BIN_DIR "${CMAKE_SOURCE_DIR}/ota-server/bin")

if(EXISTS "${OTA_SERVER_BIN_DIR}")
    list(APPEND POST_BUILD_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin"
        "${OTA_SERVER_BIN_DIR}/${APP_FIRMWARE_BIN}"
        COMMAND ${CMAKE_COMMAND} -E echo
        "Copied OTA binary to ${OTA_SERVER_BIN_DIR}"
    )
endif()

if(CONFIG_SECURE_FLASH_ENC_ENABLED)
    set(APP_FLASH_ENCRYPTION_KEY
        "${CMAKE_SOURCE_DIR}/security/flash_encryption/flash_encryption_key.bin"
    )
    set(APP_FACTORY_ENCRYPTED_BIN
        "${CMAKE_BINARY_DIR}/firmware-v${APP_FIRMWARE_VERSION}-factory-encrypted.bin"
    )

    partition_table_get_partition_info(APP_FACTORY_OFFSET "--partition-name factory" "offset")
    if(NOT APP_FACTORY_OFFSET)
        message(FATAL_ERROR "Factory partition not found for host-side firmware encryption")
    endif()

    idf_component_get_property(espsecure_py_cmd esptool_py ESPSECUREPY_CMD)
    list(APPEND POST_BUILD_COMMANDS
        COMMAND ${espsecure_py_cmd} encrypt-flash-data
            --aes-xts
            --keyfile "${APP_FLASH_ENCRYPTION_KEY}"
            --address "${APP_FACTORY_OFFSET}"
            --output "${APP_FACTORY_ENCRYPTED_BIN}"
            "${CMAKE_BINARY_DIR}/${APP_FIRMWARE_BIN}"
        COMMAND ${CMAKE_COMMAND} -E echo
            "Generated encrypted factory firmware at offset ${APP_FACTORY_OFFSET}: ${APP_FACTORY_ENCRYPTED_BIN}"
    )
endif()

add_custom_command(TARGET app POST_BUILD ${POST_BUILD_COMMANDS} VERBATIM)
