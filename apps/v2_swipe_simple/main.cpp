#include <SDL2/SDL.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct Point { int x, y; };
struct Options { int width = 320, height = 240, fps = 60; bool mirror = true; };

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
    return true;
}

static std::string build_cmd(const Options& opt) {
    std::ostringstream cmd;
    cmd << "rpicam-vid -t 0 --nopreview --codec yuv420"
        << " --width " << opt.width
        << " --height " << opt.height
        << " --framerate " << opt.fps
        << " -o - 2>/tmp/swipe_v2.log";
    return cmd.str();
}

static void draw_circle(SDL_Renderer* r, int cx, int cy, int radius) {
    for (int dy = -radius; dy <= radius; ++dy)
        for (int dx = -radius; dx <= radius; ++dx)
            if (dx * dx + dy * dy <= radius * radius)
                SDL_RenderDrawPoint(r, cx + dx, cy + dy);
}

static std::string detect_dir(const std::deque<Point>& pts, bool mirror, int min_travel) {
    if (pts.size() < 4) return "";
    int dx = pts.back().x - pts.front().x;
    int dy = pts.back().y - pts.front().y;
    if (std::abs(dx) < min_travel && std::abs(dy) < min_travel) return "";
    if (std::abs(dx) > std::abs(dy)) {
        std::string d = dx > 0 ? "RIGHT" : "LEFT";
        if (mirror) d = (d == "RIGHT") ? "LEFT" : "RIGHT";
        return d;
    }
    return dy > 0 ? "DOWN" : "UP";
}

int main(int argc, char** argv) {
    Options opt;
    if (!parse_args(argc, argv, opt)) return 1;

    const int y_size = opt.width * opt.height;
    const int uv_w = opt.width / 2;
    const int uv_h = opt.height / 2;
    const int uv_size = uv_w * uv_h;
    const int frame_size = y_size + uv_size + uv_size;

    FILE* pipe = popen(build_cmd(opt).c_str(), "r");
    if (!pipe) return 1;

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    SDL_Window* window = SDL_CreateWindow("swipe_v2", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_FULLSCREEN_DESKTOP);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, opt.width, opt.height);

    std::vector<unsigned char> frame(frame_size);
    std::vector<unsigned char> prev_y(y_size, 0);
    bool have_prev = false;
    std::deque<Point> trail;
    bool tracking = false;
    int still_frames = 0;

    while (true) {
        size_t got = fread(frame.data(), 1, frame.size(), pipe);
        if (got != frame.size()) break;

        const Uint8* y = frame.data();
        const Uint8* u = frame.data() + y_size;
        const Uint8* v = frame.data() + y_size + uv_size;

        SDL_UpdateYUVTexture(texture, nullptr, y, opt.width, u, uv_w, v, uv_w);

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) goto done;
            if (e.type == SDL_KEYDOWN && (e.key.keysym.sym == SDLK_ESCAPE || e.key.keysym.sym == SDLK_q)) goto done;
        }

        int roi_x1 = opt.width / 4, roi_x2 = opt.width * 3 / 4;
        int roi_y1 = opt.height / 4, roi_y2 = opt.height * 3 / 4;

        bool found = false;
        int minx = roi_x2, miny = roi_y2, maxx = roi_x1 - 1, maxy = roi_y1 - 1;
        long sx = 0, sy = 0; int count = 0;

        if (have_prev) {
            for (int yy = roi_y1; yy < roi_y2; ++yy) {
                for (int xx = roi_x1; xx < roi_x2; ++xx) {
                    int idx = yy * opt.width + xx;
                    int diff = std::abs(static_cast<int>(y[idx]) - static_cast<int>(prev_y[idx]));
                    if (diff >= 24) {
                        found = true;
                        count++;
                        sx += xx; sy += yy;
                        if (xx < minx) minx = xx;
                        if (yy < miny) miny = yy;
                        if (xx > maxx) maxx = xx;
                        if (yy > maxy) maxy = yy;
                    }
                }
            }
        }

        if (found && count > 50) {
            Point c{static_cast<int>(sx / count), static_cast<int>(sy / count)};
            trail.push_back(c);
            if (trail.size() > 12) trail.pop_front();
            tracking = true;
            still_frames = 0;
        } else if (tracking) {
            still_frames++;
            if (still_frames >= 4) {
                std::string dir = detect_dir(trail, opt.mirror, 40);
                if (!dir.empty()) std::cout << "SWIPE: " << dir << "\n";
                trail.clear();
                tracking = false;
                still_frames = 0;
            }
        }

        std::memcpy(prev_y.data(), y, y_size);
        have_prev = true;

        int out_w = 0, out_h = 0;
        SDL_GetRendererOutputSize(renderer, &out_w, &out_h);
        SDL_Rect dst{0, 0, out_w, out_h};

        auto scale_x = [&](int x) { int px = x; if (opt.mirror) px = (opt.width - 1) - x; return dst.x + px * dst.w / opt.width; };
        auto scale_y = [&](int yy) { return dst.y + yy * dst.h / opt.height; };

        SDL_RenderClear(renderer);
        if (opt.mirror) SDL_RenderCopyEx(renderer, texture, nullptr, &dst, 0.0, nullptr, SDL_FLIP_HORIZONTAL);
        else SDL_RenderCopy(renderer, texture, nullptr, &dst);

        SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
        SDL_Rect roi{scale_x(roi_x2 - 1), scale_y(roi_y1), (roi_x2 - roi_x1) * dst.w / opt.width, (roi_y2 - roi_y1) * dst.h / opt.height};
        if (!opt.mirror) roi.x = scale_x(roi_x1);
        SDL_RenderDrawRect(renderer, &roi);

        if (found && count > 50) {
            SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
            SDL_Rect box{scale_x(maxx), scale_y(miny), std::max(1, (maxx - minx + 1) * dst.w / opt.width), std::max(1, (maxy - miny + 1) * dst.h / opt.height)};
            if (!opt.mirror) box.x = scale_x(minx);
            SDL_RenderDrawRect(renderer, &box);
            SDL_SetRenderDrawColor(renderer, 0, 100, 255, 255);
            draw_circle(renderer, scale_x(static_cast<int>(sx / count)), scale_y(static_cast<int>(sy / count)), 5);
        }

        if (trail.size() >= 2) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            for (size_t i = 1; i < trail.size(); ++i) {
                SDL_RenderDrawLine(renderer, scale_x(trail[i - 1].x), scale_y(trail[i - 1].y), scale_x(trail[i].x), scale_y(trail[i].y));
            }
        }

        SDL_RenderPresent(renderer);
    }

done:
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    pclose(pipe);
    return 0;
}
