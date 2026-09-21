#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

namespace
{
    constexpr int kRootNote = 60;   // FL Studio "C5" == MIDI note 60
    constexpr double kRateBeats[]  = { 1.0, 0.75, 0.5, 1.0 / 3.0, 0.375, 0.25, 1.0 / 6.0, 0.125 };
    constexpr double kDelayBeats[] = { 1.0, 0.75, 0.5, 0.25 };
    constexpr double kHtBeats[]    = { 0.5, 1.0, 2.0, 4.0 };
    constexpr int kLayerInt[] = { 0, 12, -12, 7, 5, 3, 10 };

    inline float hermite (float x, float y0, float y1, float y2, float y3)
    {
        const float c0 = y1;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * x + c2) * x + c1) * x + c0;
    }

    String stepId (int i, const char* what) { return "s" + String (i) + "_" + what; }

    unsigned scaleMask (int idx)
    {
        auto m = [] (std::initializer_list<int> l) { unsigned r = 0; for (int i : l) r |= 1u << i; return r; };
        switch (idx)
        {
            case 1:  return m ({ 0, 2, 3, 5, 7, 8, 10 });   // minor
            case 2:  return m ({ 0, 2, 3, 5, 7, 8, 11 });   // harmonic minor
            case 3:  return m ({ 0, 1, 3, 5, 7, 8, 10 });   // phrygian
            case 4:  return m ({ 0, 1, 4, 5, 7, 8, 10 });   // phrygian dominant
            case 5:  return m ({ 0, 2, 3, 5, 7, 9, 10 });   // dorian
            case 6:  return m ({ 0, 1, 3, 5, 6, 8, 10 });   // locrian
            case 7:  return m ({ 0, 2, 3, 7, 8 });          // hirajoshi
            case 8:  return m ({ 0, 2, 4, 5, 7, 9, 11 });   // major
            default: return 0xFFFu;
        }
    }

    int quantizeToScale (int n, int key, unsigned mask)
    {
        for (int d = 0; d < 7; ++d)
            for (int s : { -d, d })
            {
                const int m = n + s;
                const int pc = ((m - key) % 12 + 12) % 12;
                if (mask & (1u << pc)) return m;
            }
        return n;
    }

    struct KV { const char* id; float v; };
}

//==============================================================================
AudioProcessorValueTreeState::ParameterLayout RiverrProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout L;
    auto pid = [] (const String& s) { return ParameterID (s, 1); };

    auto F = [&] (const String& id, const String& name, float lo, float hi, float def,
                  float step = 0.f, float centre = -1.f, const String& label = {})
    {
        NormalisableRange<float> r (lo, hi, step);
        if (centre > 0.f) r.setSkewForCentre (centre);
        L.add (std::make_unique<AudioParameterFloat> (pid (id), name, r, def,
                                                       AudioParameterFloatAttributes().withLabel (label)));
    };
    auto I = [&] (const String& id, const String& name, int lo, int hi, int def)
    {
        L.add (std::make_unique<AudioParameterInt> (pid (id), name, lo, hi, def));
    };
    auto B = [&] (const String& id, const String& name, bool def)
    {
        L.add (std::make_unique<AudioParameterBool> (pid (id), name, def));
    };
    auto C = [&] (const String& id, const String& name, const StringArray& items, int def)
    {
        L.add (std::make_unique<AudioParameterChoice> (pid (id), name, items, def));
    };

    // macros (PLAY page)
    F ("mDark", "Macro Dark", 0.f, 1.f, 0.f);
    F ("mSpace", "Macro Space", 0.f, 1.f, 0.f);
    F ("mGrit", "Macro Grit", 0.f, 1.f, 0.f);
    F ("mMove", "Macro Move", 0.f, 1.f, 0.f);
    F ("mGlue", "Macro Glue", 0.f, 1.f, 0.f);
    F ("mHalf", "Macro Half", 0.f, 1.f, 0.f);

    // sound
    F ("gain", "Gain", -30.f, 6.f, 0.f, 0.1f, -1.f, "dB");
    I ("octave", "A Octave", -3, 3, 0);
    F ("tune", "A Tune", -12.f, 12.f, 0.f, 0.01f, -1.f, "st");
    B ("autotune", "Auto C5", true);
    B ("reverse", "Reverse", false);
    F ("start", "A Start", 0.f, 1.f, 0.f);
    F ("end", "A End", 0.f, 1.f, 1.f);
    F ("aLevel", "A Level", -24.f, 6.f, 0.f, 0.1f, -1.f, "dB");
    F ("aPan", "A Pan", -1.f, 1.f, 0.f);
    F ("bLevel", "B Level", -24.f, 6.f, -2.f, 0.1f, -1.f, "dB");
    F ("bPan", "B Pan", -1.f, 1.f, 0.f);
    I ("bOct", "B Octave", -3, 3, 0);
    F ("bTune", "B Tune", -12.f, 12.f, 0.f, 0.01f, -1.f, "st");
    F ("bStart", "B Start", 0.f, 1.f, 0.f);
    F ("bEnd", "B End", 0.f, 1.f, 1.f);
    C ("abMode", "A/B Mode", { "Layer", "Alternate", "Random" }, 0);
    F ("atk", "Attack", 0.001f, 2.f, 0.003f, 0.f, 0.1f, "s");
    F ("dec", "Decay", 0.01f, 4.f, 0.4f, 0.f, 0.5f, "s");
    F ("sus", "Sustain", 0.f, 1.f, 1.f);
    F ("rel", "Release", 0.01f, 8.f, 0.5f, 0.f, 1.f, "s");
    F ("scan", "Scan", 0.f, 1.f, 0.f);
    F ("spread", "Spread", 0.f, 1.f, 0.f);

    // arp
    B ("arpOn", "Arp On", true);
    C ("dir", "Direction", { "Up", "Down", "Up-Down", "Down-Up", "Random", "As played" }, 0);
    C ("octRange", "Octaves", { "1", "2", "3", "4" }, 1);
    C ("rate", "Rate", { "1/4", "1/8.", "1/8", "1/8T", "1/16.", "1/16", "1/16T", "1/32" }, 5);
    F ("gate", "Gate", 0.05f, 4.f, 0.6f, 0.f, 0.8f, "x");
    F ("swing", "Swing", 0.f, 0.75f, 0.f);
    F ("humT", "Human Time", 0.f, 1.f, 0.f);
    F ("humV", "Human Vel", 0.f, 1.f, 0.f);
    I ("steps", "Steps", 1, 16, 16);
    C ("key", "Key", { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0);
    C ("scale", "Scale", { "Chromatic", "Minor", "Harm. Minor", "Phrygian", "Phryg. Dom.", "Dorian", "Locrian", "Hirajoshi", "Major" }, 0);
    F ("evolve", "Evolve", 0.f, 1.f, 0.f);
    F ("jump", "Octave Jump", 0.f, 1.f, 0.f);
    C ("layer", "Layer", { "Off", "+12", "-12", "+7", "+5", "+3", "+10" }, 0);
    F ("layerLvl", "Layer Level", 0.f, 1.f, 0.5f);
    I ("lenPit", "Pitch Length", 1, 16, 16);
    I ("lenVel", "Velocity Length", 1, 16, 16);
    I ("lenGate", "Gate Length", 1, 16, 16);
    I ("lenProb", "Probability Length", 1, 16, 16);
    I ("lenRat", "Ratchet Length", 1, 16, 16);
    F ("choke", "Choke", 0.f, 1.f, 0.f);
    F ("duck", "Duck", 0.f, 1.f, 0.f);
    F ("glide", "Glide", 0.f, 1.f, 0.f);

    for (int i = 0; i < 16; ++i)
    {
        const String n = String (i + 1);
        B (stepId (i, "on"), "Step " + n + " On", true);
        I (stepId (i, "pit"), "Step " + n + " Pitch", -12, 12, 0);
        F (stepId (i, "vel"), "Step " + n + " Vel", 0.f, 1.f, 0.85f);
        F (stepId (i, "gate"), "Step " + n + " Gate", 0.1f, 1.f, 1.f);
        F (stepId (i, "prob"), "Step " + n + " Prob", 0.f, 1.f, 1.f);
        I (stepId (i, "rat"), "Step " + n + " Ratchet", 1, 4, 1);
    }

    // fx: filter
    B ("fltOn", "Filter On", true);
    F ("cut", "Cutoff", 20.f, 20000.f, 9000.f, 0.f, 1500.f, "Hz");
    F ("res", "Resonance", 0.3f, 2.f, 0.7f);
    F ("lfoRate", "LFO Rate", 0.05f, 10.f, 0.4f, 0.f, 1.f, "Hz");
    F ("lfoDepth", "LFO Depth", 0.f, 1.f, 0.f);
    // drive
    B ("drvOn", "Drive On", false);
    C ("drvType", "Drive Type", { "Soft", "Hard", "Fold" }, 0);
    F ("drvAmt", "Drive", 0.f, 1.f, 0.35f);
    F ("drvTone", "Drive Tone", 0.f, 1.f, 0.7f);
    F ("drvMix", "Drive Mix", 0.f, 1.f, 0.6f);
    F ("drvOut", "Drive Out", -12.f, 6.f, 0.f, 0.1f, -1.f, "dB");
    // tape
    B ("tapeOn", "Tape On", false);
    F ("tapeNoise", "Tape Noise", 0.f, 1.f, 0.15f);
    F ("drift", "Tape Wow", 0.f, 1.f, 0.3f);
    F ("tapeFlutter", "Tape Flutter", 0.f, 1.f, 0.2f);
    F ("tapeSat", "Tape Sat", 0.f, 1.f, 0.3f);
    F ("tapeTone", "Tape Tone", 0.f, 1.f, 0.8f);
    // halftime
    B ("htOn", "Halftime On", false);
    C ("htLen", "Halftime Length", { "1/8", "1/4", "1/2", "1 BAR" }, 1);
    F ("htMix", "Halftime Mix", 0.f, 1.f, 0.5f);
    F ("htTone", "Halftime Tone", 0.f, 1.f, 0.6f);
    F ("htFlux", "Halftime Flux", 0.f, 1.f, 0.f);
    // halfspeed
    B ("hsOn", "Halfspeed On", false);
    F ("hsAmt", "Halfspeed Amount", 0.f, 1.f, 0.5f);
    F ("hsDive", "Halfspeed Dive", 0.f, 1.f, 0.3f);
    F ("hsSpeed", "Halfspeed Speed", 0.25f, 1.f, 0.5f);
    // delay
    B ("dlyOn", "Delay On", true);
    C ("dlyTime", "Delay Time", { "1/4", "1/8.", "1/8", "1/16" }, 1);
    F ("dlyFb", "Delay Feedback", 0.f, 0.95f, 0.45f);
    F ("dlyMix", "Delay Mix", 0.f, 1.f, 0.25f);
    F ("dlyTone", "Delay Tone", 0.f, 1.f, 0.5f);
    F ("dlyDrift", "Delay Drift", 0.f, 1.f, 0.f);
    // reverb
    B ("revOn", "Reverb On", true);
    F ("revSize", "Reverb Size", 0.f, 1.f, 0.85f);
    F ("revDamp", "Reverb Damp", 0.f, 1.f, 0.6f);
    F ("revMix", "Reverb Mix", 0.f, 1.f, 0.4f);
    F ("revPre", "Reverb Pre", 0.f, 200.f, 15.f, 0.f, -1.f, "ms");
    F ("revWidth", "Reverb Width", 0.f, 1.f, 1.f);

    return L;
}

//==============================================================================
RiverrProcessor::RiverrProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    auto P = [this] (const String& id) { return apvts.getRawParameterValue (id); };

    pGain = P ("gain");       pOctave = P ("octave");   pTune = P ("tune");
    pAutoTune = P ("autotune"); pReverse = P ("reverse");
    pStart = P ("start");     pEnd = P ("end");
    pALevel = P ("aLevel");   pAPan = P ("aPan");       pBLevel = P ("bLevel"); pBPan = P ("bPan");
    pBOct = P ("bOct");       pBTune = P ("bTune");     pBStart = P ("bStart"); pBEnd = P ("bEnd");
    pAbMode = P ("abMode");
    pAtk = P ("atk");         pDec = P ("dec");         pSus = P ("sus");   pRel = P ("rel");
    pArpOn = P ("arpOn");     pDir = P ("dir");         pOctRange = P ("octRange");
    pRate = P ("rate");       pGate = P ("gate");       pSwing = P ("swing");
    pHumT = P ("humT");       pHumV = P ("humV");       pSteps = P ("steps");
    pKey = P ("key");         pScale = P ("scale");     pEvolve = P ("evolve");  pJump = P ("jump");
    pLayer = P ("layer");     pLayerLvl = P ("layerLvl");
    pLenPit = P ("lenPit");   pLenVel = P ("lenVel");   pLenGate = P ("lenGate");
    pLenProb = P ("lenProb"); pLenRat = P ("lenRat");
    pScan = P ("scan");       pSpread = P ("spread");
    pChoke = P ("choke");     pDuck = P ("duck");       pGlide = P ("glide");
    pCut = P ("cut");         pRes = P ("res");         pLfoRate = P ("lfoRate"); pLfoDepth = P ("lfoDepth");
    pDrvOn = P ("drvOn");     pDrvType = P ("drvType"); pDrvAmt = P ("drvAmt");   pDrvTone = P ("drvTone");
    pDrvMix = P ("drvMix");   pDrvOut = P ("drvOut");
    pTapeOn = P ("tapeOn");   pTapeNoise = P ("tapeNoise"); pDrift = P ("drift");
    pTapeFlutter = P ("tapeFlutter"); pTapeSat = P ("tapeSat"); pTapeTone = P ("tapeTone");
    pHtOn = P ("htOn");       pHtLen = P ("htLen");     pHtMix = P ("htMix");   pHtTone = P ("htTone"); pHtFlux = P ("htFlux");
    pHsOn = P ("hsOn");       pHsAmt = P ("hsAmt");     pHsDive = P ("hsDive"); pHsSpeed = P ("hsSpeed");
    pDlyOn = P ("dlyOn");     pDlyTime = P ("dlyTime"); pDlyFb = P ("dlyFb");   pDlyMix = P ("dlyMix");
    pDlyTone = P ("dlyTone"); pDlyDrift = P ("dlyDrift");
    pFltOn = P ("fltOn");     pMDark = P ("mDark");     pMSpace = P ("mSpace"); pMGrit = P ("mGrit");
    pMMove = P ("mMove");     pMGlue = P ("mGlue");     pMHalf = P ("mHalf");
    pRevOn = P ("revOn");     pRevSize = P ("revSize"); pRevDamp = P ("revDamp"); pRevMix = P ("revMix");
    pRevPre = P ("revPre");   pRevWidth = P ("revWidth");

    for (int i = 0; i < 16; ++i)
    {
        sOn[(size_t) i]   = P (stepId (i, "on"));
        sPit[(size_t) i]  = P (stepId (i, "pit"));
        sVel[(size_t) i]  = P (stepId (i, "vel"));
        sGate[(size_t) i] = P (stepId (i, "gate"));
        sProb[(size_t) i] = P (stepId (i, "prob"));
        sRat[(size_t) i]  = P (stepId (i, "rat"));
    }

    for (auto& s : laneStep) s.store (-1);

    applyFactoryPreset (0);   // the plugin starts as a ready-made hard arp
}

AudioProcessorEditor* RiverrProcessor::createEditor() { return new RiverrEditor (*this); }

bool RiverrProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == AudioChannelSet::stereo();
}

int RiverrProcessor::getLaneLength (int lane) const
{
    std::atomic<float>* p[] = { pSteps, pLenPit, pLenVel, pLenGate, pLenProb, pLenRat };
    return jlimit (1, 16, (int) p[jlimit (0, 5, lane)]->load());
}

void RiverrProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    fs = sampleRate;
    maxBlock = jmax (16, samplesPerBlock);

    dsp::ProcessSpec spec { sampleRate, (uint32) maxBlock, 2 };
    filter.prepare (spec);
    filter.setType (dsp::StateVariableTPTFilterType::lowpass);
    filter.reset();

    oversampler.initProcessing ((size_t) maxBlock);
    oversampler.reset();
    scrL.assign ((size_t) maxBlock, 0.f);  scrR.assign ((size_t) maxBlock, 0.f);
    revL.assign ((size_t) maxBlock, 0.f);  revR.assign ((size_t) maxBlock, 0.f);
    drvLpL = drvLpR = tpLpL = tpLpR = noiseLpL = noiseLpR = htLpL = htLpR = 0.f;

    const int htMax = (int) (sampleRate * 6.5) + 16;
    for (auto& a : htBuf) for (auto& b : a) b.assign ((size_t) htMax, 0.f);
    htPos = 0; htRec = 0; htRead = 0.0; htLenPrev = 0;

    const int dlSize = (int) (sampleRate * 4.0) + 8;
    dlL.assign ((size_t) dlSize, 0.f);
    dlR.assign ((size_t) dlSize, 0.f);
    dlW = 0; dlSmooth = 0.f; dlLpL = dlLpR = 0.f;

    const int preSize = (int) (sampleRate * 0.25) + 8;
    preL.assign ((size_t) preSize, 0.f);  preR.assign ((size_t) preSize, 0.f);
    preW = 0;

    reverb.setSampleRate (sampleRate);
    reverb.reset();

    for (auto& v : voices) { v.active = false; v.smp.reset(); }
    held.clear();  held.reserve (128);
    seq.reserve (2048);  seqTmp.reserve (2048);
    pending.clear();  pending.reserve (256);
    evo.fill (0);
    haveLast = false;
    noteCounter = 0;
    freePpq = 0.0;
    prevWow = 1.0;
    haveLastPitch = { false, false };
}

//==============================================================================
void RiverrProcessor::updateEffective()
{
    const float mD = pMDark->load(), mS = pMSpace->load(), mG = pMGrit->load(),
                mM = pMMove->load(), mGl = pMGlue->load(), mH = pMHalf->load();
    auto c01 = [] (float v) { return jlimit (0.f, 1.f, v); };

    e.cut = pCut->load() * std::pow (2.f, -3.5f * mD);
    e.res = pRes->load();

    e.revOn = pRevOn->load() > 0.5f || mS > 0.02f;
    e.revMix = c01 (pRevMix->load() + 0.5f * mS);
    e.revSize = c01 (pRevSize->load() + 0.15f * mS);
    e.revDamp = c01 (pRevDamp->load() + 0.35f * mD);
    e.dlyOn = pDlyOn->load() > 0.5f || mS > 0.02f;
    e.dlyMix = c01 (pDlyMix->load() + 0.3f * mS);

    const bool drvBase = pDrvOn->load() > 0.5f;
    e.drvOn = drvBase || mG > 0.02f;
    e.drvAmt = drvBase ? c01 (pDrvAmt->load() + 0.6f * mG) : 0.2f + 0.6f * mG;
    e.drvMix = drvBase ? jmax (pDrvMix->load(), 0.9f * mG) : 0.9f * mG;

    const bool tapeBase = pTapeOn->load() > 0.5f;
    e.tapeOn = tapeBase || mG > 0.02f;
    e.tapeNoise = tapeBase ? c01 (pTapeNoise->load() + 0.25f * mG) : 0.25f * mG;
    e.tapeSat = tapeBase ? c01 (pTapeSat->load() + 0.4f * mG) : 0.4f * mG;
    e.wowOn = tapeBase || mM > 0.02f;
    e.drift = c01 ((tapeBase ? pDrift->load() : 0.f) + 0.5f * mM);

    e.spread = c01 (pSpread->load() + 0.6f * mM);
    e.scan = c01 (pScan->load() + 0.35f * mM);
    e.evolve = c01 (pEvolve->load() + 0.7f * mM);
    e.choke = c01 (pChoke->load() + 0.85f * mGl);
    e.duck = c01 (pDuck->load() + 0.5f * mGl);
    e.glide = c01 (pGlide->load() + 0.12f * mGl);

    const bool htBase = pHtOn->load() > 0.5f;
    e.htOn = htBase || mH > 0.02f;
    e.htMix = htBase ? jmax (pHtMix->load(), 0.6f * mH) : 0.6f * mH;
    const bool hsBase = pHsOn->load() > 0.5f;
    e.hsOn = hsBase || mH > 0.02f;
    e.hsAmt = hsBase ? c01 (pHsAmt->load() + 0.5f * mH) : 0.7f * mH;
}

//==============================================================================
void RiverrProcessor::handleNoteOn (int note, float vel, int pos)
{
    if (pArpOn->load() > 0.5f)
    {
        if (held.empty()) { noteCounter = 0; haveLast = false; evo.fill (0); }
        if (std::find (held.begin(), held.end(), note) == held.end())
            held.push_back (note);
        noteVel[(size_t) note] = vel;
    }
    else
    {
        fireHit (note, (float) (note - kRootNote), vel, pos, -1, ++groupCounter);
    }
}

void RiverrProcessor::handleNoteOff (int note, int pos)
{
    held.erase (std::remove (held.begin(), held.end(), note), held.end());

    for (auto& v : voices)
        if (v.active && v.note == note && v.releaseCountdown < 0)
            v.releaseCountdown = jmax (0, pos - v.delay);
}

// one musical hit -> one or two voices (A / B), depending on the A/B mode
void RiverrProcessor::fireHit (int note, float semis, float vel, int delay, int gateSamples, int group)
{
    const bool hasA = audioSample[0] != nullptr, hasB = audioSample[1] != nullptr;
    if (! hasA && ! hasB) return;

    if (group != lastCountedGroup) { lastCountedGroup = group; hitCount.fetch_add (1); }

    const int mode = (int) pAbMode->load();
    int mask;
    if (mode == 0 || ! (hasA && hasB))
        mask = (hasA ? 1 : 0) | (hasB ? 2 : 0);
    else
    {
        if (group != curGroup)
        {
            curGroup = group;
            curChoice = (mode == 1) ? (int) (altCounter++ & 1u) : (rng.nextBool() ? 1 : 0);
        }
        mask = curChoice ? 2 : 1;
    }

    if (group != hsGroup) { hsGroup = group; hsChoice = e.hsOn && rng.nextFloat() < e.hsAmt; }

    if (mask & 1) triggerVoice (note, semis, vel, delay, gateSamples, 0, group, hsChoice);
    if (mask & 2) triggerVoice (note, semis, vel, delay, gateSamples, 1, group, hsChoice);
}

void RiverrProcessor::triggerVoice (int note, float semis, float vel, int delay, int gateSamples,
                                    int slot, int group, bool halfSpeed)
{
    auto smp = audioSample[(size_t) slot];
    if (smp == nullptr) return;

    // the new hit chokes / ducks the older arp voices (this is what makes chords "talk" to each other)
    if (note < 0 && (e.choke > 0.f || e.duck > 0.f))
    {
        const float fadeSec = 0.012f * std::pow (1.5f / 0.012f, 1.f - e.choke);
        for (auto& x : voices)
            if (x.active && x.note < 0 && x.group != group)
            {
                if (e.choke > 0.f && x.chokeStep <= 0.f) x.chokeStep = 1.f / jmax (16.f, fadeSec * (float) fs);
                if (e.duck > 0.f) x.duckGain = jmin (x.duckGain, 1.f - 0.85f * e.duck);
            }
    }

    Voice* v = nullptr;
    for (auto& x : voices)
        if (! x.active) { v = &x; break; }
    if (v == nullptr)
    {
        v = &voices[0];
        for (auto& x : voices)
            if (x.age < v->age) v = &x;
    }

    const auto& s = *smp;
    const int len = s.audio.getNumSamples();
    const bool B = slot == 1;

    const float st = (B ? pBStart : pStart)->load();
    float en = (B ? pBEnd : pEnd)->load();
    if (en < st + 0.005f) en = jmin (1.f, st + 0.005f);

    v->startS = st * (float) len;
    v->endS = jmax (v->startS + 16.f, jmin ((float) (len - 1), en * (float) len));
    v->reverse = pReverse->load() > 0.5f;

    const float scan = e.scan;
    const float off = scan > 0.f ? rng.nextFloat() * scan * 0.5f * (v->endS - v->startS) : 0.f;
    v->pos = v->reverse ? (double) v->endS - 1.0 - (double) off : (double) v->startS + (double) off;

    const float cents = e.wowOn && e.drift > 0.f ? (rng.nextFloat() * 2.f - 1.f) * e.drift * 18.f : 0.f;
    v->detune = cents / 100.f;

    // pitch: target, plus where a glide starts from
    float target = semis + 12.f * (B ? pBOct : pOctave)->load() + (B ? pBTune : pTune)->load()
                   + (pAutoTune->load() > 0.5f ? s.rootShift : 0.f);
    float from = target;
    if (note < 0 && e.glide > 0.f && haveLastPitch[(size_t) slot]) from = lastSemis[(size_t) slot];
    lastSemis[(size_t) slot] = target;
    haveLastPitch[(size_t) slot] = true;

    float coef = 0.f;
    if (halfSpeed)
    {
        const float dive = pHsDive->load();
        const float t2 = target + 12.f * std::log2 (jmax (0.25f, pHsSpeed->load()));
        if (dive < 0.02f) from = t2;
        else coef = 1.f - std::exp (-1.f / jmax (1.f, (0.02f + dive * 0.5f) / 3.f * (float) fs));
        target = t2;
    }
    else if (from != target && e.glide > 0.f)
    {
        coef = 1.f - std::exp (-1.f / jmax (1.f, (0.005f + e.glide * e.glide * 0.4f) / 3.f * (float) fs));
    }
    if (coef <= 0.f) from = target;

    v->semisNow = from;
    v->semisTarget = target;
    v->glideCoef = coef;
    v->baseRatio = s.sr / fs;
    v->rate = std::pow (2.0, (double) (from + v->detune) / 12.0) * v->baseRatio;

    const float sp = e.spread;
    const float pan = jlimit (-1.f, 1.f, (B ? pBPan : pAPan)->load() + (sp > 0.f ? (rng.nextFloat() * 2.f - 1.f) * sp : 0.f));
    const float th = (pan + 1.f) * MathConstants<float>::pi * 0.25f;
    v->panL = std::cos (th) * 1.41421356f;
    v->panR = std::sin (th) * 1.41421356f;

    v->smp = smp;
    v->vel = vel * Decibels::decibelsToGain ((B ? pBLevel : pALevel)->load());
    v->delay = jmax (0, delay);
    v->releaseCountdown = gateSamples > 0 ? gateSamples : -1;
    v->note = note;
    v->played = 0;
    v->group = group;
    v->chokeGain = 1.f;
    v->chokeStep = 0.f;
    v->duckGain = 1.f;

    v->adsr.setSampleRate (fs);
    v->adsr.setParameters ({ pAtk->load(), pDec->load(), pSus->load(), pRel->load() });
    v->adsr.noteOn();
    v->active = true;
    v->age = ++ageCounter;
}

void RiverrProcessor::renderVoices (float* L, float* R, int n, double rmA, double rmB)
{
    const float duckCoef = 1.f - std::exp (-1.f / (0.22f * (float) fs));

    for (auto& v : voices)
    {
        if (! v.active) continue;

        const auto& buf = v.smp->audio;
        const int len = buf.getNumSamples();
        const float* d0 = buf.getReadPointer (0);
        const float* d1 = buf.getReadPointer (1);

        for (int i = 0; i < n; ++i)
        {
            if (v.delay > 0) { --v.delay; continue; }

            if (v.releaseCountdown >= 0)
            {
                if (v.releaseCountdown == 0) v.adsr.noteOff();
                --v.releaseCountdown;
            }

            const float env = v.adsr.getNextSample();
            if (! v.adsr.isActive()) { v.active = false; v.smp.reset(); break; }

            if (v.chokeStep > 0.f)
            {
                v.chokeGain -= v.chokeStep;
                if (v.chokeGain <= 0.f) { v.active = false; v.smp.reset(); break; }
            }
            if (v.duckGain < 1.f) v.duckGain += (1.f - v.duckGain) * duckCoef;

            if (v.semisNow != v.semisTarget)
            {
                v.semisNow += (v.semisTarget - v.semisNow) * v.glideCoef;
                if (std::abs (v.semisTarget - v.semisNow) < 0.003f) v.semisNow = v.semisTarget;
                v.rate = std::pow (2.0, (double) (v.semisNow + v.detune) / 12.0) * v.baseRatio;
            }

            const int i0 = (int) v.pos;
            const float fr = (float) (v.pos - (double) i0);
            const int a = jlimit (0, len - 1, i0 - 1), b = jlimit (0, len - 1, i0),
                      c = jlimit (0, len - 1, i0 + 1), d = jlimit (0, len - 1, i0 + 2);

            const float sl = hermite (fr, d0[a], d0[b], d0[c], d0[d]);
            const float sr = hermite (fr, d1[a], d1[b], d1[c], d1[d]);

            const float dist = v.reverse ? (float) (v.pos - (double) v.startS)
                                         : (float) ((double) v.endS - v.pos);
            const float fade = jmin (1.f, dist / 128.f) * jmin (1.f, (float) (++v.played) / 12.f);
            const float g = env * v.vel * fade * v.chokeGain * v.duckGain;

            L[i] += sl * g * v.panL;
            R[i] += sr * g * v.panR;

            const double rm = rmA + (rmB - rmA) * ((double) i / (double) n);
            v.pos += (v.reverse ? -v.rate : v.rate) * rm;
            if (v.pos < (double) v.startS || v.pos >= (double) v.endS)
            {
                v.active = false; v.smp.reset(); break;
            }
        }
    }
}

//==============================================================================
void RiverrProcessor::rebuildSequence()
{
    const int dir = (int) pDir->load();
    const int octs = (int) pOctRange->load() + 1;

    seq.clear();
    seqTmp.clear();

    for (int o = 0; o < octs; ++o)
        for (int n : held)
            seqTmp.push_back ({ jlimit (0, 127, n + 12 * o), noteVel[(size_t) n] });

    if (dir != 5)
    {
        std::sort (seqTmp.begin(), seqTmp.end(), [] (const SeqNote& a, const SeqNote& b) { return a.note < b.note; });
        seqTmp.erase (std::unique (seqTmp.begin(), seqTmp.end(),
                                   [] (const SeqNote& a, const SeqNote& b) { return a.note == b.note; }),
                      seqTmp.end());
    }

    const int n = (int) seqTmp.size();
    switch (dir)
    {
        case 1:
            for (int i = n - 1; i >= 0; --i) seq.push_back (seqTmp[(size_t) i]);
            break;
        case 2:
            seq = seqTmp;
            for (int i = n - 2; i >= 1; --i) seq.push_back (seqTmp[(size_t) i]);
            break;
        case 3:
            for (int i = n - 1; i >= 0; --i) seq.push_back (seqTmp[(size_t) i]);
            for (int i = 1; i <= n - 2; ++i) seq.push_back (seqTmp[(size_t) i]);
            break;
        default:
            seq = seqTmp;
            break;
    }
}

void RiverrProcessor::scheduleStep (long long k, double t, double stepLen)
{
    if (seq.empty()) return;

    auto idx = [k] (int len) { len = jlimit (1, 16, len); return (int) (((k % len) + len) % len); };
    const int iOn   = idx ((int) pSteps->load());
    const int iPit  = idx ((int) pLenPit->load());
    const int iVel  = idx ((int) pLenVel->load());
    const int iGate = idx ((int) pLenGate->load());
    const int iProb = idx ((int) pLenProb->load());
    const int iRat  = idx ((int) pLenRat->load());

    SeqNote sn;
    if ((int) pDir->load() == 4)
        sn = seq[(size_t) rng.nextInt ((int) seq.size())];
    else
        sn = seq[(size_t) (noteCounter % (long long) seq.size())];
    ++noteCounter;

    laneStep[0].store (iOn);  laneStep[1].store (iPit);  laneStep[2].store (iVel);
    laneStep[3].store (iGate); laneStep[4].store (iProb); laneStep[5].store (iRat);

    if (e.evolve > 0.f && rng.nextFloat() < e.evolve * 0.45f)
    {
        int d = rng.nextBool() ? 1 : -1;
        if (rng.nextFloat() < 0.25f) d *= 2;
        int& ev = evo[(size_t) iPit];
        ev = jlimit (-12, 12, ev + d);
        if (std::abs (ev) > 6) ev -= (ev > 0 ? 1 : -1);
    }

    if (sOn[(size_t) iOn]->load() < 0.5f) return;
    if (rng.nextFloat() > sProb[(size_t) iProb]->load()) return;

    int note = sn.note + (int) sPit[(size_t) iPit]->load() + evo[(size_t) iPit];
    if (rng.nextFloat() < pJump->load() * 0.5f)
        note += (rng.nextFloat() < 0.65f ? 12 : -12);
    note = jlimit (0, 127, note);

    const unsigned mask = scaleMask ((int) pScale->load());
    if (mask != 0xFFFu)
        note = quantizeToScale (note, (int) pKey->load(), mask);

    double delay = 0.0;
    if ((k & 1) != 0) delay += (double) pSwing->load() * stepLen;
    delay += (double) rng.nextFloat() * (double) pHumT->load() * 0.25 * stepLen;

    float vel = sVel[(size_t) iVel]->load() * (0.35f + 0.65f * sn.vel);
    vel *= 1.f - rng.nextFloat() * pHumV->load() * 0.5f;

    const double gateBeats = (double) pGate->load() * (double) sGate[(size_t) iGate]->load() * stepLen;
    const int rat = jlimit (1, 4, (int) sRat[(size_t) iRat]->load());
    const int layer = jlimit (0, 6, (int) pLayer->load());
    const float layerLvl = pLayerLvl->load();

    for (int r = 0; r < rat; ++r)
    {
        const double when = t + delay + (double) r * stepLen / (double) rat;
        const float v = jlimit (0.f, 1.f, vel * (1.f - 0.14f * (float) r));
        const int grp = ++groupCounter;

        if (pending.size() < 250)
            pending.push_back ({ when, note - kRootNote, v, gateBeats / (double) rat, grp });
        if (layer > 0 && pending.size() < 250)
            pending.push_back ({ when + 0.0005, note - kRootNote + kLayerInt[layer], v * layerLvl, gateBeats / (double) rat, grp });
    }
}

//==============================================================================
void RiverrProcessor::processFilter (float* L, float* R, int n)
{
    const float lfoR = pLfoRate->load(), lfoD = pLfoDepth->load();
    const float maxF = (float) jmin (20000.0, fs * 0.45);
    filter.setResonance (e.res);

    for (int i = 0; i < n; i += 32)
    {
        const int len = jmin (32, n - i);
        const float mod = lfoD > 0.f ? std::sin (MathConstants<float>::twoPi * (float) lfoPhase) : 0.f;
        lfoPhase += (double) lfoR * (double) len / fs;
        lfoPhase -= std::floor (lfoPhase);

        filter.setCutoffFrequency (jlimit (20.f, maxF, e.cut * std::pow (2.f, mod * lfoD * 3.f)));
        for (int j = 0; j < len; ++j)
        {
            L[i + j] = filter.processSample (0, L[i + j]);
            R[i + j] = filter.processSample (1, R[i + j]);
        }
    }
}

void RiverrProcessor::processDrive (AudioBuffer<float>& buffer, int n)
{
    const float mix = e.drvMix;
    const int type = jlimit (0, 2, (int) pDrvType->load());
    const float pre = 1.f + e.drvAmt * 24.f;
    const float outG = 1.f / std::pow (pre, 0.35f) * Decibels::decibelsToGain (pDrvOut->load());
    const float fc = jlimit (500.f, (float) (fs * 0.45), 500.f * std::pow (36.f, pDrvTone->load()));
    const float lpc = 1.f - std::exp (-MathConstants<float>::twoPi * fc / (float) fs);

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    if ((int) scrL.size() < n) { scrL.resize ((size_t) n); scrR.resize ((size_t) n); }
    std::copy (L, L + n, scrL.begin());
    std::copy (R, R + n, scrR.begin());

    auto shape = [type, pre] (float x)
    {
        x *= pre;
        if (type == 1) return jlimit (-1.f, 1.f, x);
        if (type == 2) return std::sin (x * 1.5708f);
        return std::tanh (x);
    };

    if (n <= maxBlock)
    {
        dsp::AudioBlock<float> block (buffer);
        auto up = oversampler.processSamplesUp (block);
        for (size_t ch = 0; ch < 2; ++ch)
        {
            float* d = up.getChannelPointer (ch);
            for (size_t i = 0; i < up.getNumSamples(); ++i) d[i] = shape (d[i]);
        }
        oversampler.processSamplesDown (block);
    }
    else
    {
        for (int i = 0; i < n; ++i) { L[i] = shape (L[i]); R[i] = shape (R[i]); }
    }

    for (int i = 0; i < n; ++i)
    {
        drvLpL += lpc * (L[i] * outG - drvLpL);
        drvLpR += lpc * (R[i] * outG - drvLpR);
        L[i] = scrL[(size_t) i] * (1.f - mix) + drvLpL * mix;
        R[i] = scrR[(size_t) i] * (1.f - mix) + drvLpR * mix;
    }
}

void RiverrProcessor::processTape (float* L, float* R, int n)
{
    const float sat = e.tapeSat, noise = e.tapeNoise;
    const float g = 1.f + sat * 2.5f;
    const float fc = jlimit (1500.f, (float) (fs * 0.45), 2500.f * std::pow (8.f, pTapeTone->load()));
    const float lpc = 1.f - std::exp (-MathConstants<float>::twoPi * fc / (float) fs);

    for (int i = 0; i < n; ++i)
    {
        float l = L[i], r = R[i];
        if (sat > 0.f) { l = std::tanh (l * g) / g; r = std::tanh (r * g) / g; }

        if (noise > 0.f)
        {
            const float wl = rng.nextFloat() * 2.f - 1.f, wr = rng.nextFloat() * 2.f - 1.f;
            noiseLpL += 0.3f * (wl - noiseLpL);
            noiseLpR += 0.3f * (wr - noiseLpR);
            l += (0.6f * noiseLpL + 0.4f * wl) * 0.02f * noise;
            r += (0.6f * noiseLpR + 0.4f * wr) * 0.02f * noise;
        }

        tpLpL += lpc * (l - tpLpL);
        tpLpR += lpc * (r - tpLpR);
        L[i] = tpLpL;
        R[i] = tpLpR;
    }
}

// half-speed capture: records one window, plays the previous one back at half speed
void RiverrProcessor::processHalftime (float* L, float* R, int n, double bpm, bool hostSync, double startPpq)
{
    const double lb = kHtBeats[jlimit (0, 3, (int) pHtLen->load())];
    const int maxLen = (int) htBuf[0][0].size();
    const int Lw = jlimit (512, maxLen - 4, (int) (lb * 60.0 / bpm * fs));

    if (Lw != htLenPrev) { htLenPrev = Lw; htPos = 0; htRead = 0.0; }

    if (hostSync)
    {
        double frac = startPpq / lb;  frac -= std::floor (frac);
        const double expected = frac * (double) Lw;
        const double diff = std::abs ((double) htPos - expected);
        if (diff > (double) Lw / 32.0 && diff < (double) Lw - (double) Lw / 32.0)
        {
            htPos = (int) expected;
            htRead = expected * 0.5;
        }
    }

    const float mix = e.htMix, flux = pHtFlux->load();
    const float fc = jlimit (800.f, (float) (fs * 0.45), 1500.f * std::pow (11.f, pHtTone->load()));
    const float lpc = 1.f - std::exp (-MathConstants<float>::twoPi * fc / (float) fs);
    const int fadeN = jmax (1, jmin (256, Lw / 8));

    for (int i = 0; i < n; ++i)
    {
        const int rec = htRec, play = 1 - htRec;
        htBuf[rec][0][(size_t) htPos] = L[i];
        htBuf[rec][1][(size_t) htPos] = R[i];

        const double rp = jmin (htRead, (double) (Lw - 2));
        const int i0 = (int) rp;
        const float fr = (float) (rp - (double) i0);
        float wl = htBuf[play][0][(size_t) i0] * (1.f - fr) + htBuf[play][0][(size_t) i0 + 1] * fr;
        float wr = htBuf[play][1][(size_t) i0] * (1.f - fr) + htBuf[play][1][(size_t) i0 + 1] * fr;

        const float fade = jmin (1.f, (float) htPos / (float) fadeN) * jmin (1.f, (float) (Lw - 1 - htPos) / (float) fadeN);
        wl *= fade;  wr *= fade;
        htLpL += lpc * (wl - htLpL);
        htLpR += lpc * (wr - htLpR);

        L[i] = L[i] * (1.f - mix) + htLpL * mix;
        R[i] = R[i] * (1.f - mix) + htLpR * mix;

        htFluxPh += 0.7 / fs;  htFluxPh -= std::floor (htFluxPh);
        htRead += 0.5 * (1.0 + (double) flux * 0.08 * std::sin (MathConstants<double>::twoPi * htFluxPh));

        if (++htPos >= Lw) { htPos = 0; htRead = 0.0; htRec ^= 1; }
    }
}

void RiverrProcessor::processDelay (float* L, float* R, int n, double bpm)
{
    const float mix = e.dlyMix, fb = pDlyFb->load(), drift = pDlyDrift->load();
    const int dlSize = (int) dlL.size();

    dlyPh += 0.31 * (double) n / fs;  dlyPh -= std::floor (dlyPh);
    const float wob = 1.f + drift * (0.012f * std::sin (MathConstants<float>::twoPi * (float) dlyPh)
                                     + 0.006f * std::sin (MathConstants<float>::twoPi * (float) dlyPh * 3.7f));
    const float target = jlimit (64.f, (float) (dlSize - 8),
                                 (float) (kDelayBeats[jlimit (0, 3, (int) pDlyTime->load())] * 60.0 / bpm * fs) * wob);
    if (dlSmooth <= 0.f) dlSmooth = target;

    const float fc = jlimit (300.f, (float) (fs * 0.45), 800.f * std::pow (15.f, pDlyTone->load()));
    const float lpCoef = 1.f - std::exp (-MathConstants<float>::twoPi * fc / (float) fs);

    auto readInterp = [&] (const std::vector<float>& b, float d)
    {
        float rp = (float) dlW - d;
        while (rp < 0.f) rp += (float) dlSize;
        const int i0 = (int) rp;
        const float f = rp - (float) i0;
        return b[(size_t) (i0 % dlSize)] * (1.f - f) + b[(size_t) ((i0 + 1) % dlSize)] * f;
    };

    for (int i = 0; i < n; ++i)
    {
        dlSmooth += (target - dlSmooth) * 0.0008f;
        const float dl = readInterp (dlL, dlSmooth);
        const float dr = readInterp (dlR, dlSmooth);
        dlLpL += lpCoef * (dl - dlLpL);
        dlLpR += lpCoef * (dr - dlLpR);

        const float in = 0.5f * (L[i] + R[i]);
        dlL[(size_t) dlW] = in + fb * dlLpR;
        dlR[(size_t) dlW] = fb * dlLpL;
        dlW = (dlW + 1) % dlSize;

        L[i] += mix * dl;
        R[i] += mix * dr;
    }
}

void RiverrProcessor::processReverb (float* L, float* R, int n)
{
    if ((int) revL.size() < n) { revL.resize ((size_t) n); revR.resize ((size_t) n); }
    const int preSize = (int) preL.size();
    const int preS = jlimit (0, preSize - 1, (int) (pRevPre->load() * 0.001f * (float) fs));

    for (int i = 0; i < n; ++i)
    {
        preL[(size_t) preW] = L[i];
        preR[(size_t) preW] = R[i];
        int rp = preW - preS;
        if (rp < 0) rp += preSize;
        revL[(size_t) i] = preL[(size_t) rp];
        revR[(size_t) i] = preR[(size_t) rp];
        preW = (preW + 1) % preSize;
    }

    Reverb::Parameters rp;
    rp.roomSize = e.revSize;
    rp.damping = e.revDamp;
    rp.wetLevel = 0.35f;
    rp.dryLevel = 0.f;
    rp.width = pRevWidth->load();
    rp.freezeMode = 0.f;
    reverb.setParameters (rp);
    reverb.processStereo (revL.data(), revR.data(), n);

    const float mix = e.revMix;
    for (int i = 0; i < n; ++i)
    {
        L[i] += mix * revL[(size_t) i];
        R[i] += mix * revR[(size_t) i];
    }
}

//==============================================================================
void RiverrProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    updateEffective();

    {
        SpinLock::ScopedTryLockType l (sampleLock);
        if (l.isLocked()) { audioSample[0] = sharedSample[0]; audioSample[1] = sharedSample[1]; }
    }

    double bpm = 120.0;
    bool playing = false;
    Optional<double> ppq;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            playing = pos->getIsPlaying();
            ppq = pos->getPpqPosition();
        }
    }
    if (bpm < 40.0) bpm = 120.0;

    if (e.evolve <= 0.f) evo.fill (0);

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int pos = jlimit (0, jmax (0, n - 1), meta.samplePosition);

        if (m.isNoteOn())            handleNoteOn (m.getNoteNumber(), m.getFloatVelocity(), pos);
        else if (m.isNoteOff())      handleNoteOff (m.getNoteNumber(), pos);
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            held.clear();
            for (auto& v : voices) if (v.active && v.releaseCountdown < 0) v.releaseCountdown = 0;
        }
    }
    midi.clear();

    const bool arpOn = pArpOn->load() > 0.5f;
    const double beatsPerSample = bpm / 60.0 / fs;
    const bool hostSync = playing && ppq.hasValue();
    const double startPpq = hostSync ? *ppq : freePpq;
    const double endPpq = startPpq + (double) n * beatsPerSample;

    if (arpOn)
    {
        const double stepLen = kRateBeats[jlimit (0, 7, (int) pRate->load())];

        if (held.empty())
        {
            haveLast = false;
            if (pending.empty()) for (auto& s : laneStep) s.store (-1);
        }
        else
        {
            rebuildSequence();

            const bool cont = haveLast && std::abs (stepLen - lastStepLen) < 1e-12 && hostSync == lastHostSync
                              && std::abs (startPpq - lastEndPpq) < 0.02;
            long long k = cont ? lastK + 1 : (long long) std::ceil (startPpq / stepLen - 1e-6);

            for (;; ++k)
            {
                const double t = (double) k * stepLen;
                if (t >= endPpq - 1e-9) break;
                scheduleStep (k, t, stepLen);
                lastK = k;
                haveLast = true;
            }

            lastStepLen = stepLen;
            lastEndPpq = endPpq;
            lastHostSync = hostSync;
        }
    }

    for (size_t i = 0; i < pending.size();)
    {
        const auto ev = pending[i];
        if (ev.when < endPpq)
        {
            if (ev.when >= startPpq - 4.0)
            {
                int off = (int) std::llround (jmax (0.0, ev.when - startPpq) / beatsPerSample);
                off = jlimit (0, jmax (0, n - 1), off);
                const int gate = jmax (1, (int) (ev.gateBeats / beatsPerSample));
                fireHit (-1, (float) ev.semis, ev.vel, off, gate, ev.group);
            }
            pending.erase (pending.begin() + (std::ptrdiff_t) i);
        }
        else if (ev.when > endPpq + 8.0)
            pending.erase (pending.begin() + (std::ptrdiff_t) i);
        else
            ++i;
    }

    freePpq = hostSync ? endPpq : freePpq + (double) n * beatsPerSample;

    // tape wow / flutter as a per-block pitch multiplier (interpolated across the block)
    double wow = 1.0;
    if (e.wowOn)
    {
        wowPh1 += 0.31 * (double) n / fs;  wowPh1 -= std::floor (wowPh1);
        wowPh2 += 1.13 * (double) n / fs;  wowPh2 -= std::floor (wowPh2);
        wowPh3 += 6.3 * (double) n / fs;   wowPh3 -= std::floor (wowPh3);
        wow = 1.0 + (double) e.drift * (0.004 * std::sin (MathConstants<double>::twoPi * wowPh1)
                                        + 0.0018 * std::sin (MathConstants<double>::twoPi * wowPh2))
                  + (double) pTapeFlutter->load() * 0.0012 * std::sin (MathConstants<double>::twoPi * wowPh3);
    }
    const double rmA = prevWow;
    prevWow = wow;

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    renderVoices (L, R, n, rmA, wow);

    if (pFltOn->load() > 0.5f) processFilter (L, R, n);
    if (e.drvOn)  processDrive (buffer, n);
    if (e.tapeOn) processTape (L, R, n);
    if (e.htOn)   processHalftime (L, R, n, bpm, hostSync, startPpq);
    if (e.dlyOn)  processDelay (L, R, n, bpm);
    if (e.revOn)  processReverb (L, R, n);

    buffer.applyGain (Decibels::decibelsToGain (pGain->load()));

    // gentle safety limiter: untouched below 0.8, soft knee up to 1.0
    for (int ch = 0; ch < 2; ++ch)
    {
        float* d = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
        {
            const float a = std::abs (d[i]);
            if (a > 0.8f)
                d[i] = std::copysign (0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f), d[i]);
        }
    }
}

//==============================================================================
std::shared_ptr<const SampleData> RiverrProcessor::getSample (int slot) const
{
    const SpinLock::ScopedLockType l (sampleLock);
    return sharedSample[(size_t) jlimit (0, 1, slot)];
}

bool RiverrProcessor::loadSample (const File& f, int slot)
{
    slot = jlimit (0, 1, slot);

    AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<AudioFormatReader> reader (fm.createReaderFor (f));
    if (reader == nullptr || reader->lengthInSamples < 16)
        return false;

    const int len = (int) std::min<int64> (reader->lengthInSamples, (int64) (reader->sampleRate * 30.0));

    auto s = std::make_shared<SampleData>();
    s->audio.setSize (2, len);
    s->audio.clear();
    reader->read (&s->audio, 0, len, 0, true, true);
    if (reader->numChannels < 2)
        s->audio.copyFrom (1, 0, s->audio, 0, 0, len);

    s->sr = reader->sampleRate;
    s->file = f;
    s->name = f.getFileNameWithoutExtension();

    std::vector<float> mono ((size_t) len);
    const float* a = s->audio.getReadPointer (0);
    const float* b = s->audio.getReadPointer (1);
    float peak = 1e-9f;
    for (int i = 0; i < len; ++i)
    {
        mono[(size_t) i] = 0.5f * (a[i] + b[i]);
        peak = jmax (peak, std::abs (mono[(size_t) i]));
    }

    const int cols = 1000;
    s->peakMin.assign ((size_t) cols, 0.f);
    s->peakMax.assign ((size_t) cols, 0.f);
    for (int c = 0; c < cols; ++c)
    {
        const int i0 = (int) ((int64) c * len / cols);
        const int i1 = jmin (len, jmax (i0 + 1, (int) ((int64) (c + 1) * len / cols)));
        float mn = 0.f, mx = 0.f;
        for (int i = i0; i < i1; ++i)
        {
            mn = jmin (mn, mono[(size_t) i]);
            mx = jmax (mx, mono[(size_t) i]);
        }
        s->peakMin[(size_t) c] = mn / peak;
        s->peakMax[(size_t) c] = mx / peak;
    }

    const auto pr = detectSamplePitch (mono, s->sr);
    if (pr.ok)
    {
        const float nearestC = std::round (pr.midi / 12.f) * 12.f;
        s->pitched = true;
        s->detectedMidi = pr.midi;
        s->rootShift = nearestC - pr.midi;
        s->info = "Detected " + MidiMessage::getMidiNoteName ((int) std::round (pr.midi), true, true, 5)
                  + " (" + String (pr.hz, 1) + " Hz)  ->  tuned "
                  + (s->rootShift >= 0.f ? "+" : "") + String (s->rootShift, 2) + " st to "
                  + MidiMessage::getMidiNoteName ((int) nearestC, true, true, 5) + "  =  root C5";
    }
    else
    {
        s->info = "No clear pitch found - original tuning kept (use TUNE to fix by ear)";
    }

    {
        const SpinLock::ScopedLockType l (sampleLock);
        sharedSample[(size_t) slot] = s;
    }

    apvts.state.setProperty (slot == 0 ? "samplePath" : "samplePathB", f.getFullPathName(), nullptr);
    sendChangeMessage();
    return true;
}

//==============================================================================
void RiverrProcessor::restoreSamplesFromState()
{
    for (int slot = 0; slot < 2; ++slot)
    {
        const String path = apvts.state.getProperty (slot == 0 ? "samplePath" : "samplePathB").toString();
        if (path.isEmpty()) continue;

        const File f (path);
        auto cur = getSample (slot);
        if (f.existsAsFile() && (cur == nullptr || cur->file != f))
            loadSample (f, slot);
    }
}

void RiverrProcessor::getStateInformation (MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void RiverrProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (ValueTree::fromXml (*xml));
            restoreSamplesFromState();
            sendChangeMessage();
        }
}

//==============================================================================
File RiverrProcessor::getPresetDir()
{
    auto d = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("RIVERR").getChildFile ("Presets");
    d.createDirectory();
    return d;
}

StringArray RiverrProcessor::listPresets() const
{
    StringArray names;
    for (auto& f : getPresetDir().findChildFiles (File::findFiles, false, "*.riverr"))
        names.add (f.getFileNameWithoutExtension());
    names.sort (true);
    return names;
}

bool RiverrProcessor::savePreset (const String& name)
{
    const auto f = getPresetDir().getChildFile (File::createLegalFileName (name) + ".riverr");
    if (auto xml = apvts.copyState().createXml())
        return xml->writeTo (f);
    return false;
}

bool RiverrProcessor::loadPreset (const String& name)
{
    const auto f = getPresetDir().getChildFile (name + ".riverr");
    if (auto xml = XmlDocument::parse (f))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (ValueTree::fromXml (*xml));
            restoreSamplesFromState();
            sendChangeMessage();
            return true;
        }
    return false;
}

void RiverrProcessor::setParamValue (const String& id, float realValue)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
        p->endChangeGesture();
    }
}

void RiverrProcessor::randomizeSteps()
{
    static const int pool[] = { 0, 0, 0, 3, 7, 10, 12, -5, -12, 5, 8, -2 };
    auto& r = Random::getSystemRandom();

    for (int i = 0; i < 16; ++i)
    {
        setParamValue (stepId (i, "on"), r.nextFloat() < 0.8f ? 1.f : 0.f);
        setParamValue (stepId (i, "pit"), (float) pool[r.nextInt ((int) (sizeof (pool) / sizeof (int)))]);
        setParamValue (stepId (i, "vel"), 0.45f + 0.55f * r.nextFloat());
        setParamValue (stepId (i, "gate"), 0.35f + 0.65f * r.nextFloat());
        setParamValue (stepId (i, "prob"), r.nextFloat() < 0.15f ? 0.6f : 1.f);
        const float rr = r.nextFloat();
        setParamValue (stepId (i, "rat"), rr < 0.8f ? 1.f : (rr < 0.95f ? 2.f : 3.f));
    }

    for (const char* id : { "lenPit", "lenVel", "lenGate", "lenRat" })
        setParamValue (id, r.nextFloat() < 0.5f ? (float) (3 + r.nextInt (13)) : 16.f);
}

// a musical trap-style pattern generator: accents, rests, rolls at the end of bars
void RiverrProcessor::diceHard()
{
    static const int tpl[][16] = {
        { 1,0,1,1, 0,1,1,0, 1,0,1,1, 0,1,0,1 },
        { 1,1,0,1, 1,0,1,1, 0,1,1,0, 1,1,0,1 },
        { 1,0,0,1, 0,1,0,1, 1,0,1,0, 0,1,1,0 },
        { 1,1,0,1, 0,1,0,0, 1,1,0,1, 0,1,0,1 },
        { 1,0,1,0, 1,1,0,1, 1,0,1,0, 1,1,0,0 } };
    static const int pool[] = { 0, 0, 0, 0, 7, 7, 12, 12, 3, 10, -5, 5, 8, -12 };
    static const float accent[] = { 1.f, 0.55f, 0.78f, 0.6f };

    auto& r = Random::getSystemRandom();
    const auto& t = tpl[r.nextInt (5)];

    for (int i = 0; i < 16; ++i)
    {
        setParamValue (stepId (i, "on"), (float) t[i]);
        setParamValue (stepId (i, "pit"), (float) pool[r.nextInt ((int) (sizeof (pool) / sizeof (int)))]);
        setParamValue (stepId (i, "vel"), jlimit (0.2f, 1.f, accent[i % 4] * (0.82f + 0.18f * r.nextFloat())));
        setParamValue (stepId (i, "gate"), 0.35f + 0.55f * r.nextFloat());
        setParamValue (stepId (i, "prob"), 1.f);
        const bool endOfHalf = (i % 8) == 7;
        const float rr = r.nextFloat();
        setParamValue (stepId (i, "rat"), endOfHalf ? (rr < 0.45f ? 2.f : (rr < 0.6f ? 3.f : 1.f)) : (rr < 0.08f ? 2.f : 1.f));
    }

    static const int lens[] = { 16, 16, 7, 9, 5, 6 };
    setParamValue ("lenPit", (float) lens[r.nextInt (6)]);
    setParamValue ("lenVel", 16.f);
    setParamValue ("lenGate", r.nextFloat() < 0.4f ? (float) lens[2 + r.nextInt (4)] : 16.f);
    setParamValue ("lenRat", 16.f);
    if ((int) pScale->load() == 0) setParamValue ("scale", 3.f);
}

void RiverrProcessor::resetToDefault() { applyFactoryPreset (0); }

void RiverrProcessor::applyFactoryPreset (int idx)
{
    // everything back to defaults first (the loaded sounds stay)
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (p))
        {
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->getDefaultValue());
            rp->endChangeGesture();
        }

    auto set = [this] (std::initializer_list<KV> l) { for (auto& kv : l) setParamValue (kv.id, kv.v); };
    auto pattern = [this] (const int* on, const int* pit, const float* vel, const float* gate, const int* rat)
    {
        for (int i = 0; i < 16; ++i)
        {
            setParamValue (stepId (i, "on"), (float) on[i]);
            setParamValue (stepId (i, "pit"), (float) pit[i]);
            setParamValue (stepId (i, "vel"), vel[i]);
            setParamValue (stepId (i, "gate"), gate[i]);
            setParamValue (stepId (i, "prob"), 1.f);
            setParamValue (stepId (i, "rat"), (float) rat[i]);
        }
    };

    switch (idx)
    {
        default:
        case 0:   // HARD BELL - also the default state of the plugin
        {
            static const int on[16]  = { 1,0,1,1, 0,1,0,1, 1,0,1,1, 0,1,1,0 };
            static const int pit[16] = { 0,0,7,0, 0,12,0,7, 0,0,3,0, 0,7,12,0 };
            static const float vel[16]  = { 1.f,.55f,.8f,.6f, .5f,.85f,.5f,.7f, 1.f,.5f,.8f,.6f, .5f,.85f,.9f,.5f };
            static const float gate[16] = { .6f,.5f,.7f,.5f, .5f,.8f,.4f,.6f, .6f,.5f,.7f,.5f, .5f,.8f,.9f,.4f };
            static const int rat[16] = { 1,1,1,1, 1,1,1,2, 1,1,1,1, 1,1,1,3 };
            set ({ {"dir",0}, {"octRange",1}, {"rate",5}, {"gate",0.32f}, {"swing",0.06f}, {"scale",3}, {"key",0},
                   {"evolve",0.15f}, {"jump",0.1f}, {"layer",1}, {"layerLvl",0.35f},
                   {"atk",0.001f}, {"dec",0.5f}, {"sus",1.f}, {"rel",0.4f}, {"cut",8000}, {"choke",0.55f}, {"duck",0.35f},
                   {"drvOn",1}, {"drvType",0}, {"drvAmt",0.4f}, {"drvTone",0.7f}, {"drvMix",0.55f},
                   {"tapeOn",1}, {"tapeNoise",0.12f}, {"drift",0.25f}, {"tapeFlutter",0.15f}, {"tapeSat",0.3f}, {"tapeTone",0.75f},
                   {"dlyOn",1}, {"dlyTime",1}, {"dlyFb",0.45f}, {"dlyMix",0.28f}, {"dlyTone",0.45f},
                   {"revOn",1}, {"revSize",0.88f}, {"revDamp",0.55f}, {"revMix",0.42f}, {"revPre",18} });
            pattern (on, pit, vel, gate, rat);
            break;
        }
        case 1:   // DARK PLUCK
        {
            static const int on[16]  = { 1,1,0,1, 1,0,1,1, 0,1,1,0, 1,1,0,1 };
            static const int pit[16] = { 0,3,0,7, 0,0,12,0, 0,5,0,7, 3,0,0,10 };
            static const float vel[16]  = { 1.f,.6f,.5f,.8f, .7f,.5f,.9f,.6f, .5f,.8f,.6f,.5f, .9f,.6f,.5f,.8f };
            static const float gate[16] = { .4f,.3f,.3f,.5f, .4f,.3f,.6f,.3f, .3f,.5f,.4f,.3f, .6f,.3f,.3f,.5f };
            static const int rat[16] = { 1,1,1,1, 1,1,1,1, 1,1,1,2, 1,1,1,3 };
            set ({ {"dir",2}, {"octRange",1}, {"rate",6}, {"gate",0.25f}, {"scale",2}, {"evolve",0.25f}, {"jump",0.2f},
                   {"layer",2}, {"layerLvl",0.3f}, {"atk",0.001f}, {"dec",0.35f}, {"sus",1.f}, {"rel",0.3f},
                   {"choke",0.8f}, {"duck",0.25f}, {"glide",0.05f},
                   {"drvOn",1}, {"drvType",1}, {"drvAmt",0.5f}, {"drvMix",0.45f},
                   {"tapeOn",1}, {"tapeNoise",0.18f}, {"drift",0.3f}, {"tapeFlutter",0.2f},
                   {"dlyOn",1}, {"dlyTime",2}, {"dlyFb",0.5f}, {"dlyMix",0.3f},
                   {"revOn",1}, {"revSize",0.8f}, {"revDamp",0.65f}, {"revMix",0.35f} });
            pattern (on, pit, vel, gate, rat);
            break;
        }
        case 2:   // HALFTIME GHOST
        {
            static const int on[16]  = { 1,0,0,1, 0,0,1,0, 0,1,0,0, 1,0,0,0 };
            static const int pit[16] = { 0,0,0,7, 0,0,12,0, 0,3,0,0, 10,0,0,0 };
            static const float vel[16]  = { 1.f,.5f,.5f,.8f, .5f,.5f,.9f,.5f, .5f,.7f,.5f,.5f, .85f,.5f,.5f,.5f };
            static const float gate[16] = { 1.f,.6f,.6f,1.f, .6f,.6f,1.f,.6f, .6f,1.f,.6f,.6f, 1.f,.6f,.6f,.6f };
            static const int rat[16] = { 1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1 };
            set ({ {"dir",5}, {"octRange",1}, {"rate",2}, {"gate",1.6f}, {"scale",4}, {"evolve",0.5f}, {"jump",0.15f},
                   {"layer",2}, {"layerLvl",0.5f}, {"atk",0.002f}, {"dec",0.6f}, {"sus",1.f}, {"rel",1.2f},
                   {"choke",0.2f}, {"duck",0.5f}, {"glide",0.2f},
                   {"tapeOn",1}, {"tapeNoise",0.22f}, {"drift",0.5f}, {"tapeFlutter",0.3f},
                   {"htOn",1}, {"htLen",2}, {"htMix",0.5f}, {"htTone",0.5f}, {"htFlux",0.25f},
                   {"hsOn",1}, {"hsAmt",0.35f}, {"hsDive",0.4f}, {"hsSpeed",0.5f},
                   {"dlyOn",1}, {"dlyTime",1}, {"dlyFb",0.55f}, {"dlyMix",0.35f}, {"dlyTone",0.35f}, {"dlyDrift",0.3f},
                   {"revOn",1}, {"revSize",0.95f}, {"revDamp",0.5f}, {"revMix",0.6f}, {"revPre",40} });
            pattern (on, pit, vel, gate, rat);
            break;
        }
        case 3:   // SLIDE MONO
        {
            static const int on[16]  = { 1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1 };
            static const int pit[16] = { 0,0,3,3, 7,7,10,12, 10,7,7,3, 3,0,0,-5 };
            static const float vel[16]  = { 1.f,.7f,.8f,.7f, .9f,.7f,.8f,1.f, .9f,.7f,.8f,.7f, .8f,.7f,.8f,.7f };
            static const float gate[16] = { 1.f,1.f,1.f,1.f, 1.f,1.f,1.f,1.f, 1.f,1.f,1.f,1.f, 1.f,1.f,1.f,1.f };
            static const int rat[16] = { 1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1 };
            set ({ {"dir",2}, {"octRange",1}, {"rate",5}, {"gate",1.2f}, {"scale",1}, {"jump",0.3f},
                   {"atk",0.003f}, {"dec",0.6f}, {"sus",1.f}, {"rel",0.5f}, {"choke",1.f}, {"glide",0.55f},
                   {"drvOn",1}, {"drvType",0}, {"drvAmt",0.5f}, {"drvMix",0.5f},
                   {"dlyOn",1}, {"dlyTime",1}, {"dlyMix",0.25f}, {"revOn",1}, {"revSize",0.8f}, {"revMix",0.4f} });
            pattern (on, pit, vel, gate, rat);
            break;
        }
        case 4:   // CRUSHED ROLL
        {
            static const int on[16]  = { 1,1,0,1, 1,1,0,1, 1,0,1,1, 1,1,1,0 };
            static const int pit[16] = { 0,0,0,7, 0,12,0,3, 0,0,5,0, 7,0,12,0 };
            static const float vel[16]  = { 1.f,.6f,.5f,.8f, .7f,.9f,.5f,.7f, 1.f,.5f,.8f,.6f, .8f,.6f,.9f,.5f };
            static const float gate[16] = { .5f,.4f,.4f,.6f, .5f,.6f,.4f,.5f, .5f,.4f,.6f,.4f, .6f,.4f,.7f,.4f };
            static const int rat[16] = { 1,1,1,2, 1,1,1,2, 1,1,1,2, 1,1,2,3 };
            set ({ {"dir",0}, {"octRange",2}, {"rate",7}, {"gate",0.5f}, {"scale",7}, {"evolve",0.3f},
                   {"atk",0.001f}, {"dec",0.3f}, {"sus",1.f}, {"rel",0.25f}, {"choke",0.9f}, {"duck",0.5f},
                   {"drvOn",1}, {"drvType",2}, {"drvAmt",0.55f}, {"drvTone",0.6f}, {"drvMix",0.7f},
                   {"tapeOn",1}, {"tapeNoise",0.35f}, {"drift",0.4f}, {"tapeSat",0.5f},
                   {"dlyOn",0}, {"revOn",1}, {"revSize",0.5f}, {"revMix",0.25f} });
            pattern (on, pit, vel, gate, rat);
            break;
        }
    }
    sendChangeMessage();
}

//==============================================================================
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RiverrProcessor(); }
