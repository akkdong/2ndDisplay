cmake_minimum_required(VERSION 3.16)

project(VncViewer)



#
#
#

find_package(SDL2 CONFIG REQUIRED)




set(KCONFIG_INCLUDE_DIR "${CMAKE_BINARY_DIR}/config")
include_directories(${KCONFIG_INCLUDE_DIR})


#
#
#

set(LV_BUILD_USE_KCONFIG ON CACHE INTERNAL "")
set(LV_BUILD_DEFCONFIG_PATH "${CMAKE_CURRENT_SOURCE_DIR}/.config" CACHE INTERNAL "")
set(LV_CONF_SKIP ON CACHE INTERNAL "") 

add_subdirectory(lvgl)

target_include_directories(lvgl PRIVATE ${SDL2_INCLUDE_DIRS})


#
#
#

add_subdirectory(src)
