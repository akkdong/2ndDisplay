# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

set(EXTRA_COMPONENT_DIRS ./components)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)

project(LvglTest)

idf_component_get_property(arduino_lib espressif__arduino-esp32 COMPONENT_LIB)
if(TARGET ${arduino_lib})
    target_compile_definitions(${arduino_lib} PUBLIC
        -DARDUINO_USB_MODE=1
        -DARDUINO_USB_CDC_ON_BOOT=1
    )
endif()

idf_component_get_property(modbus_lib espressif__esp-modbus COMPONENT_LIB)
if(TARGET ${modbus_lib})
    target_compile_options(${modbus_lib} PRIVATE "-Wno-error=stringop-truncation")
endif()

#
#add_compile_options("-Wno-stringop-truncation")
#
