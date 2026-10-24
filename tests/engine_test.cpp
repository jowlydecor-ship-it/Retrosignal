// Banc d'essai : génère une mire synthétique, applique les 15 looks, écrit des PPM.
#include "../src/Engine.h"
#include <cstdio>
#include <cmath>
#include <chrono>
using namespace rsc;
static Frame makeTest(int W, int H) {
    Frame f; f.w = W; f.h = H; for (auto& c : f.c) c = Plane(W, H);
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
        float u = (float)x / W, v = (float)y / H, r, g, b;
        if (v < 0.55f) { // ciel / dégradé + soleil
            r = 0.35f + 0.4f * u; g = 0.55f + 0.2f * (1 - v); b = 0.9f - 0.3f * v;
            float dx = u - 0.78f, dy = v - 0.2f; if (dx * dx * 2.f + dy * dy * 4.f < 0.012f) { r = 1.6f; g = 1.5f; b = 1.2f; }
        } else { r = 0.25f + 0.2f * v; g = 0.45f; b = 0.2f; }
        int bar = (int)(u * 7); if (v > 0.8f) { static const float C[7][3] = {{1,1,1},{1,1,0},{0,1,1},{0,1,0},{1,0,1},{1,0,0},{0,0,1}}; r = C[bar][0] * .75f; g = C[bar][1] * .75f; b = C[bar][2] * .75f; }
        float cx = u - 0.3f, cy = v - 0.5f; if (cx * cx + cy * cy * 1.8f < 0.02f) { r = 0.85f; g = 0.55f; b = 0.45f; }
        if (v > 0.6f && v < 0.78f && ((x / 12 + y / 12) & 1) && u > 0.5f) { r = g = b = 0.9f; }
        f.c[0].row(y)[x] = r; f.c[1].row(y)[x] = g; f.c[2].row(y)[x] = b;
    }
    return f;
}
static void writePPM(const char* fn, const Frame& f) {
    FILE* o = std::fopen(fn, "wb"); std::fprintf(o, "P6\n%d %d\n255\n", f.w, f.h);
    for (int y = 0; y < f.h; ++y) for (int x = 0; x < f.w; ++x) for (int k = 0; k < 3; ++k) { float v = f.c[k].row(y)[x]; v = v < 0 ? 0 : v > 1 ? 1 : v; std::fputc((int)(v * 255 + .5f), o); }
    std::fclose(o);
}
int main(int argc, char** argv) {
    int W = argc > 1 ? std::atoi(argv[1]) : 960, H = argc > 2 ? std::atoi(argv[2]) : 540;
    int q = argc > 3 ? std::atoi(argv[3]) : 1, inten = argc > 4 ? std::atoi(argv[4]) : 1;
    Frame src = makeTest(W, H); writePPM("out/src.ppm", src);
    for (int i = 0; i < 15; ++i) {
        Frame f = src; Params p; p.look = i; p.quality = q; p.intensity = inten;
        auto t0 = std::chrono::steady_clock::now();
        process(f, p, 25 + i, 1.0 + i * 0.04);
        double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        char fn[64]; std::snprintf(fn, sizeof fn, "out/look%02d.ppm", i); writePPM(fn, f);
        std::printf("look %2d %-26s %7.1f ms\n", i, kLookNames[i], ms);
    }
}
