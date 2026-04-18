#include <SDL2/SDL.h>
#include <tensorflow/lite/c/c_api.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static inline unsigned char clamp_u8(int v) { return v < 0 ? 0 : (v > 255 ? 255 : static_cast<unsigned char>(v)); }
static inline void yuv_to_rgb(unsigned char y, unsigned char u, unsigned char v, unsigned char& r, unsigned char& g, unsigned char& b) {
    int c = static_cast<int>(y) - 16, d = static_cast<int>(u) - 128, e = static_cast<int>(v) - 128;
    r = clamp_u8((298 * c + 409 * e + 128) >> 8);
    g = clamp_u8((298 * c - 100 * d - 208 * e + 128) >> 8);
    b = clamp_u8((298 * c + 516 * d + 128) >> 8);
}

int main() {
    const int width = 160, height = 120, fps = 15;
    const int y_size = width * height, uv_w = width / 2, uv_h = height / 2, uv_size = uv_w * uv_h, frame_size = y_size + uv_size + uv_size;

    std::ostringstream cmd;
    cmd << "rpicam-vid -t 0 --nopreview --codec yuv420 --width " << width << " --height " << height << " --framerate " << fps << " -o - 2>/tmp/v4_tflite.log";
    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) return 1;

    TfLiteModel* model = TfLiteModelCreateFromFile("/home/user/camera-viewer-v4/models/hand_landmark.tflite");
    TfLiteInterpreterOptions* opts = TfLiteInterpreterOptionsCreate();
    TfLiteInterpreterOptionsSetNumThreads(opts, 2);
    TfLiteInterpreter* interp = TfLiteInterpreterCreate(model, opts);
    if (!interp || TfLiteInterpreterAllocateTensors(interp) != kTfLiteOk) return 1;

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    SDL_Window* win = SDL_CreateWindow("v4", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_FULLSCREEN_DESKTOP);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture* tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, width, height);

    std::vector<unsigned char> frame(frame_size);
    auto last = std::chrono::steady_clock::now();
    int frames = 0;

    while (true) {
        size_t got = fread(frame.data(), 1, frame.size(), pipe);
        if (got != frame.size()) break;

        const Uint8* y = frame.data();
        const Uint8* u = frame.data() + y_size;
        const Uint8* v = frame.data() + y_size + uv_size;
        SDL_UpdateYUVTexture(tex, nullptr, y, width, u, uv_w, v, uv_w);

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) goto done;
            if (e.type == SDL_KEYDOWN && (e.key.keysym.sym == SDLK_ESCAPE || e.key.keysym.sym == SDLK_q)) goto done;
        }

        float roi_x = width * 0.2f, roi_y = height * 0.1f, roi_w = width * 0.6f, roi_h = width * 0.6f;
        TfLiteTensor* input = TfLiteInterpreterGetInputTensor(interp, 0);
        float* in = static_cast<float*>(TfLiteTensorData(input));

        auto t0 = std::chrono::steady_clock::now();
        for (int iy = 0; iy < 256; ++iy) {
            float syf = roi_y + (iy + 0.5f) * roi_h / 256.0f;
            int sy = std::max(0, std::min(height - 1, static_cast<int>(syf)));
            for (int ix = 0; ix < 256; ++ix) {
                float sxf = roi_x + (ix + 0.5f) * roi_w / 256.0f;
                int sx = std::max(0, std::min(width - 1, static_cast<int>(sxf)));
                int yi = sy * width + sx;
                int ui = (sy / 2) * uv_w + (sx / 2);
                unsigned char rr, gg, bb;
                yuv_to_rgb(y[yi], u[ui], v[ui], rr, gg, bb);
                int base = (iy * 256 + ix) * 3;
                in[base + 0] = rr / 255.0f; in[base + 1] = gg / 255.0f; in[base + 2] = bb / 255.0f;
            }
        }
        TfLiteInterpreterInvoke(interp);
        auto t1 = std::chrono::steady_clock::now();
        int infer_ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());

        const float* ld = static_cast<const float*>(TfLiteTensorData(TfLiteInterpreterGetOutputTensor(interp, 0)));
        const float* flag = static_cast<const float*>(TfLiteTensorData(TfLiteInterpreterGetOutputTensor(interp, 1)));
        bool has_hand = flag && flag[0] > 0.5f;

        SDL_RenderClear(ren);
        SDL_Rect dst{0, 0, 640, 480};
        SDL_RenderCopyEx(ren, tex, nullptr, &dst, 0.0, nullptr, SDL_FLIP_HORIZONTAL);

        SDL_SetRenderDrawColor(ren, 255, 255, 0, 255);
        SDL_Rect roi{static_cast<int>((width - 1 - (roi_x + roi_w)) * 640 / width), static_cast<int>(roi_y * 480 / height), static_cast<int>(roi_w * 640 / width), static_cast<int>(roi_h * 480 / height)};
        SDL_RenderDrawRect(ren, &roi);

        if (has_hand && ld) {
            float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f, sx_sum = 0, sy_sum = 0;
            for (int i = 0; i < 21; ++i) {
                float x = ld[i * 3 + 0], yy = ld[i * 3 + 1];
                minx = std::min(minx, x); miny = std::min(miny, yy); maxx = std::max(maxx, x); maxy = std::max(maxy, yy);
                sx_sum += x; sy_sum += yy;
            }
            float cx = roi_x + (sx_sum / 21.0f) * roi_w, cy = roi_y + (sy_sum / 21.0f) * roi_h;
            float bx = roi_x + minx * roi_w, by = roi_y + miny * roi_h, bw = (maxx - minx) * roi_w, bh = (maxy - miny) * roi_h;

            SDL_SetRenderDrawColor(ren, 0, 255, 0, 255);
            SDL_Rect box{static_cast<int>((width - 1 - (bx + bw)) * 640 / width), static_cast<int>(by * 480 / height), static_cast<int>(bw * 640 / width), static_cast<int>(bh * 480 / height)};
            SDL_RenderDrawRect(ren, &box);

            SDL_SetRenderDrawColor(ren, 0, 100, 255, 255);
            int px = static_cast<int>((width - 1 - cx) * 640 / width), py = static_cast<int>(cy * 480 / height);
            for (int dy = -4; dy <= 4; ++dy) for (int dx = -4; dx <= 4; ++dx) if (dx * dx + dy * dy <= 16) SDL_RenderDrawPoint(ren, px + dx, py + dy);
        }

        SDL_RenderPresent(ren);

        frames++;
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count() >= 1000) {
            std::cout << "FPS: " << frames << " landmark_ms=" << infer_ms << " hand=" << (has_hand ? 1 : 0) << "\n";
            frames = 0; last = now;
        }
    }

done:
    SDL_DestroyTexture(tex); SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
    TfLiteInterpreterDelete(interp); TfLiteInterpreterOptionsDelete(opts); TfLiteModelDelete(model);
    pclose(pipe);
    return 0;
}
