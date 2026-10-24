#pragma once
// Définition des 15 looks + 3 intensités. Toutes les valeurs "spatiales" sont exprimées
// en pixels pour une image de 480 lignes ; le moteur les met à l'échelle (H/480).
#include <algorithm>
#include <cmath>
namespace rsc {

enum class Osd  { None, DateStamp, DateTime, ClockUnset, Viewfinder, Surveillance, NightShot };
enum class Disp { None, CRT, LCD };

struct Look {
    // Capteur / optique
    float satMul = 1.f, contrast = 1.f, blackLift = 0.02f, knee = 0.85f;
    float bloom = 0.1f, smear = 0.f, gainDB = 0.f, tint[3] = {1, 1, 1};
    bool  nightVision = false;
    // Électronique
    float sharpen = 0.2f, flicker = 0.01f, noiseLuma = 0.01f, noiseChroma = 0.01f;
    // Bande
    float lumaBlur = 0.6f, chromaBlur = 2.f, chromaShiftX = 0.f;
    float jitter = 0.3f, dropouts = 0.f, streaks = 0.f, tracking = 0.f, headSwitch = 0.f;
    int   generations = 1;
    // Codec
    float codecQ = 0.f, codecDown = 1.f;
    // Affichage
    bool  interlace = false; Disp display = Disp::None; float vignette = 0.1f; float curvature = 0.f;
    // OSD
    Osd osd = Osd::None; float osdColor[3] = {1, 1, 1}; int dateFmt = 0;
    int year = 1995, month = 1, day = 1, hour = 12, minute = 0;
    const char* camLabel = "CAM 02";
};

static const char* const kLookNames[15] = {
    "Home Video '89 (VHS-C)", "Birthday Party '93", "Clock Never Set '95", "Travel Tape '98",
    "School Play '02", "Copy of a Copy", "Field Report '91", "DV Tape '03", "Viewfinder '03",
    "Digicam Movie '03", "Bedroom Webcam '06", "Pocket Cam '09", "Corner Store '97",
    "Night Vision '99", "Night Vision LCD"};

inline Look makeLook(int idx, int intensity) {
    Look L;
    auto col = [&](float r, float g, float b) { L.osdColor[0]=r; L.osdColor[1]=g; L.osdColor[2]=b; };
    auto tint = [&](float r, float g, float b) { L.tint[0]=r; L.tint[1]=g; L.tint[2]=b; };
    auto date = [&](int y,int m,int d,int hh,int mm){ L.year=y; L.month=m; L.day=d; L.hour=hh; L.minute=mm; };
    switch (idx) {
    case 0: // Home Video '89 (VHS-C)
        L.lumaBlur=1.5f; L.chromaBlur=5.f; L.chromaShiftX=2.5f; L.sharpen=.5f; L.noiseLuma=.03f; L.noiseChroma=.03f;
        L.jitter=1.3f; L.dropouts=.6f; L.streaks=.4f; L.tracking=.35f; L.headSwitch=.7f; L.flicker=.03f;
        L.satMul=.85f; L.contrast=.95f; L.blackLift=.05f; L.bloom=.3f; L.smear=.1f; tint(1.04f,1.f,.93f);
        L.interlace=true; L.display=Disp::CRT; L.vignette=.25f;
        L.osd=Osd::DateStamp; col(1.f,.55f,.12f); L.dateFmt=0; date(1989,12,24,15,32); break;
    case 1: // Birthday Party '93 (Hi8, exposition auto instable)
        L.lumaBlur=1.0f; L.chromaBlur=3.5f; L.chromaShiftX=1.5f; L.sharpen=.4f; L.noiseLuma=.02f; L.noiseChroma=.02f;
        L.jitter=.8f; L.dropouts=.3f; L.streaks=.2f; L.tracking=.15f; L.headSwitch=.4f; L.flicker=.05f;
        L.satMul=1.2f; L.contrast=1.05f; L.bloom=.35f; L.smear=.15f; tint(1.05f,1.f,.92f);
        L.interlace=true; L.display=Disp::CRT; L.vignette=.2f;
        L.osd=Osd::DateStamp; col(1.f,.6f,.1f); L.dateFmt=2; date(1993,6,12,16,5); break;
    case 2: // Clock Never Set '95 (horloge 12:00 clignotante)
        L.lumaBlur=1.2f; L.chromaBlur=4.f; L.chromaShiftX=2.f; L.sharpen=.45f; L.noiseLuma=.025f; L.noiseChroma=.025f;
        L.jitter=1.0f; L.dropouts=.4f; L.streaks=.3f; L.tracking=.25f; L.headSwitch=.5f; L.flicker=.03f;
        L.satMul=.95f; L.bloom=.25f; L.smear=.08f; tint(1.02f,1.f,.96f);
        L.interlace=true; L.display=Disp::CRT; L.vignette=.2f;
        L.osd=Osd::ClockUnset; col(1.f,1.f,1.f); break;
    case 3: // Travel Tape '98 (Digital8 / Hi8)
        L.lumaBlur=.9f; L.chromaBlur=2.5f; L.chromaShiftX=1.f; L.sharpen=.45f; L.noiseLuma=.015f; L.noiseChroma=.012f;
        L.jitter=.4f; L.dropouts=.1f; L.streaks=.1f; L.flicker=.025f; L.satMul=1.1f; L.contrast=1.05f; L.bloom=.2f;
        L.codecQ=.8f; L.interlace=true; L.display=Disp::CRT; L.vignette=.15f;
        L.osd=Osd::DateStamp; col(1.f,.65f,.15f); L.dateFmt=1; date(1998,7,14,10,41); break;
    case 4: // School Play '02 (MiniDV, salle sombre, gain élevé)
        L.lumaBlur=.8f; L.chromaBlur=2.f; L.sharpen=.5f; L.noiseLuma=.03f; L.noiseChroma=.03f; L.gainDB=12.f;
        L.jitter=.1f; L.flicker=.04f; L.satMul=.9f; L.contrast=1.08f; L.bloom=.4f; L.smear=.25f; tint(1.05f,1.f,.9f);
        L.codecQ=1.4f; L.interlace=true; L.vignette=.3f;
        L.osd=Osd::DateTime; col(1.f,1.f,1.f); L.dateFmt=0; date(2002,5,17,19,48); break;
    case 5: // Copy of a Copy (dégradation par générations)
        L.lumaBlur=1.4f; L.chromaBlur=6.f; L.chromaShiftX=3.5f; L.sharpen=.7f; L.noiseLuma=.04f; L.noiseChroma=.04f;
        L.jitter=2.f; L.dropouts=1.2f; L.streaks=.8f; L.tracking=.5f; L.headSwitch=1.f; L.flicker=.05f; L.generations=3;
        L.satMul=.65f; L.contrast=1.12f; L.blackLift=.07f; L.bloom=.3f; L.smear=.1f; tint(1.03f,1.f,.95f);
        L.interlace=true; L.display=Disp::CRT; L.vignette=.3f; break;
    case 6: // Field Report '91 (caméra d'épaule / reportage)
        L.lumaBlur=.8f; L.chromaBlur=3.f; L.chromaShiftX=1.f; L.sharpen=.9f; L.noiseLuma=.018f; L.noiseChroma=.015f;
        L.jitter=.4f; L.dropouts=.15f; L.streaks=.15f; L.tracking=.1f; L.headSwitch=.3f; L.flicker=.02f;
        L.contrast=1.12f; L.satMul=1.0f; L.bloom=.15f; L.smear=.2f; tint(.98f,1.f,1.03f);
        L.interlace=true; L.display=Disp::CRT; L.vignette=.2f;
        L.osd=Osd::DateTime; col(1.f,1.f,1.f); L.dateFmt=2; date(1991,3,2,8,15); break;
    case 7: // DV Tape '03
        L.lumaBlur=.6f; L.chromaBlur=1.8f; L.sharpen=.45f; L.noiseLuma=.015f; L.noiseChroma=.012f;
        L.jitter=.05f; L.dropouts=.05f; L.flicker=.015f; L.satMul=1.1f; L.contrast=1.05f; L.bloom=.2f; L.smear=.12f;
        L.codecQ=2.f; L.interlace=true; L.vignette=.12f;
        L.osd=Osd::DateTime; col(1.f,1.f,1.f); L.dateFmt=2; date(2003,8,9,14,22); break;
    case 8: // Viewfinder '03 (écran LCD de visée)
        L.lumaBlur=.9f; L.chromaBlur=2.2f; L.sharpen=.4f; L.noiseLuma=.018f; L.noiseChroma=.015f;
        L.flicker=.02f; L.satMul=1.1f; L.contrast=1.06f; L.bloom=.2f; L.codecQ=1.5f; L.interlace=true;
        L.display=Disp::LCD; L.vignette=.12f; L.osd=Osd::Viewfinder; col(1.f,1.f,1.f);
        L.dateFmt=2; date(2003,10,22,14,5); break;
    case 9: // Digicam Movie '03 (320x240 MJPEG)
        L.lumaBlur=1.2f; L.chromaBlur=3.f; L.sharpen=.7f; L.noiseLuma=.03f; L.noiseChroma=.03f;
        L.flicker=.03f; L.satMul=1.25f; L.contrast=1.1f; L.bloom=.15f; L.smear=.15f;
        L.codecQ=3.f; L.codecDown=2.2f; L.vignette=.2f;
        L.osd=Osd::DateStamp; col(1.f,.7f,.15f); L.dateFmt=3; date(2003,10,22,14,5); break;
    case 10: // Bedroom Webcam '06
        L.lumaBlur=1.1f; L.chromaBlur=3.f; L.sharpen=.6f; L.noiseLuma=.05f; L.noiseChroma=.045f; L.gainDB=14.f;
        L.flicker=.07f; L.satMul=.85f; L.contrast=1.08f; L.bloom=.2f; L.smear=.05f; tint(.94f,1.02f,1.06f);
        L.codecQ=3.5f; L.codecDown=2.5f; L.vignette=.25f; break;
    case 11: // Pocket Cam '09 (3GP)
        L.lumaBlur=1.4f; L.chromaBlur=4.5f; L.sharpen=.8f; L.noiseLuma=.03f; L.noiseChroma=.035f;
        L.flicker=.04f; L.satMul=1.1f; L.contrast=1.1f; L.bloom=.12f; L.smear=.1f;
        L.codecQ=5.5f; L.codecDown=3.f; L.vignette=.18f; break;
    case 12: // Corner Store '97 (vidéosurveillance, bande accélérée)
        L.lumaBlur=1.4f; L.chromaBlur=5.f; L.sharpen=.4f; L.noiseLuma=.035f; L.noiseChroma=.01f;
        L.jitter=1.f; L.dropouts=.8f; L.streaks=.7f; L.tracking=.6f; L.headSwitch=.6f; L.flicker=.06f;
        L.satMul=.15f; L.contrast=1.1f; L.blackLift=.05f; L.bloom=.15f; tint(.96f,1.f,.97f);
        L.codecQ=1.5f; L.codecDown=1.6f; L.interlace=true; L.display=Disp::CRT; L.vignette=.3f;
        L.osd=Osd::Surveillance; col(1.f,1.f,1.f); L.dateFmt=2; date(1997,11,3,23,14); break;
    case 13: // Night Vision '99 (NightShot sur CRT)
        L.nightVision=true; tint(.3f,1.f,.35f); L.gainDB=18.f;
        L.lumaBlur=1.3f; L.chromaBlur=3.f; L.sharpen=.5f; L.noiseLuma=.07f; L.noiseChroma=.004f;
        L.jitter=.8f; L.dropouts=.3f; L.streaks=.3f; L.tracking=.2f; L.headSwitch=.4f; L.flicker=.05f;
        L.contrast=1.15f; L.blackLift=.03f; L.bloom=.6f; L.smear=.3f;
        L.interlace=true; L.display=Disp::CRT; L.vignette=.5f;
        L.osd=Osd::NightShot; col(.5f,1.f,.55f); L.dateFmt=0; date(1999,10,31,23,47); break;
    case 14: // Night Vision LCD
    default:
        L.nightVision=true; tint(.3f,1.f,.35f); L.gainDB=20.f;
        L.lumaBlur=1.0f; L.chromaBlur=2.f; L.sharpen=.5f; L.noiseLuma=.07f; L.noiseChroma=.004f;
        L.flicker=.04f; L.contrast=1.15f; L.bloom=.5f; L.smear=.2f;
        L.codecQ=1.8f; L.interlace=true; L.display=Disp::LCD; L.vignette=.45f;
        L.osd=Osd::Viewfinder; col(.6f,1.f,.65f); L.dateFmt=2; date(2004,10,31,23,47); break;
    }

    if (L.display == Disp::CRT) L.curvature = 0.03f;

    // Intensité : Light / Standard / Heavy
    static const float K[3] = {0.55f, 1.0f, 1.7f};
    float k = K[std::clamp(intensity, 0, 2)];
    for (float* p : {&L.lumaBlur,&L.chromaBlur,&L.chromaShiftX,&L.noiseLuma,&L.noiseChroma,&L.jitter,&L.dropouts,
                     &L.streaks,&L.tracking,&L.headSwitch,&L.flicker,&L.bloom,&L.smear,&L.vignette,&L.gainDB})
        *p *= k;
    L.codecQ *= (intensity == 0 ? 0.6f : intensity == 2 ? 1.5f : 1.f);
    L.satMul = 1.f + (L.satMul - 1.f) * k;
    if (L.generations > 1) L.generations = std::max(1, L.generations + intensity - 1);
    return L;
}
} // namespace rsc
