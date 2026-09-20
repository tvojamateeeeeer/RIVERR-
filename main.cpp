#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PitchDetect.h"
#include "../Source/PluginEditor.h"

using namespace juce;

struct TestHead : AudioPlayHead
{
    double ppq = 0.0; double bpm = 120.0; bool playing = true;
    Optional<PositionInfo> getPosition() const override
    {
        PositionInfo i; i.setBpm (bpm); i.setIsPlaying (playing); i.setPpqPosition (ppq); return i;
    }
};

static void setP (DarkArpProcessor& p, const String& id, float real)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (real));
}

// returns onset times (in seconds) found in the rendered signal
static std::vector<double> onsets (const std::vector<float>& x, double sr)
{
    std::vector<double> out;
    const int hop = 64; float prev = 0.f; double last = -1;
    for (size_t i = 0; i + hop < x.size(); i += hop)
    {
        float e = 0; for (int j = 0; j < hop; ++j) e = std::max (e, std::abs (x[i + j]));
        if (e > 0.02f && prev < 0.005f && (double) i / sr - last > 0.02) { out.push_back ((double) i / sr); last = out.back(); }
        prev = e * 0.7f + prev * 0.3f;
        if (e < 0.003f) prev = 0.f;
    }
    return out;
}

int main()
{
    ScopedJuceInitialiser_GUI init;
    const double sr = 44100.0;

    // ---- make a test one-shot: A#3-ish (233.08 Hz) plucked sine w/ harmonics
    File wav = File ("/tmp/test_oneshot.wav");
    wav.deleteFile();
    {
        WavAudioFormat fmt;
        std::unique_ptr<FileOutputStream> os (wav.createOutputStream());
        std::unique_ptr<AudioFormatWriter> w (fmt.createWriterFor (os.get(), sr, 1, 16, {}, 0));
        os.release();
        AudioBuffer<float> b (1, (int) (sr * 0.5));
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const double t = i / sr, f = 233.08;
            const float env = (float) std::exp (-t * 6.0);
            b.setSample (0, i, env * (float) (0.6 * std::sin (2 * MathConstants<double>::pi * f * t)
                                              + 0.25 * std::sin (2 * MathConstants<double>::pi * 2 * f * t)));
        }
        w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }

    DarkArpProcessor p;
    p.setPlayConfigDetails (0, 2, sr, 512);
    p.prepareToPlay (sr, 512);
    std::printf ("load ok: %d\n", (int) p.loadSample (wav));
    auto s = p.getSample();
    std::printf ("info: %s\nrootShift=%.3f\n", s->info.toRawUTF8(), s->rootShift);

    // clean signal: no fx
    setP (p, "dlyMix", 0); setP (p, "revMix", 0); setP (p, "cut", 20000); setP (p, "gain", 0);
    setP (p, "arpOn", 1); setP (p, "rate", 5 /* 1/16 */); setP (p, "dir", 0); setP (p, "octRange", 0);
    setP (p, "gate", 0.5f); setP (p, "rel", 0.05f); setP (p, "atk", 0.001f);

    auto run = [&] (bool hostSync, double seconds, std::function<void(MidiBuffer&, int)> midiFn)
    {
        TestHead head;
        if (hostSync) p.setPlayHead (&head); else p.setPlayHead (nullptr);
        std::vector<float> out;
        AudioBuffer<float> buf (2, 512);
        const int blocks = (int) (seconds * sr / 512);
        for (int b = 0; b < blocks; ++b)
        {
            MidiBuffer mb; midiFn (mb, b);
            p.processBlock (buf, mb);
            for (int i = 0; i < 512; ++i) out.push_back (buf.getSample (0, i));
            head.ppq += 512.0 / sr * head.bpm / 60.0;
        }
        return out;
    };

    // 1) free-run arp, held C5 only: pitch check + timing
    auto y = run (false, 2.5, [&] (MidiBuffer& mb, int b) { if (b == 0) mb.addEvent (MidiMessage::noteOn (1, 60, 1.0f), 0); });
    auto on = onsets (y, sr);
    std::printf ("free-run onsets (%zu): ", on.size());
    for (size_t i = 0; i < on.size() && i < 8; ++i) std::printf ("%.3f ", on[i]);
    std::printf ("\n  expected spacing 0.125 s\n");
    if (on.size() > 1)
        std::printf ("  spacing[1]-[0] = %.4f\n", on[1] - on[0]);

    // pitch of the first hit
    {
        const int st = (int) (on[0] * sr) + 200;
        std::vector<float> seg (y.begin() + st, y.begin() + std::min<size_t> (y.size(), st + 8000));
        seg.resize (8000, 0.f);
        auto r = detectPitchYin (seg.data(), (int) seg.size(), sr);
        std::printf ("  played C5(60) -> measured %.2f Hz (expected 261.63)\n", r.hz);
    }

    // 2) chord up arp, octave range 2 -> expect 3 notes * 2 octaves pattern; measure pitches of 6 hits
    setP (p, "octRange", 1);
    p.prepareToPlay (sr, 512);
    y = run (true, 3.0, [&] (MidiBuffer& mb, int b)
    {
        if (b == 0) { mb.addEvent (MidiMessage::noteOn (1, 60, 1.0f), 0); mb.addEvent (MidiMessage::noteOn (1, 64, 1.0f), 0); mb.addEvent (MidiMessage::noteOn (1, 67, 1.0f), 0); }
    });
    on = onsets (y, sr);
    std::printf ("host-sync chord onsets (%zu). pitches:", on.size());
    for (size_t k = 0; k < on.size() && k < 8; ++k)
    {
        const int st = (int) (on[k] * sr) + 150;
        std::vector<float> seg (y.begin() + st, y.begin() + std::min<size_t> (y.size(), st + 6000));
        seg.resize (6000, 0.f);
        auto r = detectPitchYin (seg.data(), (int) seg.size(), sr);
        std::printf (" %.0fHz(%.1fst)", r.hz, 12.0 * std::log2 (r.hz / 261.63));
    }
    std::printf ("\n  first onset time %.3f (grid-aligned expected ~0)\n", on.empty() ? -1.0 : on[0]);

    // 3) step off + non-arp mode
    setP (p, "octRange", 0); setP (p, "s1_on", 0); setP (p, "s2_prob", 0);
    p.prepareToPlay (sr, 512);
    y = run (true, 1.0, [&] (MidiBuffer& mb, int b) { if (b == 0) mb.addEvent (MidiMessage::noteOn (1, 60, 1.0f), 0); });
    on = onsets (y, sr);
    std::printf ("with step2 off + step3 prob 0: onsets (%zu): ", on.size());
    for (auto t : on) std::printf ("%.3f ", t);
    std::printf ("\n");

    setP (p, "arpOn", 0);
    p.prepareToPlay (sr, 512);
    y = run (false, 1.0, [&] (MidiBuffer& mb, int b) { if (b == 2) mb.addEvent (MidiMessage::noteOn (1, 72, 1.0f), 100); if (b == 10) mb.addEvent (MidiMessage::noteOff (1, 72), 0); });
    on = onsets (y, sr);
    float peak = 0; for (auto v : y) peak = std::max (peak, std::abs (v));
    std::printf ("non-arp: onsets %zu first %.4f peak %.3f\n", on.size(), on.empty() ? -1.0 : on[0], peak);
    {
        const int st = (int) (on[0] * sr) + 150;
        std::vector<float> seg (y.begin() + st, y.begin() + st + 4000);
        seg.resize (6000, 0.f);
        auto r = detectPitchYin (seg.data(), (int) seg.size(), sr);
        std::printf ("  played C6(72) -> %.2f Hz (expected 523.25)\n", r.hz);
    }

    // 4) full FX chain finite & non-silent
    setP (p, "dlyMix", 0.4f); setP (p, "revMix", 0.6f); setP (p, "cut", 3000); setP (p, "lfoDepth", 0.5f);
    setP (p, "arpOn", 1);
    p.prepareToPlay (sr, 512);
    y = run (false, 2.0, [&] (MidiBuffer& mb, int b) { if (b == 0) { mb.addEvent (MidiMessage::noteOn (1, 60, 1.0f), 0); mb.addEvent (MidiMessage::noteOn (1, 63, 1.0f), 0); } });
    bool finite = true; float pk = 0; for (auto v : y) { if (! std::isfinite (v)) finite = false; pk = std::max (pk, std::abs (v)); }
    std::printf ("fx chain: finite=%d peak=%.3f\n", (int) finite, pk);

    // 5) state save/restore round trip
    MemoryBlock mbk; p.getStateInformation (mbk);
    DarkArpProcessor p2; p2.prepareToPlay (sr, 512); p2.setStateInformation (mbk.getData(), (int) mbk.getSize());
    std::printf ("state restore: sample loaded=%d, cut=%.0f\n", (int) (p2.getSample() != nullptr), p2.apvts.getRawParameterValue ("cut")->load());
    const bool saved = p.savePreset ("__test");
    const int listed = p.listPresets().size();
    setP (p, "cut", 500);
    const bool loaded = p.loadPreset ("__test");
    std::printf ("preset save=%d listed=%d load=%d cut after load=%.0f (expected 3000)\n", (int) saved, listed, (int) loaded, p.apvts.getRawParameterValue ("cut")->load());
    p.getPresetDir().getChildFile ("__test.darkarp").deleteFile();
    // 6) GUI snapshot
    {
        setP (p, "cut", 9000); setP (p, "lfoDepth", 0.f);
        p.loadSample (wav); p.randomizeSteps();
        std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
        ed->setVisible (true);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        PNGImageFormat png;
        File f ("/tmp/gui.png"); f.deleteFile();
        FileOutputStream fos (f);
        png.writeImageToStream (img, fos);
        std::printf ("gui snapshot %dx%d\n", img.getWidth(), img.getHeight());
    }
    return 0;
}
