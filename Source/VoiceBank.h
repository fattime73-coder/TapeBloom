#pragma once
#include "LoopAnalysis.h"
#include <array>
namespace tape
{
struct VoiceBank
{
    struct Voice
    {
        int note = -1, channel = 1;
        double pos = 0, age = 0;
        float velocity = 0;
        bool held = false;
        Envelope env;
    };
    std::array<Voice, 32> voices{};
    std::array<bool, 17> sustain{};
    std::array<double, 17> bend{};
    double sampleRate = 44100;
    VoiceBank() { bend.fill(1); }
    void reset()
    {
        for (auto &v : voices)
            v = Voice{};
        sustain.fill(false);
        bend.fill(1);
    }
    void noteOn(int note, int channel, float velocity)
    {
        Voice *target = nullptr;
        for (auto &v : voices)
            if (v.env.stage == Envelope::off)
            {
                target = &v;
                break;
            }
        if (!target)
            target = &*std::max_element(voices.begin(), voices.end(),
                                        [](const Voice &a, const Voice &b) { return a.age < b.age; });
        *target = Voice{};
        target->note = note;
        target->channel = channel;
        target->velocity = velocity;
        target->held = true;
        target->env.on();
    }
    void noteOff(int note, int channel, float release)
    {
        for (auto &v : voices)
            if (v.note == note && v.channel == channel && v.env.stage != Envelope::off)
            {
                v.held = false;
                if (!sustain[size_t(channel)])
                    v.env.stop(release, sampleRate);
            }
    }
    void pedal(int channel, bool down, float release)
    {
        sustain[size_t(channel)] = down;
        if (!down)
            for (auto &v : voices)
                if (v.channel == channel && !v.held && v.env.stage != Envelope::off)
                    v.env.stop(release, sampleRate);
    }
    void pitch(int channel, int wheel)
    {
        bend[size_t(channel)] = std::pow(2., 2. * (wheel - 8192) / 8192 / 12);
    }
    void stopChannel(int channel)
    {
        for (auto &v : voices)
            if (v.channel == channel)
                v = Voice{};
        sustain[size_t(channel)] = false;
    }
    int count() const
    {
        int n = 0;
        for (auto &v : voices)
            if (v.env.stage != Envelope::off)
                ++n;
        return n;
    }
    float render(const Sample &sample, float root, double modulation, float attack, float decay, float level)
    {
        float y = 0;
        for (auto &v : voices)
            if (v.env.stage != Envelope::off)
            {
                double step = sample.rate / sampleRate * std::pow(2., (v.note - root) / 12.) *
                              bend[size_t(v.channel)] * modulation;
                y +=
                    loopRead(sample, v.pos, step) * v.env.next(attack, decay, level, sampleRate) * v.velocity;
                ++v.age;
            }
        return y;
    }
};
} // namespace tape
