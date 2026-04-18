#include <SDL2/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    std::string model_path = "/home/user/ml-person/models/person_detection_mediapipe_2023mar.onnx";
    if (argc > 1) model_path = argv[1];

    std::cerr << "person_tracker_onnx is an experimental stub.\n";
    std::cerr << "Use an absolute model path (received): " << model_path << "\n";
    std::cerr << "This app shell keeps SDL+rpicam-vid constraints for future integration.\n";

    const int width = 320, height = 240, fps = 15;
    const int y_size = width * height;
    const int uv_w = width / 2;
    const int uv_h = height / 2;
    const int uv_size = uv_w * uv_h;
    const int frame_size = y_size + uv_size + uv_size;

    std::ostringstream cmd;
    cmd << "rpicam-vid -t 0 --nopreview --codec yuv420"
        << " --width " << width
        << " --height " << height
        << " --framerate " << fps
        << " -o - 2>/tmp/person_onnx.log";

    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) return 1;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) return 1;
    SDL_Window* win = SDL_CreateWindow("person_onnx_stub", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_FULLSCREEN_DESKTOP);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture* tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, width, height);

    std::vector<unsigned char> frame(frame_size);
    bool running = true;
    while (running) {
        size_t got = fread(frame.data(), 1, frame.size(), pipe);
        if (got != frame.size()) break;

        const Uint8* y = frame.data();
        const Uint8* u = frame.data() + y_size;
        const Uint8* v = frame.data() + y_size + uv_size;
        SDL_UpdateYUVTexture(tex, nullptr, y, width, u, uv_w, v, uv_w);

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN && (e.key.keysym.sym == SDLK_ESCAPE || e.key.keysym.sym == SDLK_q)) running = false;
        }

        SDL_RenderClear(ren);
        SDL_Rect dst{0, 0, 640, 480};
        SDL_RenderCopyEx(ren, tex, nullptr, &dst, 0.0, nullptr, SDL_FLIP_HORIZONTAL);

        SDL_SetRenderDrawColor(ren, 255, 255, 0, 255);
        SDL_Rect note_box{80, 60, 480, 40};
        SDL_RenderDrawRect(ren, &note_box);

        SDL_RenderPresent(ren);
    }

    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    pclose(pipe);
    return 0;
}
