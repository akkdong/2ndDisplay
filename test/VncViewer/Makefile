# 프로그램 이름
TARGET = VncViewer

# 빌드 디렉터리 설정
BUILD_DIR = build

# 컴파일러 설정
CC = gcc
CXX = g++

# 컴파일 플래그 설정
CFLAGS = -Wall -Wextra -O3 -g -I. -I./lvgl $(shell pkg-config --cflags sdl2)
CXXFLAGS = -Wall -Wextra -O3 -g -std=c++17 -I. -I./lvgl  $(shell pkg-config --cflags sdl2)

# 링커 플래그 (SDL2 및 C++ 표준 라이브러리 링크)
LDFLAGS = -lSDL2 -lm -lstdc++ -ljpeg -lz $(shell pkg-config --libs sdl2)

# LVGL 공식 Make 설정 가져오기 (CSRCS 변수 활성화)
LVGL_DIR_NAME = lvgl
include $(LVGL_DIR_NAME)/lvgl-minimal.mk

# 소스 파일 분류
CSRCS += d3des.c 
CPPSRCS = VncViewer.cpp Display.cpp main.cpp

# 오브젝트 파일 목록 정의
CUR_DIR := $(shell pwd)
CSRCS_REL := $(patsubst $(CUR_DIR)/%, %, $(CSRCS))
CPPSRCS_REL := $(patsubst $(CUR_DIR)/%, %, $(CPPSRCS))

#COBJS = $(addprefix $(BUILD_DIR)/, $(CSRCS:.c=.o))
#CPPOBJS = $(addprefix $(BUILD_DIR)/, $(CPPSRCS:.cpp=.o))
#OBJS = $(COBJS) $(CPPOBJS)
OBJS = $(addprefix $(BUILD_DIR)/, $(CSRCS_REL:.c=.o) $(CPPSRCS_REL:.cpp=.o))

# 기본 빌드 타겟
all: $(TARGET)

# 최종 바이너리 링크 (C++ 컴파일러인 g++를 사용하여 링크)
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

# C 파일 컴파일 규칙
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# C++ 파일 컴파일 규칙
$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 정리 규칙
clean:
	rm -rf $(TARGET) $(BUILD_DIR)/
