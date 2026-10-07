#include "../Source/VoiceBank.h"
#include <cstdlib>
#include <iostream>
#include <random>
void require(bool b, const char *msg)
{
    if (!b)
    {
        std::cerr << "FAIL: " << msg << '\n';
        std::exit(1);
    }
}
std::vector<float> tone(double hz, double sr, double seconds)
{
    std::vector<float> x(size_t(sr * seconds));
    for (size_t i = 0; i < x.size(); ++i)
        x[i] = float(.4 * std::sin(2 * tape::pi * hz * i / sr) + .1 * std::sin(4 * tape::pi * hz * i / sr));
    return x;
}
int main()
{
    for (double sr : {44100., 48000., 96000.})
        for (double hz : {65.406, 110., 261.626, 440., 880.})
        {
            auto x = tone(hz, sr, .8);
            auto l = tape::analyse(x, sr);
            double midi = 69 + 12 * std::log2(hz / 440);
            require(std::abs(l.midi - midi) < .12, "pitch within 12 cents");
            require(l.start >= 0 && l.end <= int(x.size()) && l.end - l.start > sr * .12, "loop bounds");
            tape::Sample s{x, sr, l};
            double pos = l.start;
            float last = 0, maxJump = 0;
            for (int i = 0; i < sr * 3; ++i)
            {
                float y = tape::loopRead(s, pos, 1);
                require(std::isfinite(y), "finite loop output");
                if (i)
                    maxJump = std::max(maxJump, std::abs(y - last));
                last = y;
            }
            require(maxJump < .15, "smooth seam");
        }
    auto first = tone(220, 48000, .25), stable = tone(329.628, 48000, 1.1);
    first.insert(first.end(), stable.begin(), stable.end());
    auto l = tape::analyse(first, 48000);
    require(std::abs(l.midi - 64) < .12, "longest stable note selection");
    require(l.start > 48000 * .20, "transient excluded");
    for (bool noise : {false, true})
    {
        std::vector<float> x(24000);
        std::mt19937 r(123);
        if (noise)
            for (auto &v : x)
                v = float(int(r() % 2000) - 1000) / 3000;
        bool rejected = false;
        try
        {
            tape::analyse(x, 48000);
        }
        catch (...)
        {
            rejected = true;
        }
        require(rejected, "reject unpitched recording");
    }
    tape::Envelope e;
    e.on();
    for (int i = 0; i < 480; ++i)
        e.next(.01f, .1f, .6f, 48000);
    require(e.value > .99, "attack time");
    for (int i = 0; i < 5000; ++i)
        e.next(.01f, .1f, .6f, 48000);
    require(std::abs(e.value - .6) < .001, "sustain level");
    e.stop(.1f, 48000);
    for (int i = 0; i < 4802; ++i)
        e.next(.01f, .1f, .6f, 48000);
    require(e.stage == tape::Envelope::off, "release ends");
    for (int type = 0; type < 4; ++type)
    {
        tape::Biquad f;
        f.set(type, 1500, .707, 12, 48000);
        for (int i = 0; i < 48000; ++i)
            require(std::isfinite(f.tick(i == 0 ? 1 : 0)), "filter stability");
    }
    tape::Sample sample;
    sample.data = tone(261.626, 48000, 1);
    sample.rate = 48000;
    sample.loop = tape::analyse(sample.data, sample.rate);
    tape::VoiceBank bank;
    bank.sampleRate = 48000;
    for (int note : {60, 64, 67})
        bank.noteOn(note, 1, .7f);
    require(bank.count() == 3, "three-note polyphony");
    double energy = 0;
    for (int i = 0; i < 12000; ++i)
    {
        float x = bank.render(sample, 60, 1, .01f, .1f, .8f);
        require(std::isfinite(x), "polyphonic output");
        energy += x * x;
    }
    require(energy > 10, "chord has audio energy");
    bank.pedal(1, true, .1f);
    for (int note : {60, 64, 67})
        bank.noteOff(note, 1, .1f);
    for (int i = 0; i < 6000; ++i)
        bank.render(sample, 60, 1, .01f, .1f, .8f);
    require(bank.count() == 3, "pedal holds released notes");
    bank.pedal(1, false, .1f);
    for (int i = 0; i < 5000; ++i)
        bank.render(sample, 60, 1, .01f, .1f, .8f);
    require(bank.count() == 0, "pedal release ends chord");
    bank.noteOn(60, 1, 1);
    bank.render(sample, 60, 1, .01f, .1f, .8f);
    double pos = bank.voices[0].pos;
    bank.pitch(1, 16383);
    bank.render(sample, 60, 1, .01f, .1f, .8f);
    require(std::abs((bank.voices[0].pos - pos) - std::pow(2., 2. / 12)) < .001, "two-semitone pitch bend");
    bank.reset();
    for (int i = 0; i < 40; ++i)
    {
        bank.noteOn(40 + i, 1, .5f);
        bank.render(sample, 60, 1, .01f, .1f, .8f);
    }
    require(bank.count() == 32, "bounded voice stealing");
    bank.stopChannel(1);
    require(bank.count() == 0, "panic stops voices");
    std::cout << "PASS: pitch at 3 sample rates / 5 notes; stable segment; silence/noise rejection; loop "
                 "seams; ADSR; filter stability; polyphony, sustain, pitch bend, voice stealing\n";
}
