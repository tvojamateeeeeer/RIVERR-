#pragma once
#include <JuceHeader.h>
#include "PitchDetect.h"
#include <array>
#include <atomic>
#include <memory>
#include <vector>

// One loaded one-shot (immutable after creation, shared with the audio thread).
struct SampleData
{
    juce::AudioBuffer<float> audio;   // always stereo
    double sr = 44100.0;
    juce::File file;
    juce::String name;
    bool pitched = false;
    float detectedMidi = 0.f;
    float rootShift = 0.f;            // semitones needed to land exactly on the nearest C
    juce::String info;
    std::vector<float> peakMin, peakMax;   // normalised, for the waveform display
};

struct Voice
{
    bool active = false;
    std::shared_ptr<const SampleData> smp;
    double pos = 0.0, rate = 1.0, baseRatio = 1.0;
    float semisNow = 0.f, semisTarget = 0.f, glideCoef = 0.f, detune = 0.f;
    float startS = 0.f, endS = 0.f;
    bool reverse = false;
    float vel = 1.f;
    float panL = 1.f, panR = 1.f;
    int delay = 0;                 // samples until the voice starts (inside this block)
    int releaseCountdown = -1;     // samples (after start) until note-off, -1 = none
    int note = -1;                 // midi note when played without arp, -1 = arp voice
    int played = 0;
    int group = 0;                 // voices of one arp hit share a group (layers / A+B)
    float chokeGain = 1.f, chokeStep = 0.f, duckGain = 1.f;
    juce::ADSR adsr;
    juce::uint64 age = 0;
};

struct ArpEvent { double when; int semis; float vel; double gateBeats; int group; };
struct SeqNote  { int note; float vel; };

class RiverrProcessor : public juce::AudioProcessor,
                        public juce::ChangeBroadcaster
{
public:
    using APVTS = juce::AudioProcessorValueTreeState;

    RiverrProcessor();
    ~RiverrProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "RIVERR"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- sample / presets / GUI helpers (message thread) ---
    bool loadSample (const juce::File& f, int slot = 0);
    std::shared_ptr<const SampleData> getSample (int slot = 0) const;

    static juce::File getPresetDir();
    juce::StringArray listPresets() const;
    bool savePreset (const juce::String& name);
    bool loadPreset (const juce::String& name);
    void randomizeSteps();
    void diceHard();
    void applyFactoryPreset (int index);
    void resetToDefault();
    std::atomic<int> hitCount { 0 };

    // lanes: 0 on, 1 pitch, 2 velocity, 3 gate, 4 probability, 5 ratchet
    int getLaneLength (int lane) const;
    std::array<std::atomic<int>, 6> laneStep;

    APVTS apvts;

private:
    static APVTS::ParameterLayout createLayout();
    void setParamValue (const juce::String& id, float realValue);
    void restoreSamplesFromState();
    void updateEffective();

    // audio thread
    void fireHit (int note, float semis, float vel, int delay, int gateSamples, int group);
    void triggerVoice (int note, float semis, float vel, int delay, int gateSamples, int slot, int group, bool halfSpeed);
    void renderVoices (float* L, float* R, int n, double rmA, double rmB);
    void rebuildSequence();
    void scheduleStep (long long k, double t, double stepLen);
    void handleNoteOn (int note, float vel, int pos);
    void handleNoteOff (int note, int pos);

    void processFilter (float* L, float* R, int n);
    void processDrive (juce::AudioBuffer<float>& buffer, int n);
    void processTape (float* L, float* R, int n);
    void processHalftime (float* L, float* R, int n, double bpm, bool hostSync, double startPpq);
    void processDelay (float* L, float* R, int n, double bpm);
    void processReverb (float* L, float* R, int n);

    double fs = 44100.0;
    int maxBlock = 512;

    // sample sharing (slot 0 = A, slot 1 = B)
    mutable juce::SpinLock sampleLock;
    std::array<std::shared_ptr<const SampleData>, 2> sharedSample, audioSample;

    std::array<Voice, 32> voices;
    juce::uint64 ageCounter = 0;

    // arp / voice-interaction state
    std::vector<int> held;
    std::array<float, 128> noteVel {};
    std::array<int, 16> evo {};
    std::vector<SeqNote> seq, seqTmp;
    std::vector<ArpEvent> pending;
    long long noteCounter = 0, lastK = 0;
    bool haveLast = false, lastHostSync = false;
    double lastStepLen = 0.0, lastEndPpq = 0.0, freePpq = 0.0;
    int groupCounter = 0, curGroup = -1, curChoice = 0, hsGroup = -1;
    bool hsChoice = false;
    unsigned altCounter = 0;
    std::array<float, 2> lastSemis { 0.f, 0.f };
    std::array<bool, 2> haveLastPitch { false, false };
    juce::Random rng;
    int lastCountedGroup = -1;

    // effective (macro-modulated) values, refreshed once per block on the audio thread
    struct Eff
    {
        float cut = 9000.f, res = 0.7f, revMix = 0.f, revSize = 0.f, revDamp = 0.f, dlyMix = 0.f,
              drvAmt = 0.f, drvMix = 0.f, tapeNoise = 0.f, tapeSat = 0.f, drift = 0.f, spread = 0.f,
              scan = 0.f, evolve = 0.f, choke = 0.f, duck = 0.f, glide = 0.f, htMix = 0.f, hsAmt = 0.f;
        bool revOn = false, dlyOn = false, drvOn = false, tapeOn = false, wowOn = false, htOn = false, hsOn = false;
    } e;

    // fx state
    juce::dsp::StateVariableTPTFilter<float> filter;
    double lfoPhase = 0.0;

    juce::dsp::Oversampling<float> oversampler { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false };
    std::vector<float> scrL, scrR;
    float drvLpL = 0.f, drvLpR = 0.f;

    double wowPh1 = 0.0, wowPh2 = 0.0, wowPh3 = 0.0, prevWow = 1.0;
    float tpLpL = 0.f, tpLpR = 0.f, noiseLpL = 0.f, noiseLpR = 0.f;

    std::vector<float> htBuf[2][2];
    int htPos = 0, htRec = 0, htLenPrev = 0;
    double htRead = 0.0, htFluxPh = 0.0;
    float htLpL = 0.f, htLpR = 0.f;

    std::vector<float> dlL, dlR;
    int dlW = 0;
    float dlSmooth = 0.f, dlLpL = 0.f, dlLpR = 0.f;
    double dlyPh = 0.0;

    juce::Reverb reverb;
    std::vector<float> revL, revR, preL, preR;
    int preW = 0;

    // cached parameter pointers
    std::atomic<float> *pGain, *pOctave, *pTune, *pAutoTune, *pReverse, *pStart, *pEnd,
                       *pALevel, *pAPan, *pBLevel, *pBPan, *pBOct, *pBTune, *pBStart, *pBEnd, *pAbMode,
                       *pAtk, *pDec, *pSus, *pRel,
                       *pArpOn, *pDir, *pOctRange, *pRate, *pGate, *pSwing, *pHumT, *pHumV, *pSteps,
                       *pKey, *pScale, *pEvolve, *pJump, *pLayer, *pLayerLvl,
                       *pLenPit, *pLenVel, *pLenGate, *pLenProb, *pLenRat,
                       *pScan, *pSpread, *pChoke, *pDuck, *pGlide,
                       *pCut, *pRes, *pLfoRate, *pLfoDepth,
                       *pDrvOn, *pDrvType, *pDrvAmt, *pDrvTone, *pDrvMix, *pDrvOut,
                       *pTapeOn, *pTapeNoise, *pDrift, *pTapeFlutter, *pTapeSat, *pTapeTone,
                       *pHtOn, *pHtLen, *pHtMix, *pHtTone, *pHtFlux,
                       *pHsOn, *pHsAmt, *pHsDive, *pHsSpeed,
                       *pDlyOn, *pDlyTime, *pDlyFb, *pDlyMix, *pDlyTone, *pDlyDrift,
                       *pFltOn, *pMDark, *pMSpace, *pMGrit, *pMMove, *pMGlue, *pMHalf,
                       *pRevOn, *pRevSize, *pRevDamp, *pRevMix, *pRevPre, *pRevWidth;
    std::array<std::atomic<float>*, 16> sOn, sPit, sVel, sGate, sProb, sRat;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RiverrProcessor)
};
