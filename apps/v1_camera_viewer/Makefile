CXX := g++
CXXFLAGS := -O3 -pipe -Wall -Wextra -std=c++17
SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS := $(shell sdl2-config --libs)

TARGET ?= camera_viewer
SRC ?= main.cpp

all:
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) $(SRC) -o $(TARGET) $(SDL_LIBS)

clean:
	rm -f $(TARGET)
