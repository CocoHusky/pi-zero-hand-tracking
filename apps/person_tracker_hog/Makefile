CXX := g++
CXXFLAGS := -O3 -pipe -Wall -Wextra -std=c++17
SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS := $(shell sdl2-config --libs)
OPENCV_CFLAGS := $(shell pkg-config --cflags opencv4)
OPENCV_LIBS := $(shell pkg-config --libs opencv4)

all:
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) $(OPENCV_CFLAGS) main.cpp -o person_tracker_hog $(SDL_LIBS) $(OPENCV_LIBS)

clean:
	rm -f person_tracker_hog
