// Couche OpenFX (OpenFX Support Library). Rendu CPU float RGBA, image entière (pas de tiling) :
// les effets verticaux (blocs DCT, jitter de lignes, head-switch) exigent la trame complète.
#include "ofxsImageEffect.h"
#include "ofxsMultiThread.h"
#include "Engine.h"
#include <memory>
#include <cmath>

using namespace OFX;

#define kPluginName        "Retro Signal Chain"
#define kPluginGrouping    "Retro Signal"
#define kPluginDescription "Simulation d'une chaîne de signal vidéo rétro : capteur, électronique, bande/codec, lecture/affichage."
#define kPluginIdentifier  "com.example.RetroSignalChain"
#define kVersionMajor 1
#define kVersionMinor 0

class RetroPlugin : public ImageEffect {
    Clip *dst_, *src_;
    ChoiceParam *look_, *intensity_, *quality_, *osd_;
    DoubleParam *mix_, *damage_, *tapeNoise_, *gain_, *sat_;
    BooleanParam *custom_, *customGain_;
    IntParam *year_, *month_, *day_, *hour_, *minute_, *second_, *seed_;
public:
    explicit RetroPlugin(OfxImageEffectHandle h) : ImageEffect(h) {
        dst_ = fetchClip(kOfxImageEffectOutputClipName);
        src_ = fetchClip(kOfxImageEffectSimpleSourceClipName);
        look_ = fetchChoiceParam("look"); intensity_ = fetchChoiceParam("intensity"); quality_ = fetchChoiceParam("quality");
        osd_ = fetchChoiceParam("osd");
        mix_ = fetchDoubleParam("mix"); damage_ = fetchDoubleParam("damage"); tapeNoise_ = fetchDoubleParam("tapeNoise");
        gain_ = fetchDoubleParam("maxGain"); sat_ = fetchDoubleParam("saturation");
        custom_ = fetchBooleanParam("customDate"); customGain_ = fetchBooleanParam("customGain");
        year_ = fetchIntParam("year"); month_ = fetchIntParam("month"); day_ = fetchIntParam("day");
        hour_ = fetchIntParam("hour"); minute_ = fetchIntParam("minute"); second_ = fetchIntParam("second");
        seed_ = fetchIntParam("seed");
    }

    void changedParam(const InstanceChangedArgs&, const std::string& name) override {
        if (name == "customGain" || name.empty()) gain_->setEnabled(customGain_->getValue());
        bool c = custom_->getValue();
        if (name == "customDate" || name.empty()) for (IntParam* p : {year_, month_, day_, hour_, minute_, second_}) p->setEnabled(c);
    }

    bool isIdentity(const IsIdentityArguments& a, Clip*& clip, double& t) override {
        if (mix_->getValueAtTime(a.time) <= 0.0) { clip = src_; t = a.time; return true; }
        return false;
    }

    void render(const RenderArguments& a) override {
        std::unique_ptr<Image> dst(dst_->fetchImage(a.time)), src(src_->fetchImage(a.time));
        if (!dst || !src) throwSuiteStatusException(kOfxStatFailed);
        if (dst->getPixelDepth() != eBitDepthFloat || dst->getPixelComponents() != ePixelComponentRGBA)
            throwSuiteStatusException(kOfxStatErrFormat);

        const OfxRectI db = dst->getBounds(), sb = src->getBounds();
        const int W = db.x2 - db.x1, H = db.y2 - db.y1, sW = sb.x2 - sb.x1;
        if (W < 2 || H < 2 || sW < 1) return;

        auto srcRow = [&](int r) { // r = ligne depuis le HAUT ; OFX : y croissant vers le haut
            int oy = std::min(std::max(db.y2 - 1 - r, sb.y1), sb.y2 - 1);
            return (const float*)src->getPixelAddress(sb.x1, oy);
        };

        rsc::Frame fr; fr.w = W; fr.h = H;
        for (auto& c : fr.c) c = rsc::Plane(W, H);
        for (int r = 0; r < H; ++r) {
            const float* sp = srcRow(r);
            for (int x = 0; x < W; ++x) { const float* p = sp + 4 * std::min(x, sW - 1);
                fr.c[0].row(r)[x] = p[0]; fr.c[1].row(r)[x] = p[1]; fr.c[2].row(r)[x] = p[2]; }
        }

        const double t = a.time; double fps = src_->getFrameRate(); if (fps <= 0) fps = 24.0;
        rsc::Params P;
        P.look = look_->getValueAtTime(t); P.intensity = intensity_->getValueAtTime(t); P.quality = quality_->getValueAtTime(t);
        P.damage = (float)damage_->getValueAtTime(t) / 100.f; P.tapeNoise = (float)tapeNoise_->getValueAtTime(t) / 100.f;
        P.gainOverride = customGain_->getValueAtTime(t); P.gainDB = (float)gain_->getValueAtTime(t); P.saturation = (float)sat_->getValueAtTime(t) / 100.f;
        P.osdMode = osd_->getValueAtTime(t); P.customDate = custom_->getValueAtTime(t);
        P.year = year_->getValueAtTime(t); P.month = month_->getValueAtTime(t); P.day = day_->getValueAtTime(t);
        P.hour = hour_->getValueAtTime(t); P.minute = minute_->getValueAtTime(t); P.second = second_->getValueAtTime(t);
        P.seed = seed_->getValueAtTime(t);
        const float mix = (float)mix_->getValueAtTime(t) / 100.f;

        rsc::process(fr, P, (int)std::floor(t + 0.5), t / fps);

        for (int r = 0; r < H; ++r) {
            float* dp = (float*)dst->getPixelAddress(db.x1, db.y2 - 1 - r); const float* sp = srcRow(r);
            for (int x = 0; x < W; ++x) { const float* s = sp + 4 * std::min(x, sW - 1); float* d = dp + 4 * x;
                for (int k = 0; k < 3; ++k) d[k] = s[k] + (fr.c[k].row(r)[x] - s[k]) * mix;
                d[3] = s[3]; }
        }
    }
};

class RetroFactory : public PluginFactoryHelper<RetroFactory> {
public:
    RetroFactory() : PluginFactoryHelper<RetroFactory>(kPluginIdentifier, kVersionMajor, kVersionMinor) {}
    void load() override {}
    void unload() override {}

    void describe(ImageEffectDescriptor& d) override {
        d.setLabels(kPluginName, kPluginName, kPluginName);
        d.setPluginGrouping(kPluginGrouping); d.setPluginDescription(kPluginDescription);
        d.addSupportedContext(eContextFilter); d.addSupportedContext(eContextGeneral);
        d.addSupportedBitDepth(eBitDepthFloat);
        d.setSingleInstance(false); d.setHostFrameThreading(false); d.setSupportsMultiResolution(false);
        d.setSupportsTiles(false); d.setTemporalClipAccess(false); d.setRenderTwiceAlways(false);
        d.setSupportsMultipleClipPARs(false); d.setRenderThreadSafety(eRenderFullySafe);
    }

    void describeInContext(ImageEffectDescriptor& d, ContextEnum) override {
        ClipDescriptor* s = d.defineClip(kOfxImageEffectSimpleSourceClipName);
        s->addSupportedComponent(ePixelComponentRGBA); s->setTemporalClipAccess(false); s->setSupportsTiles(false);
        ClipDescriptor* o = d.defineClip(kOfxImageEffectOutputClipName);
        o->addSupportedComponent(ePixelComponentRGBA); o->setSupportsTiles(false);

        PageParamDescriptor* page = d.definePageParam("Controls");
        auto choice = [&](const char* id, const char* label, std::initializer_list<const char*> opts, int def, ParamDescriptor* parent = nullptr) {
            ChoiceParamDescriptor* p = d.defineChoiceParam(id); p->setLabel(label);
            for (auto* o : opts) p->appendOption(o); p->setDefault(def);
            if (parent) p->setParent(*parent); page->addChild(*p); };
        auto dbl = [&](const char* id, const char* label, double mn, double mx, double def, ParamDescriptor* parent = nullptr) {
            DoubleParamDescriptor* p = d.defineDoubleParam(id); p->setLabel(label);
            p->setRange(mn, mx); p->setDisplayRange(mn, mx); p->setDefault(def); p->setAnimates(true);
            if (parent) p->setParent(*parent); page->addChild(*p); };
        auto integer = [&](const char* id, const char* label, int mn, int mx, int def, ParamDescriptor* parent = nullptr) {
            IntParamDescriptor* p = d.defineIntParam(id); p->setLabel(label);
            p->setRange(mn, mx); p->setDisplayRange(mn, mx); p->setDefault(def);
            if (parent) p->setParent(*parent); page->addChild(*p); };

        choice("look", "Look", {"Home Video '89 (VHS-C)", "Birthday Party '93", "Clock Never Set '95", "Travel Tape '98",
               "School Play '02", "Copy of a Copy", "Field Report '91", "DV Tape '03", "Viewfinder '03", "Digicam Movie '03",
               "Bedroom Webcam '06", "Pocket Cam '09", "Corner Store '97", "Night Vision '99", "Night Vision LCD"}, 0);
        choice("intensity", "Intensity", {"Light", "Standard", "Heavy"}, 1);
        choice("quality", "Quality", {"Fast", "Balanced", "Best"}, 1);
        dbl("mix", "Mix With Original (%)", 0, 100, 100);
        dbl("damage", "Damage / Tape Wear (%)", 0, 300, 100);
        dbl("tapeNoise", "Tape Noise / Streaks (%)", 0, 300, 100);
        { BooleanParamDescriptor* cg = d.defineBooleanParam("customGain"); cg->setLabel("Override Look Gain"); cg->setDefault(false); page->addChild(*cg); }
        dbl("maxGain", "Max Gain (dB)", 0, 36, 12);
        dbl("saturation", "Saturation (%)", 0, 300, 100);

        GroupParamDescriptor* g = d.defineGroupParam("display"); g->setLabel("Display / Overlay"); g->setOpen(true); page->addChild(*g);
        choice("osd", "Overlay", {"Auto (Look)", "Force On", "Force Off"}, 0, g);
        BooleanParamDescriptor* b = d.defineBooleanParam("customDate"); b->setLabel("Custom Date / Time"); b->setDefault(false);
        b->setParent(*g); page->addChild(*b);
        integer("year", "Year", 1980, 2030, 2003, g); integer("month", "Month", 1, 12, 10, g); integer("day", "Day", 1, 31, 22, g);
        integer("hour", "Hour", 0, 23, 12, g); integer("minute", "Minute", 0, 59, 0, g); integer("second", "Second", 0, 59, 0, g);
        integer("seed", "Noise Seed", 0, 9999, 0);
    }

    ImageEffect* createInstance(OfxImageEffectHandle h, ContextEnum) override { return new RetroPlugin(h); }
};

namespace OFX { namespace Plugin {
void getPluginIDs(PluginFactoryArray& ids) { static RetroFactory p; ids.push_back(&p); }
}}
