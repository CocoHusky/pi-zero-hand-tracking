#include <SDL2/SDL.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct Options { int width = 320, height = 240, fps = 15; bool mirror = true; };
struct TrackBox { bool valid = false; float x = 0, y = 0, w = 0, h = 0; int miss_count = 0; };

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
    cmd << "rpicam-vid -t 0 --nopreview --codec yuv420 --width " << opt.width << " --height " << opt.height << " --framerate " << opt.fps << " -o - 2>/tmp/person_hog.log";
    return cmd.str();
}

int main(int argc, char** argv) {
    Options opt;
    if (!parse_args(argc, argv, opt)) return 1;

    const int y_size = opt.width * opt.height, uv_w = opt.width / 2, uv_h = opt.height / 2, uv_size = uv_w * uv_h, frame_size = y_size + uv_size + uv_size;
    FILE* pipe = popen(build_cmd(opt).c_str(), "r");
    if (!pipe) return 1;

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    SDL_Window* win = SDL_CreateWindow("person_hog", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_FULLSCREEN_DESKTOP);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture* tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, opt.width, opt.height);

    cv::HOGDescriptor hog;
    hog.setSVMDetector(cv::HOGDescriptor::getDefaultPeopleDetector());

    std::vector<unsigned char> frame(frame_size);
    TrackBox track;
    const int detect_every_n = 3, max_misses = 10;
    int counter = 0, detect_ms = 0;
    auto last = std::chrono::steady_clock::now(); int fps_count = 0;

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

        counter++;
        if (counter % detect_every_n == 0) {
            auto t0 = std::chrono::steady_clock::now();
            cv::Mat gray(opt.height, opt.width, CV_8UC1, const_cast<Uint8*>(y));
            cv::Mat eq; cv::equalizeHist(gray, eq);
            std::vector<cv::Rect> found; std::vector<double> weights;
            hog.detectMultiScale(eq, found, weights, 0.0, cv::Size(4, 4), cv::Size(8, 8), 1.03, 1.0, false);
            auto t1 = std::chrono::steady_clock::now();
            detect_ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());

            int best = -1; double best_score = -1e9;
            for (size_t i = 0; i < found.size(); ++i) {
                double score = (i < weights.size()) ? weights[i] : 0.0;
                score += found[i].area() * 0.002;
                if (score > best_score) { best_score = score; best = static_cast<int>(i); }
            }

            if (best >= 0) {
                auto r = found[best];
                float nx = r.x, ny = r.y, nw = r.width, nh = r.height;
                if (!track.valid) { track.valid = true; track.x = nx; track.y = ny; track.w = nw; track.h = nh; }
                else {
                    float s = 0.55f;
                    track.x = track.x * s + nx * (1 - s);
                    track.y = track.y * s + ny * (1 - s);
                    track.w = track.w * s + nw * (1 - s);
                    track.h = track.h * s + nh * (1 - s);
                }
                track.miss_count = 0;
            } else if (track.valid) {
                track.miss_count++;
                if (track.miss_count > max_misses) track.valid = false;
            }
        }

        SDL_RenderClear(ren);
        SDL_Rect dst{0, 0, 640, 480};
        if (opt.mirror) SDL_RenderCopyEx(ren, tex, nullptr, &dst, 0.0, nullptr, SDL_FLIP_HORIZONTAL);
        else SDL_RenderCopy(ren, tex, nullptr, &dst);

        if (track.valid) {
            SDL_SetRenderDrawColor(ren, 0, 255, 0, 255);
            int x = opt.mirror ? static_cast<int>((opt.width - 1 - (track.x + track.w)) * 640 / opt.width) : static_cast<int>(track.x * 640 / opt.width);
            SDL_Rect box{x, static_cast<int>(track.y * 480 / opt.height), std::max(1, static_cast<int>(track.w * 640 / opt.width)), std::max(1, static_cast<int>(track.h * 480 / opt.height))};
            SDL_RenderDrawRect(ren, &box);
        }

        SDL_RenderPresent(ren);

        fps_count++;
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count() >= 1000) {
            std::cout << "FPS: " << fps_count << " detect_ms=" << detect_ms << " person=" << (track.valid ? 1 : 0) << "\n";
            fps_count = 0; last = now;
        }
    }

done:
    SDL_DestroyTexture(tex); SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
    pclose(pipe);
    return 0;
}
