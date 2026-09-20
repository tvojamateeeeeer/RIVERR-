#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

// Small YIN pitch detector. Used once, when a sample is loaded, to find the
// fundamental of the one-shot so it can be tuned to the nearest C.

struct PitchResult
{
    bool ok = false;
    float hz = 0.f;
    float midi = 0.f;       // fractional MIDI note number (A4 = 69)
    float confidence = 0.f; // 0..1
};

inline PitchResult detectPitchYin (const float* x, int n, double sr)
{
    PitchResult res;
    const int W = 4096;
    const int tauMax = (int) std::min (sr / 30.0, 4096.0);   // lowest ~30 Hz
    const int tauMin = (int) std::max (2.0, sr / 2500.0);    // highest ~2.5 kHz
    if (n < W + tauMax + 1 || tauMax <= tauMin + 2)
        return res;

    std::vector<double> d ((size_t) tauMax + 1, 0.0), cm ((size_t) tauMax + 1, 1.0);

    for (int tau = 1; tau <= tauMax; ++tau)
    {
        double s = 0.0;
        for (int j = 0; j < W; ++j)
        {
            const double diff = (double) x[j] - (double) x[j + tau];
            s += diff * diff;
        }
        d[(size_t) tau] = s;
    }

    double run = 0.0;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        run += d[(size_t) tau];
        cm[(size_t) tau] = run > 0.0 ? d[(size_t) tau] * tau / run : 1.0;
    }

    int tauEst = -1;
    for (int tau = tauMin; tau < tauMax; ++tau)
    {
        if (cm[(size_t) tau] < 0.15)
        {
            while (tau + 1 < tauMax && cm[(size_t) tau + 1] < cm[(size_t) tau])
                ++tau;
            tauEst = tau;
            break;
        }
    }

    if (tauEst < 0)
    {
        double best = 1e9;
        for (int tau = tauMin; tau < tauMax; ++tau)
            if (cm[(size_t) tau] < best) { best = cm[(size_t) tau]; tauEst = tau; }

        if (tauEst < 0 || best > 0.35)
            return res;
    }

    double t = tauEst;
    if (tauEst > 1 && tauEst < tauMax)
    {
        const double a = cm[(size_t) tauEst - 1], b = cm[(size_t) tauEst], c = cm[(size_t) tauEst + 1];
        const double den = a - 2.0 * b + c;
        if (std::abs (den) > 1e-12)
            t = tauEst + 0.5 * (a - c) / den;
    }

    res.hz = (float) (sr / t);
    res.midi = (float) (69.0 + 12.0 * std::log2 (res.hz / 440.0));
    res.confidence = (float) (1.0 - cm[(size_t) tauEst]);
    res.ok = true;
    return res;
}

// Looks at several windows after the attack and returns the median pitch.
inline PitchResult detectSamplePitch (const std::vector<float>& mono, double sr)
{
    const int len = (int) mono.size();
    float peak = 0.f;
    int peakIdx = 0;
    for (int i = 0; i < len; ++i)
        if (std::abs (mono[(size_t) i]) > peak) { peak = std::abs (mono[(size_t) i]); peakIdx = i; }

    if (peak < 1e-4f)
        return {};

    const int W = 4096;
    const int tauMax = (int) std::min (sr / 30.0, 4096.0);
    const int need = W + tauMax + 2;
    std::vector<float> buf ((size_t) need);
    std::vector<double> midis;

    const double offsets[] = { 0.005, 0.02, 0.05, 0.12, 0.25, 0.4 };
    for (double off : offsets)
    {
        const int start = peakIdx + (int) (off * sr);
        if (start >= len - 256)
            break;

        std::fill (buf.begin(), buf.end(), 0.f);
        const int cnt = std::min (need, len - start);
        std::copy (mono.begin() + start, mono.begin() + start + cnt, buf.begin());

        double e = 0.0;
        for (int i = 0; i < W; ++i)
            e += (double) buf[(size_t) i] * buf[(size_t) i];
        e = std::sqrt (e / W);
        if (e < peak * 0.02)
            continue;

        const auto r = detectPitchYin (buf.data(), need, sr);
        if (r.ok && r.confidence > 0.75f)
            midis.push_back ((double) r.midi);
    }

    if (midis.empty())
        return {};

    std::sort (midis.begin(), midis.end());
    const double m = midis[midis.size() / 2];

    PitchResult out;
    out.ok = true;
    out.midi = (float) m;
    out.hz = (float) (440.0 * std::pow (2.0, (m - 69.0) / 12.0));
    out.confidence = 1.f;
    return out;
}
