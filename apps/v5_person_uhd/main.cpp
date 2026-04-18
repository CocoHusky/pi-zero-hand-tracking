#include <SDL2/SDL.h>

#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
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
    int fps = 15;
    int input = 320;
    bool mirror = true;
    int detect_every = 2;
    int person_class_id = 0;
    float conf_thresh = 0.35f;
    float nms_thresh = 0.45f;
    std::string model_path = "/home/user/models/uhd_person.onnx";
};

struct Detection {
    cv::Rect box;
    float conf;
};

static bool parse_args(int argc, char** argv, Options& opt) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--model" && i + 1 < argc) opt.model_path = argv[++i];
        else if (a == "--width" && i + 1 < argc) opt.width = std::atoi(argv[++i]);
        else if (a == "--height" && i + 1 < argc) opt.height = std::atoi(argv[++i]);
        else if (a == "--fps" && i + 1 < argc) opt.fps = std::atoi(argv[++i]);
        else if (a == "--input" && i + 1 < argc) opt.input = std::atoi(argv[++i]);
        else if (a == "--detect-every" && i + 1 < argc) opt.detect_every = std::atoi(argv[++i]);
        else if (a == "--person-class-id" && i + 1 < argc) opt.person_class_id = std::atoi(argv[++i]);
        else if (a == "--conf" && i + 1 < argc) opt.conf_thresh = std::atof(argv[++i]);
        else if (a == "--nms" && i + 1 < argc) opt.nms_thresh = std::atof(argv[++i]);
        else if (a == "--mirror") opt.mirror = true;
        else if (a == "--no-mirror") opt.mirror = false;
        else return false;
    }
    return opt.width > 0 && opt.height > 0 && opt.fps > 0 && opt.input > 0 && opt.detect_every > 0;
}

static std::string build_cmd(const Options& opt) {
    std::ostringstream cmd;
    cmd << "rpicam-vid -t 0 --nopreview --codec yuv420"
        << " --width " << opt.width
        << " --height " << opt.height
        << " --framerate " << opt.fps
        << " -o - 2>/tmp/person_uhd.log";
    return cmd.str();
}

static void draw_seg(SDL_Renderer* ren, int x, int y, int w, int h, bool on) {
    if (!on) return;
    SDL_Rect r{x, y, w, h};
    SDL_RenderFillRect(ren, &r);
}

static void draw_digit(SDL_Renderer* ren, int x, int y, int s, int d) {
    static const bool seg[10][7] = {
        {1,1,1,1,1,1,0}, {0,1,1,0,0,0,0}, {1,1,0,1,1,0,1}, {1,1,1,1,0,0,1}, {0,1,1,0,0,1,1},
        {1,0,1,1,0,1,1}, {1,0,1,1,1,1,1}, {1,1,1,0,0,0,0}, {1,1,1,1,1,1,1}, {1,1,1,1,0,1,1}
    };
    if (d < 0 || d > 9) return;

    int t = std::max(1, s / 4);
    int w = s;
    int h = s * 2;

    draw_seg(ren, x + t, y, w - 2 * t, t, seg[d][0]);
    draw_seg(ren, x + w - t, y + t, t, h / 2 - t, seg[d][1]);
    draw_seg(ren, x + w - t, y + h / 2, t, h / 2 - t, seg[d][2]);
    draw_seg(ren, x + t, y + h - t, w - 2 * t, t, seg[d][3]);
    draw_seg(ren, x, y + h / 2, t, h / 2 - t, seg[d][4]);
    draw_seg(ren, x, y + t, t, h / 2 - t, seg[d][5]);
    draw_seg(ren, x + t, y + h / 2 - t / 2, w - 2 * t, t, seg[d][6]);
}

static void draw_confidence(SDL_Renderer* ren, int x, int y, float conf) {
    int pct = std::max(0, std::min(99, static_cast<int>(conf * 100.0f + 0.5f)));
    int tens = pct / 10;
    int ones = pct % 10;
    draw_digit(ren, x, y, 10, tens);
    draw_digit(ren, x + 14, y, 10, ones);
}

static void decode_output(const cv::Mat& out, const Options& opt, int src_w, int src_h,
                          std::vector<cv::Rect>& boxes, std::vector<float>& scores) {
    const int item = out.size[2];
    const int dims = out.size[1];
    const float* data = reinterpret_cast<const float*>(out.data);

    float sx = static_cast<float>(src_w) / opt.input;
    float sy = static_cast<float>(src_h) / opt.input;

    for (int i = 0; i < dims; ++i) {
        const float* p = data + i * item;
        if (item < 6) continue;

        float obj = p[4];
        if (obj < 1e-6f) continue;

        int cls = 0;
        float cls_score = 1.0f;
        if (item > 5) {
            cls = 0;
            cls_score = p[5];
            for (int c = 6; c < item; ++c) {
                if (p[c] > cls_score) {
                    cls_score = p[c];
                    cls = c - 5;
                }
            }
        }
        if (cls != opt.person_class_id) continue;

        float conf = obj * cls_score;
        if (conf < opt.conf_thresh) continue;

        float cx = p[0];
        float cy = p[1];
        float w = p[2];
        float h = p[3];

        if (cx <= 2.0f && cy <= 2.0f && w <= 2.0f && h <= 2.0f) {
            cx *= opt.input;
            cy *= opt.input;
            w *= opt.input;
            h *= opt.input;
        }

        int x = static_cast<int>((cx - w * 0.5f) * sx);
        int y = static_cast<int>((cy - h * 0.5f) * sy);
        int bw = static_cast<int>(w * sx);
        int bh = static_cast<int>(h * sy);

        bw = std::max(1, bw);
        bh = std::max(1, bh);
        x = std::max(0, std::min(src_w - 1, x));
        y = std::max(0, std::min(src_h - 1, y));
        if (x + bw > src_w) bw = src_w - x;
        if (y + bh > src_h) bh = src_h - y;

        boxes.emplace_back(x, y, bw, bh);
        scores.push_back(conf);
    }
}

int main(int argc, char** argv) {
    Options opt;
    if (!parse_args(argc, argv, opt)) {
        std::cerr << "Usage: ./person_uhd --model ABS_PATH [--width N --height N --fps N --input N --conf F --nms F --detect-every N --person-class-id N --mirror|--no-mirror]\n";
        return 1;
    }

    if (opt.model_path.empty() || opt.model_path[0] != '/') {
        std::cerr << "Model path must be absolute. Got: " << opt.model_path << "\n";
        return 1;
    }

    cv::dnn::Net net;
    try {
        net = cv::dnn::readNet(opt.model_path);
    } catch (const std::exception& e) {
        std::cerr << "Failed to load model: " << e.what() << "\n";
        return 1;
    }
    if (net.empty()) {
        std::cerr << "Model did not load: " << opt.model_path << "\n";
        return 1;
    }
    net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);

    const int y_size = opt.width * opt.height;
    const int uv_w = opt.width / 2;
    const int uv_size = uv_w * (opt.height / 2);
    const int frame_size = y_size + uv_size + uv_size;

    FILE* pipe = popen(build_cmd(opt).c_str(), "r");
    if (!pipe) return 1;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) return 1;
    SDL_Window* win = SDL_CreateWindow("person_uhd", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_FULLSCREEN_DESKTOP);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture* tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, opt.width, opt.height);

    std::vector<unsigned char> frame(frame_size);
    std::vector<Detection> dets;
    int counter = 0;
    int detect_ms = 0;

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
        if (counter % opt.detect_every == 0) {
            auto t0 = std::chrono::steady_clock::now();

            cv::Mat yuv420(opt.height + opt.height / 2, opt.width, CV_8UC1, frame.data());
            cv::Mat bgr;
            cv::cvtColor(yuv420, bgr, cv::COLOR_YUV2BGR_I420);

            cv::Mat blob = cv::dnn::blobFromImage(bgr, 1.0f / 255.0f, cv::Size(opt.input, opt.input), cv::Scalar(), true, false);
            net.setInput(blob);
            cv::Mat out = net.forward();

            std::vector<cv::Rect> boxes;
            std::vector<float> scores;

            if (out.dims == 3) {
                decode_output(out, opt, bgr.cols, bgr.rows, boxes, scores);
            }

            std::vector<int> keep;
            cv::dnn::NMSBoxes(boxes, scores, opt.conf_thresh, opt.nms_thresh, keep);

            dets.clear();
            for (int idx : keep) {
                dets.push_back({boxes[idx], scores[idx]});
            }

            auto t1 = std::chrono::steady_clock::now();
            detect_ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());

            float top_conf = 0.0f;
            for (const auto& d : dets) top_conf = std::max(top_conf, d.conf);
            std::cout << "person_count=" << dets.size() << " top_conf=" << top_conf << " detect_ms=" << detect_ms << "\n";
        }

        SDL_RenderClear(ren);
        SDL_Rect dst{0, 0, 640, 480};
        if (opt.mirror) SDL_RenderCopyEx(ren, tex, nullptr, &dst, 0.0, nullptr, SDL_FLIP_HORIZONTAL);
        else SDL_RenderCopy(ren, tex, nullptr, &dst);

        SDL_SetRenderDrawColor(ren, 0, 255, 0, 255);
        for (const auto& d : dets) {
            int x = opt.mirror
                ? static_cast<int>((opt.width - 1 - (d.box.x + d.box.width)) * dst.w / opt.width)
                : static_cast<int>(d.box.x * dst.w / opt.width);
            int y = static_cast<int>(d.box.y * dst.h / opt.height);
            int w = std::max(1, static_cast<int>(d.box.width * dst.w / opt.width));
            int h = std::max(1, static_cast<int>(d.box.height * dst.h / opt.height));

            SDL_Rect box{x, y, w, h};
            SDL_RenderDrawRect(ren, &box);

            draw_confidence(ren, x + 2, std::max(0, y - 22), d.conf);
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
