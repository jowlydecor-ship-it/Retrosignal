#include "Engine.h"
#include "Font.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <thread>

namespace rsc {
namespace {

constexpr float kPi = 3.14159265f;

// ---------------------------------------------------------------- utilitaires
template <class F> void parallelFor(int n, F&& fn) {
    unsigned T = std::max(1u, std::thread::hardware_concurrency());
    if (n < 16) T = 1;
    std::atomic<int> next{0};
    auto worker = [&] { for (;;) { int i = next.fetch_add(1); if (i >= n) break; fn(i); } };
    std::vector<std::thread> th;
    for (unsigned t = 1; t < T; ++t) th.emplace_back(worker);
    worker();
    for (auto& t : th) t.join();
}
inline uint32_t hash32(uint32_t x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; }
inline float u01(uint32_t h) { return (h >> 8) * (1.0f / 16777216.0f); }
inline float gauss(uint32_t s) {   // ~N(0,1)
    return (u01(hash32(s)) + u01(hash32(s ^ 0x9e3779b9u)) + u01(hash32(s + 0x85ebca6bu)) - 1.5f) * 2.f;
}
inline float clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

std::vector<float> kernel(float sigma) {
    if (sigma < 0.3f) return {1.f};
    int r = std::min(48, (int)std::ceil(sigma * 3.f));
    std::vector<float> k(2 * r + 1); float s = 0;
    for (int i = -r; i <= r; ++i) { k[i + r] = std::exp(-0.5f * i * i / (sigma * sigma)); s += k[i + r]; }
    for (auto& v : k) v /= s;
    return k;
}

void shiftRows(Plane& p, const std::vector<float>& s) {
    int w = p.w;
    parallelFor(p.h, [&](int y) {
        float sh = s[y]; if (std::fabs(sh) < 0.05f) return;
        float* r = p.row(y); std::vector<float> t(r, r + w);
        for (int x = 0; x < w; ++x) { float xf = std::clamp(x - sh, 0.f, (float)w - 1.001f); int i = (int)xf; float fr = xf - i; r[x] = t[i] * (1 - fr) + t[i + 1] * fr; }
    });
}
// Gaussienne ~ 3 passes de box blur (O(1) par pixel)
void boxRadii(float sigma, int r[3]) {
    float wi = std::sqrt(12.f * sigma * sigma / 3.f + 1.f); int wl = (int)std::floor(wi); if (wl % 2 == 0) --wl; int wu = wl + 2;
    int m = (int)std::lround((12.f * sigma * sigma - 3.f * wl * wl - 12.f * wl - 9.f) / (-4.f * wl - 4.f));
    for (int i = 0; i < 3; ++i) r[i] = ((i < m ? wl : wu) - 1) / 2;
}
void boxH(Plane& p, int r) {
    if (r < 1) return; int w = p.w; float inv = 1.f / (2 * r + 1);
    parallelFor(p.h, [&](int y) {
        float* row = p.row(y); std::vector<float> pad(w + 2 * r);
        for (int j = 0; j < w + 2 * r; ++j) pad[j] = row[std::clamp(j - r, 0, w - 1)];
        float acc = 0; for (int i = 0; i < 2 * r + 1; ++i) acc += pad[i];
        for (int x = 0; x < w; ++x) { row[x] = acc * inv; acc += pad[x + 2 * r + 1 < w + 2 * r ? x + 2 * r + 1 : w + 2 * r - 1] - pad[x]; }
    });
}
void boxV(Plane& p, int r) {
    if (r < 1) return; int w = p.w, h = p.h; float inv = 1.f / (2 * r + 1); Plane src = p; const int S = 64;
    parallelFor((w + S - 1) / S, [&](int t) {
        int x0 = t * S, x1 = std::min(w, x0 + S), n = x1 - x0; std::vector<float> acc(n, 0.f);
        for (int i = -r; i <= r; ++i) { const float* s = src.row(std::clamp(i, 0, h - 1)) + x0; for (int x = 0; x < n; ++x) acc[x] += s[x]; }
        for (int y = 0; y < h; ++y) {
            float* o = p.row(y) + x0; const float *add = src.row(std::min(y + r + 1, h - 1)) + x0, *sub = src.row(std::max(y - r, 0)) + x0;
            for (int x = 0; x < n; ++x) { o[x] = acc[x] * inv; acc[x] += add[x] - sub[x]; }
        }
    });
}
// Flou horizontal avec décalage fractionnaire optionnel (shift > 0 : le contenu part vers la droite).
void blurH(Plane& p, float sigma, float shift = 0.f) {
    if (std::fabs(shift) > 1e-3f) shiftRows(p, std::vector<float>(p.h, shift));
    if (sigma < 0.3f) return;
    if (sigma >= 2.5f) { int r[3]; boxRadii(sigma, r); for (int i = 0; i < 3; ++i) boxH(p, r[i]); return; }
    auto k = kernel(sigma); int r = (int)k.size() / 2, w = p.w;
    parallelFor(p.h, [&](int y) {
        float* row = p.row(y); std::vector<float> pad(w + 2 * r);
        for (int j = 0; j < w + 2 * r; ++j) pad[j] = row[std::clamp(j - r, 0, w - 1)];
        for (int x = 0; x < w; ++x) { float a = 0; for (size_t i = 0; i < k.size(); ++i) a += k[i] * pad[x + i]; row[x] = a; }
    });
}
void blurV(Plane& p, float sigma) {
    if (sigma < 0.3f) return;
    if (sigma >= 2.5f) { int r[3]; boxRadii(sigma, r); for (int i = 0; i < 3; ++i) boxV(p, r[i]); return; }
    auto k = kernel(sigma); int r = (int)k.size() / 2; Plane src = p; int w = p.w, h = p.h;
    parallelFor(h, [&](int y) {
        float* o = p.row(y); for (int x = 0; x < w; ++x) o[x] = 0;
        for (size_t i = 0; i < k.size(); ++i) {
            const float* s = src.row(std::clamp(y + (int)i - r, 0, h - 1)); float kk = k[i];
            for (int x = 0; x < w; ++x) o[x] += kk * s[x];
        }
    });
}
Plane resize(const Plane& s, int nw, int nh) {
    Plane o(nw, nh);
    parallelFor(nh, [&](int y) {
        float sy = std::clamp((y + 0.5f) * s.h / nh - 0.5f, 0.f, (float)s.h - 1.f);
        int y0 = (int)sy, y1 = std::min(y0 + 1, s.h - 1); float fy = sy - y0;
        const float *a = s.row(y0), *b = s.row(y1); float* d = o.row(y);
        for (int x = 0; x < nw; ++x) {
            float sx = std::clamp((x + 0.5f) * s.w / nw - 0.5f, 0.f, (float)s.w - 1.f);
            int x0 = (int)sx, x1 = std::min(x0 + 1, s.w - 1); float fx = sx - x0;
            d[x] = lerp(lerp(a[x0], a[x1], fx), lerp(b[x0], b[x1], fx), fy);
        }
    });
    return o;
}
void blurChroma(Plane& p, float sh, float sv, float shift, int cd) {
    if (cd > 1) {
        Plane s = resize(p, std::max(8, p.w / cd), std::max(8, p.h / cd));
        blurH(s, sh / cd, shift / cd); blurV(s, sv / cd);
        p = resize(s, p.w, p.h);
    } else { blurH(p, sh, shift); blurV(p, sv); }
}
Plane noisePlane(int w, int h, int ns, uint32_t seed) {
    Plane n(std::max(2, w / ns), std::max(2, h / ns));
    parallelFor(n.h, [&](int y) { float* r = n.row(y); for (int x = 0; x < n.w; ++x) r[x] = gauss((seed + (uint32_t)(y * 73856093u)) ^ (uint32_t)(x * 19349663u)); });
    return ns > 1 ? resize(n, w, h) : n;
}

// ---------------------------------------------------------------- couleur
void rgb2yuv(const Frame& f, Plane& Y, Plane& U, Plane& V) {
    Y = Plane(f.w, f.h); U = Plane(f.w, f.h); V = Plane(f.w, f.h);
    parallelFor(f.h, [&](int y) {
        const float *r = f.c[0].row(y), *g = f.c[1].row(y), *b = f.c[2].row(y);
        float *yy = Y.row(y), *u = U.row(y), *v = V.row(y);
        for (int x = 0; x < f.w; ++x) {
            float l = 0.299f * r[x] + 0.587f * g[x] + 0.114f * b[x];
            yy[x] = l; u[x] = 0.564f * (b[x] - l); v[x] = 0.713f * (r[x] - l);
        }
    });
}
void yuv2rgb(Frame& f, const Plane& Y, const Plane& U, const Plane& V) {
    parallelFor(f.h, [&](int y) {
        const float *yy = Y.row(y), *u = U.row(y), *v = V.row(y);
        float *r = f.c[0].row(y), *g = f.c[1].row(y), *b = f.c[2].row(y);
        for (int x = 0; x < f.w; ++x) {
            r[x] = yy[x] + 1.403f * v[x]; b[x] = yy[x] + 1.773f * u[x];
            g[x] = yy[x] - 0.344136f * u[x] - 0.714136f * v[x];
        }
    });
}

// ---------------------------------------------------------------- DCT / codec
struct DctTab { float c[8][8]; DctTab() { for (int u = 0; u < 8; ++u) for (int j = 0; j < 8; ++j)
    c[u][j] = (u == 0 ? std::sqrt(1.f / 8) : std::sqrt(2.f / 8)) * std::cos((2 * j + 1) * u * kPi / 16.f); } };
const int kQLuma[64] = {16,11,10,16,24,40,51,61, 12,12,14,19,26,58,60,55, 14,13,16,24,40,57,69,56, 14,17,22,29,51,87,80,62,
                        18,22,37,56,68,109,103,77, 24,35,55,64,81,104,113,92, 49,64,78,87,103,121,120,101, 72,92,95,98,112,100,103,99};
int qChroma(int u, int v) {
    static const int t[4][4] = {{17,18,24,47},{18,21,26,66},{24,26,56,99},{47,66,99,99}};
    return (u < 4 && v < 4) ? t[u][v] : 99;
}
void dctQuant(Plane& p, float offset, bool chroma, float scale) {
    static const DctTab T;
    int bw = p.w / 8, bh = p.h / 8, w = p.w;
    parallelFor(bh, [&](int by) {
        float b[8][8], t[8][8], F[8][8];
        for (int bx = 0; bx < bw; ++bx) {
            for (int j = 0; j < 8; ++j) for (int i = 0; i < 8; ++i) b[j][i] = p.d[(size_t)(by * 8 + j) * w + bx * 8 + i] * 255.f - offset;
            for (int u = 0; u < 8; ++u) for (int i = 0; i < 8; ++i) { float a = 0; for (int j = 0; j < 8; ++j) a += T.c[u][j] * b[j][i]; t[u][i] = a; }
            for (int u = 0; u < 8; ++u) for (int v = 0; v < 8; ++v) {
                float a = 0; for (int i = 0; i < 8; ++i) a += t[u][i] * T.c[v][i];
                float q = (chroma ? qChroma(u, v) : kQLuma[u * 8 + v]) * scale; F[u][v] = std::round(a / q) * q;
            }
            for (int j = 0; j < 8; ++j) for (int v = 0; v < 8; ++v) { float a = 0; for (int u = 0; u < 8; ++u) a += T.c[u][j] * F[u][v]; t[j][v] = a; }
            for (int j = 0; j < 8; ++j) for (int i = 0; i < 8; ++i) { float a = 0; for (int v = 0; v < 8; ++v) a += t[j][v] * T.c[v][i];
                p.d[(size_t)(by * 8 + j) * w + bx * 8 + i] = (a + offset) / 255.f; }
        }
    });
}
void codecStage(Plane& Y, Plane& U, Plane& V, const Look& L, bool dct) {
    if (L.codecQ <= 0.f) return;
    int W = Y.w, H = Y.h; float f = std::max(1.f, L.codecDown);
    int w2 = std::max(16, (int)(W / f)), h2 = std::max(16, (int)(H / f));
    w2 = ((w2 + 15) / 16) * 16; h2 = ((h2 + 15) / 16) * 16;
    if (f > 1.01f) { blurH(Y, 0.45f * f); blurV(Y, 0.45f * f); blurH(U, 0.45f * f); blurV(U, 0.45f * f); blurH(V, 0.45f * f); blurV(V, 0.45f * f); }
    Plane y2 = resize(Y, w2, h2), u2 = resize(U, w2 / 2, h2 / 2), v2 = resize(V, w2 / 2, h2 / 2);
    if (dct) { dctQuant(y2, 128.f, false, L.codecQ); dctQuant(u2, 0.f, true, L.codecQ); dctQuant(v2, 0.f, true, L.codecQ); }
    Y = resize(y2, W, H); U = resize(u2, W, H); V = resize(v2, W, H);
}

// ---------------------------------------------------------------- OSD
void blendPx(Frame& f, int x, int y, const float c[3], float a) {
    if (x < 0 || y < 0 || x >= f.w || y >= f.h) return;
    for (int k = 0; k < 3; ++k) { float& v = f.c[k].row(y)[x]; v = lerp(v, c[k], a); }
}
void fillRect(Frame& f, int x, int y, int w, int h, const float c[3], float a = 1.f) {
    for (int j = 0; j < h; ++j) for (int i = 0; i < w; ++i) blendPx(f, x + i, y + j, c, a);
}
int textWidth(const std::string& s, int px) { return (int)s.size() * 6 * px - px; }
void drawText(Frame& f, int x, int y, const std::string& s, int px, const float col[3], bool shadow = true) {
    static const float black[3] = {0, 0, 0};
    for (int pass = shadow ? 0 : 1; pass < 2; ++pass)
        for (size_t i = 0; i < s.size(); ++i) {
            const uint8_t* g = glyph(s[i]);
            for (int r = 0; r < 7; ++r) for (int c = 0; c < 5; ++c) if (g[r] & (0x10 >> c)) {
                int ox = x + ((int)i * 6 + c) * px + (pass == 0 ? px : 0), oy = y + r * px + (pass == 0 ? px : 0);
                fillRect(f, ox, oy, px, px, pass == 0 ? black : col, pass == 0 ? 0.7f : 1.f);
            }
        }
}
void fillDisc(Frame& f, int cx, int cy, int r, const float c[3]) {
    for (int j = -r; j <= r; ++j) for (int i = -r; i <= r; ++i) if (i * i + j * j <= r * r) blendPx(f, cx + i, cy + j, c, 1.f);
}
std::string dateString(const Look& L, int fmt, int Y, int M, int D) {
    static const char* mon[12] = {"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
    char b[48]; M = std::clamp(M, 1, 12);
    switch (fmt) {
        case 0: std::snprintf(b, sizeof b, "%s %02d %04d", mon[M - 1], D, Y); break;
        case 1: std::snprintf(b, sizeof b, "%04d.%02d.%02d", Y, M, D); break;
        case 2: std::snprintf(b, sizeof b, "%02d/%02d/%04d", M, D, Y); break;
        default: std::snprintf(b, sizeof b, "'%02d %02d %02d", Y % 100, M, D); break;
    }
    (void)L; return b;
}
void drawOsd(Frame& f, const Look& L, const Params& P, double t, bool postTape) {
    Osd kind = L.osd;
    if (P.osdMode == 2) return;
    if (P.osdMode == 1 && kind == Osd::None) kind = Osd::DateTime;
    if (kind == Osd::None) return;
    bool isPost = (kind == Osd::Viewfinder);
    if (isPost != postTape) return;

    int W = f.w, H = f.h, px = std::max(1, (int)(H / 180.f)), mx = (int)(W * 0.05f), my = (int)(H * 0.06f), lh = 9 * px;
    int Y = P.customDate ? P.year : L.year, M = P.customDate ? P.month : L.month, D = P.customDate ? P.day : L.day;
    int sod = ((P.customDate ? P.hour * 3600 + P.minute * 60 + P.second : L.hour * 3600 + L.minute * 60) + (int)std::floor(t)) % 86400;
    char tb[32]; std::snprintf(tb, sizeof tb, "%02d:%02d:%02d", sod / 3600, (sod / 60) % 60, sod % 60);
    std::string date = dateString(L, L.dateFmt, Y, M, D), time = tb;
    const float* c = L.osdColor; static const float white[3] = {1, 1, 1}, red[3] = {1.f, .1f, .08f};

    switch (kind) {
    case Osd::DateStamp: {
        std::string s = date; drawText(f, W - mx - textWidth(s, px), H - my - lh, s, px, c); break; }
    case Osd::DateTime: {
        drawText(f, W - mx - textWidth(date, px), H - my - 2 * lh, date, px, c);
        drawText(f, W - mx - textWidth(time, px), H - my - lh, time, px, c); break; }
    case Osd::ClockUnset: {
        if (((int)std::floor(t)) % 2 == 0) { std::string s = "12:00 AM"; drawText(f, W - mx - textWidth(s, px), H - my - lh, s, px, c); }
        break; }
    case Osd::Surveillance: {
        drawText(f, mx, my, L.camLabel, px, c);
        std::string s = date + " " + time; drawText(f, mx, H - my - lh, s, px, c); break; }
    case Osd::NightShot: {
        drawText(f, mx, my, "NIGHTSHOT", px, c);
        drawText(f, W - mx - textWidth(date, px), H - my - lh, date, px, c); break; }
    case Osd::Viewfinder: {
        if (((int)std::floor(t)) % 2 == 0) { fillDisc(f, mx + 3 * px, my + 3 * px, 3 * px, red); drawText(f, mx + 8 * px, my, "REC", px, c); }
        char tc[24]; int s = (int)t; std::snprintf(tc, sizeof tc, "0:%02d:%02d", (s / 60) % 60, s % 60);
        drawText(f, mx, my + lh * 2, tc, px, c);
        // batterie
        int bw = 16 * px, bh = 7 * px, bx = W - mx - bw - px, by = my;
        fillRect(f, bx, by, bw, px, c); fillRect(f, bx, by + bh - px, bw, px, c);
        fillRect(f, bx, by, px, bh, c); fillRect(f, bx + bw - px, by, px, bh, c); fillRect(f, bx + bw, by + 2 * px, px, 3 * px, c);
        for (int i = 0; i < 2; ++i) fillRect(f, bx + 2 * px + i * 5 * px, by + 2 * px, 4 * px, bh - 4 * px, c);
        drawText(f, mx, H - my - lh, "SP", px, c);
        // barre de zoom
        int zw = (int)(W * 0.18f), zx = (W - zw) / 2, zy = H - my - lh;
        drawText(f, zx - 8 * px, zy, "W", px, c); drawText(f, zx + zw + 3 * px, zy, "T", px, c);
        fillRect(f, zx, zy + 3 * px, zw, px, c);
        fillRect(f, zx + (int)(zw * (0.25f + 0.2f * std::sin(t * 0.3f))), zy + px, 2 * px, 5 * px, c);
        drawText(f, W - mx - textWidth(date, px), H - my - lh, date, px, c); break; }
    default: break;
    }
    (void)white;
}

// ---------------------------------------------------------------- étages
void sensorStage(Frame& f, const Look& L, const Params& P, float sc, float g) {
    int W = f.w, H = f.h; float sat = L.satMul * P.saturation;
    const float gl = std::min(0.45f, (g - 1.f) * 0.12f);
    parallelFor(H, [&](int y) {
        float* ch[3] = {f.c[0].row(y), f.c[1].row(y), f.c[2].row(y)};
        for (int x = 0; x < W; ++x) {
            float r = ch[0][x], gg = ch[1][x], b = ch[2][x];
            // crosstalk CCD (réponse spectrale imparfaite)
            float r2 = r * .94f + gg * .05f + b * .01f, g2 = gg * .92f + r * .04f + b * .04f, b2 = b * .93f + gg * .06f + r * .01f;
            float l = 0.299f * r2 + 0.587f * g2 + 0.114f * b2;
            if (L.nightVision) { // intensificateur : monochrome, gain fort, teinte verte
                float v = l * (1.f + (g - 1.f) * 0.1f); v = v / (1.f + 0.5f * v) * 1.1f;
                r2 = v * L.tint[0]; g2 = v * L.tint[1]; b2 = v * L.tint[2];
            } else {
                r2 = (l + (r2 - l) * sat) * L.tint[0]; g2 = (l + (g2 - l) * sat) * L.tint[1]; b2 = (l + (b2 - l) * sat) * L.tint[2];
                // gain AGC : relève les ombres
                r2 += r2 * gl * (1.f - clamp01(r2)); g2 += g2 * gl * (1.f - clamp01(g2)); b2 += b2 * gl * (1.f - clamp01(b2));
            }
            ch[0][x] = r2; ch[1][x] = g2; ch[2][x] = b2;
        }
    });
    // flou optique
    float ls = L.lumaBlur * 0.5f * sc;
    for (int k = 0; k < 3; ++k) { blurH(f.c[k], ls); blurV(f.c[k], ls * 0.8f); }
    // bloom des hautes lumières
    if (L.bloom > 0.01f) {
        Plane hl(W, H);
        parallelFor(H, [&](int y) { float* o = hl.row(y); for (int x = 0; x < W; ++x)
            o[x] = std::max(0.f, std::max(f.c[0].row(y)[x], std::max(f.c[1].row(y)[x], f.c[2].row(y)[x])) - 0.65f); });
        Plane s = resize(hl, std::max(8, W / 4), std::max(8, H / 4));
        blurH(s, 7.f * sc / 4.f); blurV(s, 7.f * sc / 4.f);
        hl = resize(s, W, H);
        float bl = L.bloom;
        parallelFor(H, [&](int y) { const float* h = hl.row(y); for (int k = 0; k < 3; ++k) { float* o = f.c[k].row(y);
            for (int x = 0; x < W; ++x) o[x] += h[x] * bl * 0.7f * (L.nightVision ? L.tint[k] : 1.f); } });
    }
    // smear vertical CCD
    if (L.smear > 0.01f) {
        std::vector<float> col(W, 0.f);
        for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
            float l = 0.33f * (f.c[0].row(y)[x] + f.c[1].row(y)[x] + f.c[2].row(y)[x]); if (l > 0.9f) col[x] += (l - 0.9f);
        }
        std::vector<float> sm(W); for (int x = 0; x < W; ++x) { float a = 0; int n = 0; for (int i = -2; i <= 2; ++i) if (x + i >= 0 && x + i < W) { a += col[x + i]; ++n; } sm[x] = a / n / H * L.smear * 1.2f; }
        parallelFor(H, [&](int y) { for (int k = 0; k < 3; ++k) { float* o = f.c[k].row(y); for (int x = 0; x < W; ++x) o[x] += sm[x] * (L.nightVision ? L.tint[k] : 1.f); } });
    }
    // saturation de gain : compression douce des hautes lumières, contraste, noirs relevés
    float kn = L.knee, ik = 1.f - kn;
    parallelFor(H, [&](int y) { for (int k = 0; k < 3; ++k) { float* o = f.c[k].row(y); for (int x = 0; x < W; ++x) {
        float v = o[x]; if (v > kn) v = kn + ik * std::tanh((v - kn) / ik);
        v = (v - 0.5f) * L.contrast + 0.5f; o[x] = L.blackLift + (1.f - L.blackLift) * v; } } });
}

void electronicsStage(Frame& f, const Look& L, float sc, int frame, double t, uint32_t seed) {
    int W = f.w, H = f.h;
    // instabilité de luminance : pompage AGC global + bandes de scintillement
    float gl = 1.f + L.flicker * (0.8f * std::sin((float)t * 2.3f) + 0.8f * gauss(seed ^ 0x1234u));
    float ph = u01(hash32(seed ^ 0x777u)) * 6.28f;
    parallelFor(H, [&](int y) { float m = gl * (1.f + L.flicker * 0.5f * std::sin(y * 0.02f / sc * 2.f + ph + (float)t * 5.f));
        for (int k = 0; k < 3; ++k) { float* o = f.c[k].row(y); for (int x = 0; x < W; ++x) o[x] *= m; } });
    // renforcement de contours horizontal (halo analogique)
    if (L.sharpen > 0.01f) for (int k = 0; k < 3; ++k) {
        Plane b = f.c[k]; blurH(b, 0.9f * sc);
        parallelFor(H, [&](int y) { float* o = f.c[k].row(y); const float* s = b.row(y); for (int x = 0; x < W; ++x) o[x] += L.sharpen * (o[x] - s[x]); });
    }
    (void)frame;
}

struct Defects { std::vector<float> shift, band; };
Defects makeDefects(const Look& L, int /*W*/, int H, float sc, int frame, double t, const Params& P, uint32_t seed) {
    Defects d; d.shift.assign(H, 0.f); d.band.assign(H, 0.f);
    float w = 0.f, jit = L.jitter * sc * (1.f + 0.4f * (L.generations - 1));
    for (int y = 0; y < H; ++y) {
        w = w * 0.85f + gauss(seed ^ (uint32_t)(y * 0x9E3779B1u)) * 0.53f;
        d.shift[y] = w * jit + jit * 0.6f * std::sin(y * 0.004f + (float)t * 3.f);
    }
    int y0 = H - (int)(14 * sc);
    for (int y = std::max(0, y0); y < H; ++y) { float tt = (y - y0) / (float)(H - y0);
        d.shift[y] += L.headSwitch * (tt * tt * 20.f * sc + gauss(seed + y * 31u) * 3.f * sc * tt); }
    uint32_t epoch = (uint32_t)(frame / 48);
    if (L.tracking > 0.01f && u01(hash32(epoch * 31u + P.seed * 7u)) < std::min(0.9f, L.tracking * 0.8f)) {
        float c = ((float)std::fmod(t * 0.11 + u01(hash32(epoch + 99u)), 1.0) * 1.2f - 0.1f) * H, half = (4.f + 16.f * std::min(L.tracking, 1.5f)) * sc;
        for (int y = 0; y < H; ++y) { float wgt = std::max(0.f, 1.f - std::fabs(y - c) / half); d.band[y] = wgt;
            d.shift[y] += wgt * (std::sin(y * 0.8f + frame) * L.tracking * 8.f * sc + gauss(seed + y * 17u) * 2.f * sc); }
    }
    return d;
}
void tapeDefects(Plane& Y, Plane& U, Plane& V, const Look& L, const Defects& d, float sc, uint32_t seed) {
    int W = Y.w, H = Y.h;
    // bandes de tracking : gain de luma, bruit, perte de chroma
    // + bruit de commutation de têtes en bas d'image
    int y0 = H - (int)(14 * sc);
    parallelFor(H, [&](int y) {
        float b = d.band[y], hs = (y >= y0 && L.headSwitch > 0.f) ? (y - y0) / (float)(H - y0) * L.headSwitch : 0.f;
        float* yy = Y.row(y); float* u = U.row(y); float* v = V.row(y);
        if (b > 0.f || hs > 0.f) for (int x = 0; x < W; ++x) {
            float n = gauss(seed ^ (uint32_t)(y * 2654435761u) ^ (uint32_t)(x * 40503u));
            yy[x] += b * (0.12f + 0.25f * n) + hs * 0.22f * n; u[x] *= (1.f - 0.6f * b); v[x] *= (1.f - 0.6f * b);
        }
    });
    // stries parasites + décalage statique de ligne
    for (int y = 0; y < H; ++y) {
        uint32_t s = seed ^ (uint32_t)(y * 0x85ebca6bu);
        Y.row(y)[0] += 0.f;
        float off = gauss(s) * L.streaks * 0.008f; for (int x = 0; x < W; ++x) Y.row(y)[x] += off;
        if (u01(hash32(s + 5u)) < L.streaks * 0.01f / std::max(1.f, sc * 0.5f)) {
            int len = (int)((60 + u01(hash32(s + 6u)) * 300) * sc), x0 = (int)(u01(hash32(s + 7u)) * W); float amp = 0.08f + u01(hash32(s + 8u)) * 0.25f;
            int th = std::max(1, (int)(sc * (1.f + u01(hash32(s + 9u)) * 1.5f)));
            for (int j = 0; j < th && y + j < H; ++j) for (int i = 0; i < len && x0 + i < W; ++i) {
                float e = std::min(1.f, std::min(i, len - i) / (8.f * sc));
                Y.row(y + j)[x0 + i] += amp * e * gauss(s + (uint32_t)(i * 97u)); }
        }
    }
    // dropouts : traits blancs, perte de chroma
    int n = (int)(L.dropouts * 6.f + u01(hash32(seed + 3u)));
    for (int k = 0; k < n; ++k) {
        uint32_t s = hash32(seed + 1000u + k * 17u);
        int y = (int)(u01(s) * H), x0 = (int)(u01(hash32(s + 1u)) * W), len = (int)((10 + u01(hash32(s + 2u)) * 70) * sc), th = std::max(1, (int)sc);
        for (int j = 0; j < th && y + j < H; ++j) for (int i = 0; i < len && x0 + i < W; ++i) {
            float a = std::min(1.f, std::min(i, len - i) / (3.f * sc)) * 0.9f;
            float& yy = Y.row(y + j)[x0 + i]; yy = lerp(yy, 0.97f, a); U.row(y + j)[x0 + i] *= (1.f - a); V.row(y + j)[x0 + i] *= (1.f - a);
        }
    }
}

void addNoise(Plane& Y, Plane& U, Plane& V, float sL, float sC, int ns, uint32_t seed) {
    int W = Y.w, H = Y.h;
    if (sL > 1e-4f) { Plane n = noisePlane(W, H, ns, seed);
        parallelFor(H, [&](int y) { float* o = Y.row(y); const float* q = n.row(y); for (int x = 0; x < W; ++x) o[x] += sL * (1.3f - 0.8f * clamp01(o[x])) * q[x]; }); }
    if (sC > 1e-4f) { Plane nu = noisePlane(W, H, ns * 3, seed + 11u), nv = noisePlane(W, H, ns * 3, seed + 23u);
        parallelFor(H, [&](int y) { float *u = U.row(y), *v = V.row(y); const float *a = nu.row(y), *b = nv.row(y);
            for (int x = 0; x < W; ++x) { u[x] += sC * a[x]; v[x] += sC * b[x]; } }); }
}

void displayStage(Frame& f, const Look& L, int frame, float sc, bool best) {
    int W = f.w, H = f.h; float cx = W * 0.5f, cy = H * 0.5f, inv = 1.f / (cx * cx + cy * cy);
    int cell = std::max(2, (int)std::lround(sc * 2.f));
    static const int bayer[4][4] = {{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}};
    parallelFor(H, [&](int y) {
        float line = y / sc; int li = (int)std::floor(line); float fr = line - li;
        float rowMul = 1.f;
        if (L.display == Disp::CRT) {
            rowMul = 1.f - 0.13f * (0.5f - 0.5f * std::cos(2.f * kPi * fr));
            if (L.interlace && (((li + frame) & 1) == 1)) rowMul *= 0.94f;   // scintillement inter-trame
        }
        for (int x = 0; x < W; ++x) {
            float dx = x - cx, dy = y - cy, vg = 1.f - L.vignette * (dx * dx + dy * dy) * inv * 1.6f; vg = std::max(0.f, vg);
            for (int k = 0; k < 3; ++k) {
                float v = f.c[k].row(y)[x] * vg;
                if (L.display == Disp::CRT) { v *= rowMul; if (best) v *= (x % 3 == k) ? 1.04f : 0.94f; v = clamp01(v); v = std::pow(v, 0.95f); }
                else if (L.display == Disp::LCD) {
                    v = clamp01((v - 0.5f) * 1.08f + 0.5f) * (k == 0 ? 0.97f : k == 2 ? 1.04f : 1.f);
                    if (x % cell == 0 || y % cell == 0) v *= 0.93f;
                    float dth = (bayer[y & 3][x & 3] + 0.5f) / 16.f - 0.5f; v = std::floor(clamp01(v) * 63.f + 0.5f + dth) / 63.f;
                }
                f.c[k].row(y)[x] = clamp01(v);
            }
        }
    });
}

void curvatureStage(Frame& f, float k) {
    Frame src = f; int W = f.w, H = f.h;
    parallelFor(H, [&](int y) {
        float v = (y + 0.5f) / H * 2.f - 1.f;
        for (int x = 0; x < W; ++x) {
            float u = (x + 0.5f) / W * 2.f - 1.f, r2 = u * u + v * v, m = 1.f + k * r2;
            float sx = (u * m * 0.5f + 0.5f) * W - 0.5f, sy = (v * m * 0.5f + 0.5f) * H - 0.5f;
            float edge = std::min(std::min(sx + 0.5f, W - 0.5f - sx), std::min(sy + 0.5f, H - 0.5f - sy));
            float a = clamp01(edge / 2.f);
            int x0 = std::clamp((int)std::floor(sx), 0, W - 2), y0 = std::clamp((int)std::floor(sy), 0, H - 2);
            float fx = std::clamp(sx - x0, 0.f, 1.f), fy = std::clamp(sy - y0, 0.f, 1.f);
            for (int c = 0; c < 3; ++c) { const float *r0 = src.c[c].row(y0), *r1 = src.c[c].row(y0 + 1);
                f.c[c].row(y)[x] = a * lerp(lerp(r0[x0], r0[x0 + 1], fx), lerp(r1[x0], r1[x0 + 1], fx), fy); }
        }
    });
}

} // namespace

// ------------------------------------------------------------------ point d'entrée
static void processImpl(Frame& f, const Params& P, int frame, double t) {
    Look L = makeLook(P.look, P.intensity);
    int W = f.w, H = f.h; if (W < 32 || H < 32) return;
    float sc = H / 480.f; bool fast = P.quality == 0, best = P.quality == 2;
    int cd = fast ? 4 : best ? 1 : 2;
    uint32_t seed = hash32((uint32_t)frame * 7919u + (uint32_t)P.seed * 104729u + 12345u);

    L.jitter *= P.damage; L.dropouts *= P.damage; L.tracking *= P.damage; L.headSwitch *= P.damage;
    L.streaks *= P.tapeNoise;
    float gdb = std::clamp(P.gainOverride ? P.gainDB : L.gainDB, 0.f, 48.f), g = std::pow(10.f, gdb / 20.f);
    if (fast) L.generations = 1; else if (!best) L.generations = std::min(L.generations, 2);

    // 1. Lentille & capteur
    sensorStage(f, L, P, sc, g);
    // 2. Électronique de la caméra
    electronicsStage(f, L, sc, frame, t, seed);
    drawOsd(f, L, P, t, false);                      // incrustation date : gravée AVANT la bande
    Plane Y, U, V; rgb2yuv(f, Y, U, V);
    int ns = std::max(1, (int)std::lround(sc * 0.9f));
    addNoise(Y, U, V, L.noiseLuma * 0.5f * std::sqrt(g), L.noiseChroma * 0.5f * std::sqrt(g), ns, seed + 1u);   // bruit capteur/gain max

    // 3. Bande / codec
    codecStage(Y, U, V, L, !fast);
    Defects d = makeDefects(L, W, H, sc, frame, t, P, seed);
    for (int gen = 0; gen < L.generations; ++gen) {
        blurH(Y, L.lumaBlur * sc);
        blurChroma(U, L.chromaBlur * sc, L.chromaBlur * 0.35f * sc, L.chromaShiftX * sc, cd);
        blurChroma(V, L.chromaBlur * sc, L.chromaBlur * 0.35f * sc, L.chromaShiftX * sc, cd);
        shiftRows(Y, d.shift); shiftRows(U, d.shift); shiftRows(V, d.shift);
        addNoise(Y, U, V, L.noiseLuma * 0.8f * P.tapeNoise, L.noiseChroma * 0.8f * P.tapeNoise, ns, seed + 100u + gen * 13u);
    }
    tapeDefects(Y, U, V, L, d, sc, seed + 777u);
    yuv2rgb(f, Y, U, V);

    // 4. Lecture & affichage
    drawOsd(f, L, P, t, true);                       // OSD de visière : par-dessus le signal
    displayStage(f, L, frame, sc, best);
    if (L.curvature > 0.f) curvatureStage(f, L.curvature);
}

void process(Frame& f, const Params& P, int frame, double t) {
    if (P.quality == 0 && f.h >= 360) {   // Fast : pipeline complet à demi-résolution
        Frame s; s.w = f.w / 2; s.h = f.h / 2;
        for (int k = 0; k < 3; ++k) s.c[k] = resize(f.c[k], s.w, s.h);
        processImpl(s, P, frame, t);
        for (int k = 0; k < 3; ++k) f.c[k] = resize(s.c[k], f.w, f.h);
    } else processImpl(f, P, frame, t);
}

} // namespace rsc
