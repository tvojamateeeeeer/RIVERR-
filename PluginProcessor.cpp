#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

namespace
{
    constexpr int kRootNote = 60;   // FL Studio "C5" == MIDI note 60
    constexpr double kRateBeats[]  = { 1.0, 0.75, 0.5, 1.0 / 3.0, 0.375, 0.25, 1.0 / 6.0, 0.125 };
    constexpr double kDelayBeats[] = { 1.0, 0.75, 0.5, 0.25 };
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

    // sound
    F ("gain", "Gain", -30.f, 6.f, 0.f, 0.1f, -1.f, "dB");
    I ("octave", "Octave", -3, 3, 0);
    F ("tune", "Tune", -12.f, 12.f, 0.f, 0.01f, -1.f, "st");
    B ("autotune", "Auto C5", true);
    B ("reverse", "Reverse", false);
    F ("start", "Start", 0.f, 1.f, 0.f);
    F ("end", "End", 0.f, 1.f, 1.f);
    F ("atk", "Attack", 0.001f, 2.f, 0.003f, 0.f, 0.1f, "s");
    F ("dec", "Decay", 0.01f, 4.f, 0.4f, 0.f, 0.5f, "s");
    F ("sus", "Sustain", 0.f, 1.f, 1.f);
    F ("rel", "Release", 0.01f, 8.f, 0.5f, 0.f, 1.f, "s");
    F ("scan", "Scan", 0.f, 1.f, 0.f);
    F ("drift", "Drift", 0.f, 1.f, 0.f);
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

    // fx
    F ("cut", "Cutoff", 20.f, 20000.f, 9000.f, 0.f, 1500.f, "Hz");
    F ("res", "Resonance", 0.3f, 2.f, 0.7f);
    F ("lfoRate", "LFO Rate", 0.05f, 10.f, 0.4f, 0.f, 1.f, "Hz");
    F ("lfoDepth", "LFO Depth", 0.f, 1.f, 0.f);
    C ("dlyTime", "Delay Time", { "1/4", "1/8.", "1/8", "1/16" }, 1);
    F ("dlyFb", "Delay Feedback", 0.f, 0.95f, 0.45f);
    F ("dlyMix", "Delay Mix", 0.f, 1.f, 0.25f);
    F ("revSize", "Reverb Size", 0.f, 1.f, 0.85f);
    F ("revDamp", "Reverb Damp", 0.f, 1.f, 0.6f);
    F ("revMix", "Reverb Mix", 0.f, 1.f, 0.4f);

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
    pAtk = P ("atk");         pDec = P ("dec");         pSus = P ("sus");   pRel = P ("rel");
    pArpOn = P ("arpOn");     pDir = P ("dir");         pOctRange = P ("octRange");
    pRate = P ("rate");       pGate = P ("gate");       pSwing = P ("swing");
    pHumT = P ("humT");       pHumV = P ("humV");       pSteps = P ("steps");
    pKey = P ("key");         pScale = P ("scale");     pEvolve = P ("evolve");  pJump = P ("jump");
    pLayer = P ("layer");     pLayerLvl = P ("layerLvl");
    pLenPit = P ("lenPit");   pLenVel = P ("lenVel");   pLenGate = P ("lenGate");
    pLenProb = P ("lenProb"); pLenRat = P ("lenRat");
    pScan = P ("scan");       pDrift = P ("drift");     pSpread = P ("spread");
    pCut = P ("cut");         pRes = P ("res");         pLfoRate = P ("lfoRate"); pLfoDepth = P ("lfoDepth");
    pDlyTime = P ("dlyTime"); pDlyFb = P ("dlyFb");     pDlyMix = P ("dlyMix");
    pRevSize = P ("revSize"); pRevDamp = P ("revDamp"); pRevMix = P ("revMix");

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

    dsp::ProcessSpec spec { sampleRate, (uint32) samplesPerBlock, 2 };
    filter.prepare (spec);
    filter.setType (dsp::StateVariableTPTFilterType::lowpass);
    filter.reset();

    const int dlSize = (int) (sampleRate * 4.0) + 8;
    dlL.assign ((size_t) dlSize, 0.f);
    dlR.assign ((size_t) dlSize, 0.f);
    dlW = 0; dlSmooth = 0.f; dlLpL = dlLpR = 0.f;

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
        triggerVoice (note, (float) (note - kRootNote), vel, pos, -1);
    }
}

void RiverrProcessor::handleNoteOff (int note, int pos)
{
    held.erase (std::remove (held.begin(), held.end(), note), held.end());

    for (auto& v : voices)
        if (v.active && v.note == note && v.releaseCountdown < 0)
            v.releaseCountdown = jmax (0, pos - v.delay);
}

void RiverrProcessor::triggerVoice (int note, float semis, float vel, int delay, int gateSamples)
{
    if (audioSample == nullptr)
        return;

    Voice* v = nullptr;
    for (auto& x : voices)
        if (! x.active) { v = &x; break; }
    if (v == nullptr)
    {
        v = &voices[0];
        for (auto& x : voices)
            if (x.age < v->age) v = &x;
    }

    const auto& s = *audioSample;
    const int len = s.audio.getNumSamples();

    const float st = pStart->load();
    float en = pEnd->load();
    if (en < st + 0.005f) en = jmin (1.f, st + 0.005f);

    v->startS = st * (float) len;
    v->endS = jmax (v->startS + 16.f, jmin ((float) (len - 1), en * (float) len));
    v->reverse = pReverse->load() > 0.5f;

    // "scan": every hit starts at a slightly different place in the sound
    const float scan = pScan->load();
    const float off = scan > 0.f ? rng.nextFloat() * scan * 0.5f * (v->endS - v->startS) : 0.f;
    v->pos = v->reverse ? (double) v->endS - 1.0 - (double) off : (double) v->startS + (double) off;

    // "drift": tiny random detune per hit (global wow is applied while rendering)
    const float drift = pDrift->load();
    const double cents = drift > 0.f ? (double) ((rng.nextFloat() * 2.f - 1.f) * drift * 18.f) : 0.0;

    const float total = semis + 12.f * pOctave->load() + pTune->load()
                        + (pAutoTune->load() > 0.5f ? s.rootShift : 0.f);
    v->rate = std::pow (2.0, ((double) total + cents / 100.0) / 12.0) * s.sr / fs;

    // "spread": random pan per hit (equal power)
    const float sp = pSpread->load();
    const float pan = sp > 0.f ? (rng.nextFloat() * 2.f - 1.f) * sp : 0.f;
    const float th = (pan + 1.f) * MathConstants<float>::pi * 0.25f;
    v->panL = std::cos (th) * 1.41421356f;
    v->panR = std::sin (th) * 1.41421356f;

    v->smp = audioSample;
    v->vel = vel;
    v->delay = jmax (0, delay);
    v->releaseCountdown = gateSamples > 0 ? gateSamples : -1;
    v->note = note;
    v->played = 0;

    v->adsr.setSampleRate (fs);
    v->adsr.setParameters ({ pAtk->load(), pDec->load(), pSus->load(), pRel->load() });
    v->adsr.noteOn();
    v->active = true;
    v->age = ++ageCounter;
}

void RiverrProcessor::renderVoices (float* L, float* R, int n, double rateMul)
{
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

            const int i0 = (int) v.pos;
            const float fr = (float) (v.pos - (double) i0);
            const int a = jlimit (0, len - 1, i0 - 1), b = jlimit (0, len - 1, i0),
                      c = jlimit (0, len - 1, i0 + 1), d = jlimit (0, len - 1, i0 + 2);

            const float sl = hermite (fr, d0[a], d0[b], d0[c], d0[d]);
            const float sr = hermite (fr, d1[a], d1[b], d1[c], d1[d]);

            const float dist = v.reverse ? (float) (v.pos - (double) v.startS)
                                         : (float) ((double) v.endS - v.pos);
            const float fade = jmin (1.f, dist / 128.f) * jmin (1.f, (float) (++v.played) / 12.f);
            const float g = env * v.vel * fade;

            L[i] += sl * g * v.panL;
            R[i] += sr * g * v.panR;

            v.pos += (v.reverse ? -v.rate : v.rate) * rateMul;
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
        case 1:  // down
            for (int i = n - 1; i >= 0; --i) seq.push_back (seqTmp[(size_t) i]);
            break;
        case 2:  // up-down
            seq = seqTmp;
            for (int i = n - 2; i >= 1; --i) seq.push_back (seqTmp[(size_t) i]);
            break;
        case 3:  // down-up
            for (int i = n - 1; i >= 0; --i) seq.push_back (seqTmp[(size_t) i]);
            for (int i = 1; i <= n - 2; ++i) seq.push_back (seqTmp[(size_t) i]);
            break;
        default: // up, random, as played
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

    // evolve: the pitch lane slowly wanders (random walk that pulls back to the origin)
    const float evolve = pEvolve->load();
    if (evolve > 0.f && rng.nextFloat() < evolve * 0.45f)
    {
        int d = rng.nextBool() ? 1 : -1;
        if (rng.nextFloat() < 0.25f) d *= 2;
        int& e = evo[(size_t) iPit];
        e = jlimit (-12, 12, e + d);
        if (std::abs (e) > 6) e -= (e > 0 ? 1 : -1);
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

        if (pending.size() < 250)
            pending.push_back ({ when, note - kRootNote, v, gateBeats / (double) rat });
        if (layer > 0 && pending.size() < 250)
            pending.push_back ({ when + 0.0005, note - kRootNote + kLayerInt[layer], v * layerLvl, gateBeats / (double) rat });
    }
}

//==============================================================================
void RiverrProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();

    {
        SpinLock::ScopedTryLockType l (sampleLock);
        if (l.isLocked()) audioSample = sharedSample;
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
    if (bpm < 20.0) bpm = 120.0;

    if (pEvolve->load() <= 0.f) evo.fill (0);

    // ---- MIDI ----
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

    // ---- ARP ----
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
        const auto e = pending[i];
        if (e.when < endPpq)
        {
            if (e.when >= startPpq - 4.0)
            {
                int off = (int) std::llround (jmax (0.0, e.when - startPpq) / beatsPerSample);
                off = jlimit (0, jmax (0, n - 1), off);
                const int gate = jmax (1, (int) (e.gateBeats / beatsPerSample));
                triggerVoice (-1, (float) e.semis, e.vel, off, gate);
            }
            pending.erase (pending.begin() + (std::ptrdiff_t) i);
        }
        else if (e.when > endPpq + 8.0)
            pending.erase (pending.begin() + (std::ptrdiff_t) i);
        else
            ++i;
    }

    freePpq = hostSync ? endPpq : freePpq + (double) n * beatsPerSample;

    // ---- tape-ish wow (only when DRIFT > 0) ----
    double wow = 1.0;
    {
        const float dr = pDrift->load();
        if (dr > 0.f)
        {
            wowPh1 += 0.31 * (double) n / fs;  wowPh1 -= std::floor (wowPh1);
            wowPh2 += 1.13 * (double) n / fs;  wowPh2 -= std::floor (wowPh2);
            wow = 1.0 + (double) dr * (0.004 * std::sin (MathConstants<double>::twoPi * wowPh1)
                                       + 0.0018 * std::sin (MathConstants<double>::twoPi * wowPh2));
        }
    }

    // ---- voices ----
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    renderVoices (L, R, n, wow);

    // ---- filter + LFO (block of 32 samples) ----
    {
        const float cutBase = pCut->load(), lfoR = pLfoRate->load(), lfoD = pLfoDepth->load();
        const float maxF = (float) jmin (20000.0, fs * 0.45);
        filter.setResonance (pRes->load());

        for (int i = 0; i < n; i += 32)
        {
            const int len = jmin (32, n - i);
            const float mod = lfoD > 0.f ? std::sin (MathConstants<float>::twoPi * (float) lfoPhase) : 0.f;
            lfoPhase += (double) lfoR * (double) len / fs;
            lfoPhase -= std::floor (lfoPhase);

            filter.setCutoffFrequency (jlimit (20.f, maxF, cutBase * std::pow (2.f, mod * lfoD * 3.f)));
            for (int j = 0; j < len; ++j)
            {
                L[i + j] = filter.processSample (0, L[i + j]);
                R[i + j] = filter.processSample (1, R[i + j]);
            }
        }
    }

    // ---- ping-pong delay (dark, filtered feedback) ----
    {
        const float mix = pDlyMix->load(), fb = pDlyFb->load();
        const int dlSize = (int) dlL.size();
        const float target = jlimit (64.f, (float) (dlSize - 8),
                                     (float) (kDelayBeats[jlimit (0, 3, (int) pDlyTime->load())] * 60.0 / bpm * fs));
        if (dlSmooth <= 0.f) dlSmooth = target;
        const float lpCoef = 1.f - std::exp (-MathConstants<float>::twoPi * 3500.f / (float) fs);

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

    // ---- reverb ----
    {
        Reverb::Parameters rp;
        rp.roomSize = pRevSize->load();
        rp.damping = pRevDamp->load();
        rp.wetLevel = pRevMix->load() * 0.35f;
        rp.dryLevel = 0.5f;
        rp.width = 1.f;
        rp.freezeMode = 0.f;
        reverb.setParameters (rp);
        reverb.processStereo (L, R, n);
    }

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
std::shared_ptr<const SampleData> RiverrProcessor::getSample() const
{
    const SpinLock::ScopedLockType l (sampleLock);
    return sharedSample;
}

bool RiverrProcessor::loadSample (const File& f)
{
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

    // ---- auto-tune: find pitch, shift by the fewest semitones to land on a C ----
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
        sharedSample = s;
    }

    apvts.state.setProperty ("samplePath", f.getFullPathName(), nullptr);
    sendChangeMessage();
    return true;
}

//==============================================================================
void RiverrProcessor::restoreSampleFromState()
{
    const String path = apvts.state.getProperty ("samplePath").toString();
    if (path.isEmpty()) return;

    const File f (path);
    auto cur = getSample();
    if (f.existsAsFile() && (cur == nullptr || cur->file != f))
        loadSample (f);
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
            restoreSampleFromState();
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
            restoreSampleFromState();
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

    // polymeter: lanes get different lengths so the pattern takes ages to repeat
    for (const char* id : { "lenPit", "lenVel", "lenGate", "lenRat" })
        setParamValue (id, r.nextFloat() < 0.5f ? (float) (3 + r.nextInt (13)) : 16.f);
}

//==============================================================================
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RiverrProcessor(); }
