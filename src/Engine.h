#pragma once
// Moteur de traitement : aucune dépendance OpenFX. Chaîne de signal :
// Lentille/Capteur -> Électronique -> Bande/Codec -> Lecture/Affichage.
#include <vector>
#include "Looks.h"

namespace rsc {

struct Plane {
    int w = 0, h = 0; std::vector<float> d;
    Plane() = default;
    Plane(int W, int H, float v = 0.f) : w(W), h(H), d((size_t)W * H, v) {}
    float*       row(int y)       { return d.data() + (size_t)y * w; }
    const float* row(int y) const { return d.data() + (size_t)y * w; }
};

// Image RGB planaire, ligne 0 = HAUT de l'image, valeurs flottantes (0..1 nominal).
struct Frame { int w = 0, h = 0; Plane c[3]; };

struct Params {
    int   look = 0, intensity = 1, quality = 1;   // intensity: 0 Light 1 Standard 2 Heavy ; quality: 0 Fast 1 Balanced 2 Best
    float damage = 1.f, tapeNoise = 1.f;          // multiplicateurs (1 = valeur du look)
    bool  gainOverride = false; float gainDB = 12.f; // Max Gain (dB) absolu, utilisé si gainOverride
    float saturation = 1.f;                       // multiplicateur de saturation
    int   osdMode = 0;                            // 0 auto (look), 1 forcé ON, 2 OFF
    bool  customDate = false;
    int   year = 2003, month = 10, day = 22, hour = 12, minute = 0, second = 0;
    int   seed = 0;
};

// Traite l'image sur place (sans mix). frameIndex/timeSec pilotent toutes les variations temporelles.
void process(Frame& f, const Params& p, int frameIndex, double timeSec);

} // namespace rsc
