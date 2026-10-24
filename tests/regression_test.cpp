// Tests de non-régression du moteur : déterminisme, finitude, monotonie, effets des paramètres.
#include "../src/Engine.h"
#include <cstdio>
#include <cmath>
#include <chrono>
using namespace rsc;
static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { std::printf("FAIL: %s\n", msg); ++fails; } else std::printf("ok   : %s\n", msg); } while (0)
static Frame grad(int W, int H) { Frame f; f.w = W; f.h = H; for (auto& c : f.c) c = Plane(W, H);
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) { f.c[0].row(y)[x] = .2f + .6f * x / W; f.c[1].row(y)[x] = .5f; f.c[2].row(y)[x] = .8f - .6f * y / H; } return f; }
static double diff(const Frame& a, const Frame& b) { double s = 0; for (int k = 0; k < 3; ++k) for (size_t i = 0; i < a.c[k].d.size(); ++i) s += std::fabs(a.c[k].d[i] - b.c[k].d[i]); return s / (3.0 * a.c[0].d.size()); }
static double hfNoise(const Frame& f) { double s = 0; long n = 0; for (int y = 0; y < f.h; ++y) for (int x = 1; x < f.w; ++x) { s += std::fabs(f.c[1].row(y)[x] - f.c[1].row(y)[x - 1]); ++n; } return s / n; }
int main() {
    const int W = 640, H = 360; Frame src = grad(W, H);
    bool finite = true;
    for (int q = 0; q < 3; ++q) for (int i = 0; i < 15; ++i) for (int it = 0; it < 3; ++it) {
        Frame f = src; Params p; p.look = i; p.quality = q; p.intensity = it; process(f, p, 3, 0.1);
        for (int k = 0; k < 3; ++k) for (float v : f.c[k].d) if (!std::isfinite(v) || v < -1e-4f || v > 1.0001f) finite = false; }
    CHECK(finite, "45 looks x 3 qualites : valeurs finies et dans [0,1]");
    { Frame a = src, b = src; Params p; p.look = 0; process(a, p, 7, .3); process(b, p, 7, .3); CHECK(diff(a, b) == 0.0, "deterministe pour une meme image"); }
    { Frame a = src, b = src; Params p; p.look = 0; process(a, p, 7, .3); process(b, p, 8, .3); CHECK(diff(a, b) > 1e-4, "le bruit varie d'une image a l'autre"); }
    { double prev = -1; bool mono = true; for (int it = 0; it < 3; ++it) { Frame f = src; Params p; p.look = 10; p.intensity = it; process(f, p, 2, 0); double n = hfNoise(f); if (n < prev) mono = false; prev = n; }
      CHECK(mono, "bruit croissant Light < Standard < Heavy"); }
    { Frame a = src, b = src; Params p; p.look = 4; p.gainOverride = true; p.gainDB = 0; process(a, p, 2, 0); p.gainDB = 30; process(b, p, 2, 0);
      CHECK(hfNoise(b) > hfNoise(a) * 1.2, "Max Gain (dB) augmente le bruit de luminance"); }
    { Frame a = src, b = src; Params p; p.look = 5; p.tapeNoise = 0; p.damage = 0; process(a, p, 2, 0); p.damage = 2; p.tapeNoise = 2; process(b, p, 2, 0); CHECK(diff(a, b) > 1e-3, "Damage/Tape Noise modifient l'image"); }
    { Frame a = src, b = src; Params p; p.look = 3; p.saturation = 0; process(a, p, 2, 0);
      double chroma = 0; for (size_t i = 0; i < a.c[0].d.size(); ++i) chroma += std::fabs(a.c[0].d[i] - a.c[2].d[i]); chroma /= a.c[0].d.size();
      p.saturation = 1; process(b, p, 2, 0); double chroma2 = 0; for (size_t i = 0; i < b.c[0].d.size(); ++i) chroma2 += std::fabs(b.c[0].d[i] - b.c[2].d[i]); chroma2 /= b.c[0].d.size();
      CHECK(chroma < chroma2, "Saturation 0 reduit la chroma"); }
    { Frame a = src, b = src; Params p; p.look = 0; p.osdMode = 2; process(a, p, 2, 0); p.osdMode = 0; process(b, p, 2, 0); CHECK(diff(a, b) > 1e-5, "Overlay Off/Auto differents"); }
    { Frame big = grad(1920, 1080); for (int q = 0; q < 3; ++q) { Frame f = big; Params p; p.look = 0; p.quality = q; auto t0 = std::chrono::steady_clock::now(); process(f, p, 1, 0);
      std::printf("perf 1080p quality %d : %.0f ms\n", q, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); } }
    std::printf(fails ? "%d ECHEC(S)\n" : "TOUS LES TESTS PASSENT\n", fails); return fails ? 1 : 0;
}
