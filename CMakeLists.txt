cmake_minimum_required(VERSION 3.16)

set(ESP_MATTER_PATH $ENV{ESP_MATTER_PATH})
set(MATTER_SDK_PATH ${ESP_MATTER_PATH}/connectedhomeip/connectedhomeip)

set(EXTRA_COMPONENT_DIRS
    "${ESP_MATTER_PATH}/components"
    "${ESP_MATTER_PATH}/examples/common"
    "${MATTER_SDK_PATH}/config/esp32/components"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(garage_relay)
