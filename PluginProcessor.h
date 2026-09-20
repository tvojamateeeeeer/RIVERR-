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
    double pos = 0.0, rate = 1.0;
    float startS = 0.f, endS = 0.f;
    bool reverse = false;
    float vel = 1.f;
    float panL = 1.f, panR = 1.f;
    int delay = 0;                 // samples until the voice starts (inside this block)
    int releaseCountdown = -1;     // samples (after start) until note-off, -1 = none
    int note = -1;                 // midi note when played without arp
    int played = 0;
    juce::ADSR adsr;
    juce::uint64 age = 0;
};

struct ArpEvent { double when; int semis; float vel; double gateBeats; };
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
    bool loadSample (const juce::File& f);
    std::shared_ptr<const SampleData> getSample() const;

    static juce::File getPresetDir();
    juce::StringArray listPresets() const;
    bool savePreset (const juce::String& name);
    bool loadPreset (const juce::String& name);
    void randomizeSteps();

    // lanes: 0 on, 1 pitch, 2 velocity, 3 gate, 4 probability, 5 ratchet
    int getLaneLength (int lane) const;
    std::array<std::atomic<int>, 6> laneStep;

    APVTS apvts;

private:
    static APVTS::ParameterLayout createLayout();
    void setParamValue (const juce::String& id, float realValue);
    void restoreSampleFromState();

    // audio thread
    void triggerVoice (int note, float semis, float vel, int delay, int gateSamples);
    void renderVoices (float* L, float* R, int n, double rateMul);
    void rebuildSequence();
    void scheduleStep (long long k, double t, double stepLen);
    void handleNoteOn (int note, float vel, int pos);
    void handleNoteOff (int note, int pos);

    double fs = 44100.0;

    // sample sharing
    mutable juce::SpinLock sampleLock;
    std::shared_ptr<const SampleData> sharedSample;
    std::shared_ptr<const SampleData> audioSample;

    std::array<Voice, 24> voices;
    juce::uint64 ageCounter = 0;

    // arp state
    std::vector<int> held;
    std::array<float, 128> noteVel {};
    std::array<int, 16> evo {};          // evolving pitch drift per pitch-lane step
    std::vector<SeqNote> seq, seqTmp;
    std::vector<ArpEvent> pending;
    long long noteCounter = 0, lastK = 0;
    bool haveLast = false, lastHostSync = false;
    double lastStepLen = 0.0, lastEndPpq = 0.0, freePpq = 0.0;
    double wowPh1 = 0.0, wowPh2 = 0.0;
    juce::Random rng;

    // fx
    juce::dsp::StateVariableTPTFilter<float> filter;
    double lfoPhase = 0.0;
    std::vector<float> dlL, dlR;
    int dlW = 0;
    float dlSmooth = 0.f, dlLpL = 0.f, dlLpR = 0.f;
    juce::Reverb reverb;

    // cached parameter pointers
    std::atomic<float> *pGain, *pOctave, *pTune, *pAutoTune, *pReverse, *pStart, *pEnd,
                       *pAtk, *pDec, *pSus, *pRel,
                       *pArpOn, *pDir, *pOctRange, *pRate, *pGate, *pSwing, *pHumT, *pHumV, *pSteps,
                       *pKey, *pScale, *pEvolve, *pJump, *pLayer, *pLayerLvl,
                       *pLenPit, *pLenVel, *pLenGate, *pLenProb, *pLenRat,
                       *pScan, *pDrift, *pSpread,
                       *pCut, *pRes, *pLfoRate, *pLfoDepth,
                       *pDlyTime, *pDlyFb, *pDlyMix, *pRevSize, *pRevDamp, *pRevMix;
    std::array<std::atomic<float>*, 16> sOn, sPit, sVel, sGate, sProb, sRat;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RiverrProcessor)
};
