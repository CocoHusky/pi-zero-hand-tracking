#include <SDL2/SDL.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct Options {
    int width = 320;
    int height = 240;
    int fps = 60;
    bool mirror = true;
};

static bool parse_args(int argc, char** argv, Options& opt) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--width" && i + 1 < argc) opt.width = std::atoi(argv[++i]);
        else if (a == "--height" && i + 1 < argc) opt.height = std::atoi(argv[++i]);
        else if (a == "--fps" && i + 1 < argc) opt.fps = std::atoi(argv[++i]);
        else if (a == "--mirror") opt.mirror = true;
        else if (a == "--no-mirror") opt.mirror = false;
        else return false;
    }
    return opt.width > 0 && opt.height > 0 && opt.fps > 0;
}

static std::string build_rpicam_cmd(const Options& opt) {
    std::ostringstream cmd;
    cmd << "rpicam-vid"
        << " -t 0"
        << " --nopreview"
        << " --codec yuv420"
        << " --width " << opt.width
        << " --height " << opt.height
        << " --framerate " << opt.fps
        << " -o -"
        << " 2>/tmp/camera_viewer_rpicam.log";
    return cmd.str();
}

int main(int argc, char** argv) {
    Options opt;
    if (!parse_args(argc, argv, opt)) {
        std::cerr << "Usage: ./camera_viewer [--width N] [--height N] [--fps N] [--mirror|--no-mirror]\n";
        return 1;
    }

    const int y_size = opt.width * opt.height;
    const int uv_w = opt.width / 2;
    const int uv_h = opt.height / 2;
    const int uv_size = uv_w * uv_h;
    const int frame_size = y_size + uv_size + uv_size;

    FILE* pipe = popen(build_rpicam_cmd(opt).c_str(), "r");
    if (!pipe) return 1;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) return 1;

    SDL_Window* window = SDL_CreateWindow(
        "camera_viewer",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        640, 480,
        SDL_WINDOW_FULLSCREEN_DESKTOP
    );
    if (!window) return 1;

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!renderer) return 1;

    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_IYUV,
        SDL_TEXTUREACCESS_STREAMING,
        opt.width,
        opt.height
    );
    if (!texture) return 1;

    std::vector<unsigned char> frame(frame_size);
    auto last = std::chrono::steady_clock::now();
    int frames = 0;
    bool running = true;

    while (running) {
        size_t got = fread(frame.data(), 1, frame.size(), pipe);
        if (got != frame.size()) break;

        const Uint8* y_plane = frame.data();
        const Uint8* u_plane = frame.data() + y_size;
        const Uint8* v_plane = frame.data() + y_size + uv_size;

        if (SDL_UpdateYUVTexture(texture, nullptr, y_plane, opt.width, u_plane, uv_w, v_plane, uv_w) != 0) {
            break;
        }

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN &&
                (e.key.keysym.sym == SDLK_ESCAPE || e.key.keysym.sym == SDLK_q)) running = false;
        }

        int out_w = 0, out_h = 0;
        SDL_GetRendererOutputSize(renderer, &out_w, &out_h);

        SDL_Rect dst{};
        float src_aspect = static_cast<float>(opt.width) / opt.height;
        float out_aspect = static_cast<float>(out_w) / out_h;
        if (out_aspect > src_aspect) {
            dst.h = out_h;
            dst.w = static_cast<int>(out_h * src_aspect);
            dst.x = (out_w - dst.w) / 2;
            dst.y = 0;
        } else {
            dst.w = out_w;
            dst.h = static_cast<int>(out_w / src_aspect);
            dst.x = 0;
            dst.y = (out_h - dst.h) / 2;
        }

        SDL_RenderClear(renderer);
        if (opt.mirror) SDL_RenderCopyEx(renderer, texture, nullptr, &dst, 0.0, nullptr, SDL_FLIP_HORIZONTAL);
        else SDL_RenderCopy(renderer, texture, nullptr, &dst);
        SDL_RenderPresent(renderer);

        frames++;
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count() >= 1000) {
            std::cout << "FPS: " << frames << "\n";
            frames = 0;
            last = now;
        }
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    pclose(pipe);
    return 0;
}
