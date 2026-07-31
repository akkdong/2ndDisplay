CXX      = g++
CC       = gcc
CXXFLAGS = -std=c++20 -Wall -Wextra -O2 -g $(shell pkg-config --cflags sdl2)
LDFLAGS  = $(shell pkg-config --libs sdl2) -lz -ljpeg

TARGET = VncViewer
OBJS   = VncViewer.o d3des.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

VncViewer.o: VncViewer.cpp d3des.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

d3des.o: d3des.c d3des.h
	$(CC) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
