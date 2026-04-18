#include <SDL2/SDL.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>
#include <vector>

struct Options { int width = 320, height = 240, fps = 60; bool mirror = true; };
struct Point { int x, y; };
struct Box { int x, y, w, h; };

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
        << " -o - 2>/tmp/swipe_v3.log";
    return cmd.str();
}

static std::string classify(const std::deque<Point>& pts, bool mirror, int min_travel) {
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
    SDL_Window* win = SDL_CreateWindow("swipe_v3", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_FULLSCREEN_DESKTOP);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture* tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, opt.width, opt.height);

    std::vector<unsigned char> frame(frame_size), prev_y(y_size, 0), mask(y_size, 0), seen(y_size, 0);
    bool have_prev = false;
    std::deque<Point> trail;
    bool tracking = false;
    int still = 0;

    while (true) {
        size_t got = fread(frame.data(), 1, frame.size(), pipe);
        if (got != frame.size()) break;

        const Uint8* y = frame.data();
        const Uint8* u = frame.data() + y_size;
        const Uint8* v = frame.data() + y_size + uv_size;
        SDL_UpdateYUVTexture(tex, nullptr, y, opt.width, u, uv_w, v, uv_w);

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) goto done;
            if (e.type == SDL_KEYDOWN && (e.key.keysym.sym == SDLK_ESCAPE || e.key.keysym.sym == SDLK_q)) goto done;
        }

        int roi_x1 = opt.width / 4, roi_x2 = opt.width * 3 / 4;
        int roi_y1 = opt.height / 4, roi_y2 = opt.height * 3 / 4;

        bool found = false;
        Box best{0, 0, 0, 0};
        Point center{0, 0};

        if (have_prev) {
            std::fill(mask.begin(), mask.end(), 0);
            std::fill(seen.begin(), seen.end(), 0);

            for (int yy = roi_y1; yy < roi_y2; ++yy)
                for (int xx = roi_x1; xx < roi_x2; ++xx) {
                    int idx = yy * opt.width + xx;
                    if (std::abs(static_cast<int>(y[idx]) - static_cast<int>(prev_y[idx])) >= 20) mask[idx] = 1;
                }

            int best_area = 0;
            for (int yy = roi_y1; yy < roi_y2; ++yy) {
                for (int xx = roi_x1; xx < roi_x2; ++xx) {
                    int idx = yy * opt.width + xx;
                    if (!mask[idx] || seen[idx]) continue;

                    std::queue<Point> q;
                    q.push({xx, yy});
                    seen[idx] = 1;

                    int minx = xx, miny = yy, maxx = xx, maxy = yy;
                    int area = 0; long sx_sum = 0, sy_sum = 0;

                    while (!q.empty()) {
                        Point p = q.front(); q.pop();
                        area++; sx_sum += p.x; sy_sum += p.y;
                        minx = std::min(minx, p.x); miny = std::min(miny, p.y);
                        maxx = std::max(maxx, p.x); maxy = std::max(maxy, p.y);

                        static const int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
                        for (auto& d : dirs) {
                            int nx = p.x + d[0], ny = p.y + d[1];
                            if (nx < roi_x1 || nx >= roi_x2 || ny < roi_y1 || ny >= roi_y2) continue;
                            int nidx = ny * opt.width + nx;
                            if (!mask[nidx] || seen[nidx]) continue;
                            seen[nidx] = 1;
                            q.push({nx, ny});
                        }
                    }

                    int bw = maxx - minx + 1, bh = maxy - miny + 1;
                    if (area < 60 || area > (opt.width * opt.height) / 6) continue;
                    float aspect = static_cast<float>(bw) / bh;
                    if (aspect < 0.4f || aspect > 2.0f) continue;

                    if (area > best_area) {
                        best_area = area;
                        best = {minx, miny, bw, bh};
                        center = {static_cast<int>(sx_sum / area), static_cast<int>(sy_sum / area)};
                        found = true;
                    }
                }
            }
        }

        if (found) {
            trail.push_back(center);
            if (trail.size() > 12) trail.pop_front();
            tracking = true;
            still = 0;
        } else if (tracking) {
            still++;
            if (still >= 4) {
                std::string d = classify(trail, opt.mirror, 40);
                if (!d.empty()) std::cout << "SWIPE: " << d << "\n";
                trail.clear();
                tracking = false;
                still = 0;
            }
        }

        std::memcpy(prev_y.data(), y, y_size);
        have_prev = true;

        int out_w = 0, out_h = 0;
        SDL_GetRendererOutputSize(ren, &out_w, &out_h);
        SDL_Rect dst{0, 0, out_w, out_h};

        auto sx = [&](int x) { int px = x; if (opt.mirror) px = (opt.width - 1) - x; return dst.x + px * dst.w / opt.width; };
        auto sy = [&](int yy) { return dst.y + yy * dst.h / opt.height; };

        SDL_RenderClear(ren);
        if (opt.mirror) SDL_RenderCopyEx(ren, tex, nullptr, &dst, 0.0, nullptr, SDL_FLIP_HORIZONTAL);
        else SDL_RenderCopy(ren, tex, nullptr, &dst);

        SDL_SetRenderDrawColor(ren, 255, 255, 0, 255);
        SDL_Rect roi{sx(roi_x2 - 1), sy(roi_y1), (roi_x2 - roi_x1) * dst.w / opt.width, (roi_y2 - roi_y1) * dst.h / opt.height};
        if (!opt.mirror) roi.x = sx(roi_x1);
        SDL_RenderDrawRect(ren, &roi);

        if (found) {
            SDL_SetRenderDrawColor(ren, 0, 255, 0, 255);
            SDL_Rect box{sx(best.x + best.w - 1), sy(best.y), best.w * dst.w / opt.width, best.h * dst.h / opt.height};
            if (!opt.mirror) box.x = sx(best.x);
            SDL_RenderDrawRect(ren, &box);
            SDL_SetRenderDrawColor(ren, 0, 100, 255, 255);
            for (int dy = -4; dy <= 4; ++dy)
                for (int dx = -4; dx <= 4; ++dx)
                    if (dx * dx + dy * dy <= 16) SDL_RenderDrawPoint(ren, sx(center.x) + dx, sy(center.y) + dy);
        }

        if (trail.size() >= 2) {
            SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
            for (size_t i = 1; i < trail.size(); ++i) SDL_RenderDrawLine(ren, sx(trail[i - 1].x), sy(trail[i - 1].y), sx(trail[i].x), sy(trail[i].y));
        }

        SDL_RenderPresent(ren);
    }

done:
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    pclose(pipe);
    return 0;
}
