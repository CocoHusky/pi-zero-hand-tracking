CXX := g++
CXXFLAGS := -O3 -pipe -Wall -Wextra -std=c++17
SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS := $(shell sdl2-config --libs)

all:
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) main.cpp -o v4_tflite $(SDL_LIBS) -ltensorflow-lite

clean:
	rm -f v4_tflite
