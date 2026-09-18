cmake_minimum_required(VERSION 3.16)

project(VncViewer)


#
#
#

find_package(JPEG REQUIRED)


#
#
#

option(USE_ZLIB "Use system zlib instead of miniz" OFF)

if(DEFINED ENV{USE_ZLIB})
    if($ENV{USE_ZLIB})
        set(USE_ZLIB ON CACHE BOOL "Use system zlib instead of miniz" FORCE)
    else()
        set(USE_ZLIB OFF CACHE BOOL "Use system zlib instead of miniz" FORCE)
    endif()
endif()


if(USE_ZLIB)
    find_package(ZLIB REQUIRED)
endif()


#
#
#

set(CPP_SOURCES
    main.cpp
    VncDisplay.cpp
    VncScreen.cpp
    VncClient.cpp
)

set(C_SOURCES
    images/vnc_logo.c
    encrypt/d3des.c
    miniz/miniz.c
    miniz/miniz_tdef.c
    miniz/miniz_tinfl.c
    miniz/miniz_zip.c
    VncEventQueue.c
    zlib_inflate.c
    zrle_decode.c
    tight_decode.c    
)

set(HEADERS
    encrypt/d3des.h
    miniz/miniz.h
    miniz/miniz_common.h
    miniz/miniz_export.h
    miniz/miniz_tdef.h
    miniz/miniz_tinfl.h
    miniz/miniz_zip.h
    VncDisplay.h
    VncScreen.h
    VncClient.h
    VncEventQueue.h
    zlib_inflate.h
    zrle_decode.h
    tight_decode.h
)

add_executable(VncViewer ${CPP_SOURCES} ${C_SOURCES} ${HEADERS})

target_include_directories(VncViewer PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(
    VncViewer 
    PRIVATE 
    lvgl 
    $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)
target_link_libraries(VncViewer PRIVATE JPEG::JPEG)

if(USE_ZLIB)
    target_link_libraries(VncViewer PRIVATE ZLIB::ZLIB)
    target_compile_definitions(VncViewer PRIVATE USE_ZLIB)
endif()
