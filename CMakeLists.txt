cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(test_project C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(dev_hid_composite)

# Add your source files
target_sources(dev_hid_composite PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/main.c
        ${CMAKE_CURRENT_LIST_DIR}/usb_descriptors.c
        ${CMAKE_CURRENT_LIST_DIR}/totp.c
        ${CMAKE_CURRENT_LIST_DIR}/base32.c
        ${CMAKE_CURRENT_LIST_DIR}/sha1.c
        )

# Make sure TinyUSB can find tusb_config.h and enable CDC class
target_include_directories(dev_hid_composite PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
        ${PICO_SDK_PATH}/lib/tinyusb/src
)

target_compile_definitions(dev_hid_composite PUBLIC)

target_link_libraries(dev_hid_composite PUBLIC pico_stdlib pico_unique_id tinyusb_device tinyusb_board)

pico_enable_stdio_usb(dev_hid_composite 1)
pico_enable_stdio_uart(dev_hid_composite 1)
pico_add_extra_outputs(dev_hid_composite)
