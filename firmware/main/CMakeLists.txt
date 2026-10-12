idf_component_register(
    SRCS main.cpp sh1106.cpp touch.cpp link.cpp hw.cpp
    INCLUDE_DIRS .
    REQUIRES pet esp_driver_i2c esp_driver_gpio esp_driver_usb_serial_jtag esp_timer nvs_flash
)

# pet headers are c++17 (see the note in the project CMakeLists.txt)
target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++17>)
