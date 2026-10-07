#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>
namespace tape
{
constexpr double pi = 3.14159265358979323846;
struct Loop
{
    int start = 0, end = 0, fade = 0;
    double midi = 60, confidence = 0;
    bool pitched = false;
};
// Analysis is deliberately independent of the UI/audio framework and runs off the audio thread.
inline Loop analyse(const std::vector<float> &audio, double sr)
{
    if (sr < 8000 || audio.size() < size_t(sr * .15))
        throw std::runtime_error("Record at least 0.15 seconds.");
    const int dec = std::max(1, int(sr / 12000));
    const double rate = sr / dec;
    std::vector<float> x;
    for (size_t i = 0; i + dec <= audio.size(); i += dec)
    {
        double v = 0;
        for (int j = 0; j < dec; ++j)
            v += audio[i + j];
        x.push_back(float(v / dec));
    }
    const int n = int(rate * .065), hop = int(rate * .012), lo = int(rate / 1400), hi = int(rate / 55);
    struct Frame
    {
        int pos;
        double midi, quality, rms;
    };
    std::vector<Frame> frames;
    for (int p = 0; p + n + hi < int(x.size()); p += hop)
    {
        double energy = 0;
        for (int j = 0; j < n; ++j)
            energy += x[p + j] * x[p + j];
        double rms = std::sqrt(energy / n);
        if (rms < .004)
        {
            frames.push_back({p, 0, 0, rms});
            continue;
        }
        std::vector<double> d(hi + 1, 1);
        double sum = 0;
        for (int t = 1; t <= hi; ++t)
        {
            double diff = 0;
            for (int j = 0; j < n; ++j)
            {
                double a = x[p + j] - x[p + j + t];
                diff += a * a;
            }
            sum += diff;
            d[t] = sum > 1e-15 ? diff * t / sum : 1;
        }
        int lag = -1;
        for (int t = lo; t < hi - 1; ++t)
            if (d[t] < .15)
            {
                while (t + 1 < hi && d[t + 1] < d[t])
                    ++t;
                lag = t;
                break;
            }
        if (lag < 0)
        {
            frames.push_back({p, 0, 0, rms});
            continue;
        }
        double offset = 0;
        if (lag > 1 && lag < hi)
        {
            double den = d[lag - 1] - 2 * d[lag] + d[lag + 1];
            if (std::abs(den) > 1e-12)
                offset = std::clamp(.5 * (d[lag - 1] - d[lag + 1]) / den, -.5, .5);
        }
        frames.push_back({p, 69 + 12 * std::log2(rate / (lag + offset) / 440), 1 - d[lag], rms});
    }
    int bestA = -1, bestB = -1;
    double bestScore = 0;
    for (int a = 0; a < int(frames.size()); ++a)
    {
        if (frames[a].quality < .85)
            continue;
        double mean = frames[a].midi;
        for (int b = a; b < int(frames.size()); ++b)
        {
            if (frames[b].quality < .85 || std::abs(frames[b].midi - mean) > .30)
                break;
            mean += (frames[b].midi - mean) / (b - a + 1);
            double length = (frames[b].pos - frames[a].pos + n) / rate;
            double score = length * frames[b].quality;
            if (length >= .12 && score > bestScore)
            {
                bestA = a;
                bestB = b;
                bestScore = score;
            }
        }
    }
    Loop out;
    if (bestA < 0)
        throw std::runtime_error("No stable pitch found. Record a sustained single note, without vibrato.");
    out.start = frames[bestA].pos * dec;
    out.end = (frames[bestB].pos + n) * dec;
    out.midi = 0;
    out.confidence = 0;
    for (int i = bestA; i <= bestB; ++i)
    {
        out.midi += frames[i].midi;
        out.confidence += frames[i].quality;
    }
    out.midi /= bestB - bestA + 1;
    out.confidence /= bestB - bestA + 1;
    out.pitched = true;
    // Align the endpoints to positive zero crossings, then choose a locally similar seam.
    int radius = int(sr / (440 * std::pow(2., (out.midi - 69) / 12)) * 2);
    auto crossing = [&](int p)
    { return p > 0 && p < int(audio.size()) && audio[p - 1] <= 0 && audio[p] > 0; };
    for (int p = out.start; p < std::min(out.start + radius, out.end - 128); ++p)
        if (crossing(p))
        {
            out.start = p;
            break;
        }
    double cost = 1e100;
    int end = out.end;
    for (int p = std::max(out.start + 128, out.end - radius); p < out.end; ++p)
        if (crossing(p))
        {
            double c = 0;
            for (int j = -32; j < 32; ++j)
            {
                int a = std::clamp(out.start + j, 0, int(audio.size()) - 1),
                    b = std::clamp(p + j, 0, int(audio.size()) - 1);
                double v = audio[a] - audio[b];
                c += v * v;
            }
            if (c < cost)
            {
                cost = c;
                end = p;
            }
        }
    out.end = end;
    out.fade = std::min(int(sr * .015), (out.end - out.start) / 4);
    return out;
}
struct Sample
{
    std::vector<float> data;
    double rate = 44100;
    Loop loop;
};
struct Envelope
{
    enum Stage
    {
        off,
        attack,
        decay,
        sustain,
        release
    };
    Stage stage = off;
    float value = 0, releaseStep = 0;
    void on()
    {
        stage = attack;
        value = 0;
    }
    void stop(float seconds, double sr)
    {
        stage = release;
        releaseStep = value / float(std::max(1., seconds * sr));
    }
    float next(float a, float d, float s, double sr)
    {
        if (stage == attack)
        {
            value += 1.f / float(std::max(1., a * sr));
            if (value >= 1)
            {
                value = 1;
                stage = decay;
            }
        }
        else if (stage == decay)
        {
            value -= (1 - s) / float(std::max(1., d * sr));
            if (value <= s)
            {
                value = s;
                stage = sustain;
            }
        }
        else if (stage == sustain)
            value = s;
        else if (stage == release)
        {
            value -= releaseStep;
            if (value <= 0)
            {
                value = 0;
                stage = off;
            }
        }
        return value;
    }
};
inline float read(const Sample &s, double pos)
{
    int a = std::clamp(int(pos), 0, int(s.data.size()) - 1), b = std::min(a + 1, int(s.data.size()) - 1);
    return s.data[a] + float(pos - a) * (s.data[b] - s.data[a]);
}
inline float loopRead(const Sample &s, double &position, double step)
{
    const auto &l = s.loop;
    if (position >= l.end)
        position = l.start + l.fade + std::fmod(position - l.end, double(l.end - l.start - l.fade));
    float y = read(s, position);
    if (l.fade > 0 && position >= l.end - l.fade)
    {
        double t = (position - (l.end - l.fade)) / l.fade;
        // Complementary raised-cosine windows retain amplitude for correlated material.
        double w = .5 - .5 * std::cos(pi * t);
        y = float((1 - w) * y + w * read(s, l.start + t * l.fade));
    }
    position += step;
    return y;
}
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    float tick(float x)
    {
        double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return float(y);
    }
    void set(int type, double freq, double q, double db, double sr)
    {
        double w = 2 * pi * std::clamp(freq, 20., sr * .45) / sr, c = std::cos(w), s = std::sin(w),
               alpha = s / (2 * q), A = std::pow(10., db / 40), a0;
        if (type == 0)
        {
            b0 = (1 - c) / 2;
            b1 = 1 - c;
            b2 = b0;
            a0 = 1 + alpha;
            a1 = -2 * c;
            a2 = 1 - alpha;
        }
        else if (type == 1)
        {
            b0 = 1 + alpha * A;
            b1 = -2 * c;
            b2 = 1 - alpha * A;
            a0 = 1 + alpha / A;
            a1 = -2 * c;
            a2 = 1 - alpha / A;
        }
        else
        {
            double beta = 2 * std::sqrt(A) * alpha;
            if (type == 2)
            {
                b0 = A * ((A + 1) - (A - 1) * c + beta);
                b1 = 2 * A * ((A - 1) - (A + 1) * c);
                b2 = A * ((A + 1) - (A - 1) * c - beta);
                a0 = (A + 1) + (A - 1) * c + beta;
                a1 = -2 * ((A - 1) + (A + 1) * c);
                a2 = (A + 1) + (A - 1) * c - beta;
            }
            else
            {
                b0 = A * ((A + 1) + (A - 1) * c + beta);
                b1 = -2 * A * ((A - 1) + (A + 1) * c);
                b2 = A * ((A + 1) + (A - 1) * c - beta);
                a0 = (A + 1) - (A - 1) * c + beta;
                a1 = 2 * ((A - 1) - (A + 1) * c);
                a2 = (A + 1) - (A - 1) * c - beta;
            }
        }
        b0 /= a0;
        b1 /= a0;
        b2 /= a0;
        a1 /= a0;
        a2 /= a0;
    }
};
} // namespace tape
