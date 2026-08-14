cmake_minimum_required(VERSION 3.15)
project(lvgl_lib LANGUAGES C CXX)

# MSVC 런타임 라이브러리 설정을 메인 프로젝트와 동기화하기 위한 정책 활성화
cmake_policy(SET CMP0091 NEW)

# 메인 프로젝트의 런타임 라이브러리에 맞춤 (기본값 /MD, /MDd)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# 정적 라이브러리(.lib) 생성 설정
set(BUILD_SHARED_LIBS OFF)

# lv_conf.h 경로 설정
set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE STRING "" FORCE)

# 🌟 중요: 소스 코드 내부에서 #include LV_CONF_PATH 처리 시 따옴표가 들어가도록 이스케이프 처리
add_compile_definitions(LV_CONF_PATH=\"${LV_CONF_PATH}\")
add_compile_definitions(LV_CONF_INCLUDE_SIMPLE)

# LVGL 소스 트리 추가
add_subdirectory(lvgl)
