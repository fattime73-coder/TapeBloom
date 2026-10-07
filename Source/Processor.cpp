#include "Processor.h"
#include "Editor.h"
using namespace juce;
AudioProcessorValueTreeState::ParameterLayout TapeBloomProcessor::layout()
{
    AudioProcessorValueTreeState::ParameterLayout p;
    auto add = [&](const char *id, const char *name, float lo, float hi, float initial, float skew = 1.f)
    {
        p.add(std::make_unique<AudioParameterFloat>(ParameterID{id, 1}, name,
                                                    NormalisableRange<float>(lo, hi, 0, skew), initial));
    };
    add("attack", "Attack", .001f, 5, .02f, .35f);
    add("decay", "Decay", .01f, 5, .3f, .35f);
    add("sustain", "Sustain", 0, 1, .8f);
    add("release", "Release", .01f, 10, 1, .35f);
    add("cutoff", "Cutoff", 30, 20000, 9000, .3f);
    add("resonance", "Resonance", .5f, 5, .707f);
    add("wow", "Wow cents", 0, 60, 5);
    add("flutter", "Flutter cents", 0, 30, 2);
    add("drive", "Tape drive", 0, 1, .15f);
    add("noise", "Tape noise", 0, 1, 0);
    add("low", "Low EQ dB", -12, 12, 0);
    add("mid", "Mid EQ dB", -12, 12, 0);
    add("high", "High EQ dB", -12, 12, 0);
    add("volume", "Volume dB", -60, 6, -12);
    add("root", "Root MIDI note", 24, 96, 60);
    return p;
}
TapeBloomProcessor::TapeBloomProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Recording Input", AudioChannelSet::stereo(), true)
                         .withOutput("Output", AudioChannelSet::stereo(), true)),
      params(*this, nullptr, "TapeBloom", layout())
{
    formats.registerBasicFormats();
    startTimerHz(20);
}
TapeBloomProcessor::~TapeBloomProcessor()
{
    stopTimer();
    if (pending.valid())
        pending.wait();
}
bool TapeBloomProcessor::isBusesLayoutSupported(const BusesLayout &b) const
{
    return b.getMainOutputChannelSet() == AudioChannelSet::stereo() &&
           (b.getMainInputChannelSet().isDisabled() ||
            b.getMainInputChannelSet() == AudioChannelSet::mono() ||
            b.getMainInputChannelSet() == AudioChannelSet::stereo());
}
void TapeBloomProcessor::prepareToPlay(double rate, int)
{
    sr = rate;
    capture.assign(size_t(sr * 30), 0);
    captureCount = 0;
    recording = false;
    seconds = 0;
    bank.sampleRate = sr;
    bank.reset();
    for (auto &f : filters)
        f.reset();
    gain.reset(sr, .03);
    gain.setCurrentAndTargetValue(Decibels::decibelsToGain(value("volume")));
}
void TapeBloomProcessor::message(const String &s)
{
    const ScopedLock l(statusLock);
    status = s;
}
String TapeBloomProcessor::statusText()
{
    const ScopedLock l(statusLock);
    return status;
}
std::shared_ptr<const tape::Sample> TapeBloomProcessor::snapshot()
{
    const ScopedLock l(getCallbackLock());
    return sample;
}
void TapeBloomProcessor::panic()
{
    const ScopedLock l(getCallbackLock());
    bank.reset();
    keyboard.reset();
}
bool TapeBloomProcessor::startRecording()
{
    if (busy)
        return false;
    const ScopedLock l(getCallbackLock());
    if (getTotalNumInputChannels() == 0 || capture.empty())
    {
        message("No input. Enable an input device or import a WAV/AIFF file.");
        return false;
    }
    bank.reset();
    captureCount = 0;
    seconds = 0;
    recording = true;
    message("Recording — hold one steady note; maximum 30 seconds");
    return true;
}
void TapeBloomProcessor::stopRecording()
{
    std::vector<float> copy;
    double rate;
    {
        const ScopedLock l(getCallbackLock());
        if (captureCount == 0)
        {
            recording = false;
            return;
        }
        recording = false;
        copy.assign(capture.begin(), capture.begin() + captureCount);
        captureCount = 0;
        rate = sr;
    }
    beginAnalysis(std::move(copy), rate);
}
void TapeBloomProcessor::beginAnalysis(std::vector<float> data, double rate)
{
    if (busy)
        return;
    discardAnalysis = false;
    busy = true;
    message("Finding stable pitch and a smooth loop…");
    pending = std::async(std::launch::async,
                         [data = std::move(data), rate]() mutable
                         {
                             Result r;
                             try
                             {
                                 auto s = std::make_shared<tape::Sample>();
                                 s->loop = tape::analyse(data, rate);
                                 s->rate = rate;
                                 s->data = std::move(data);
                                 r.sample = s;
                             }
                             catch (const std::exception &e)
                             {
                                 r.error = e.what();
                             }
                             return r;
                         });
}
void TapeBloomProcessor::loadAudio(const File &file)
{
    if (busy || recording)
        return;
    std::unique_ptr<AudioFormatReader> reader(formats.createReaderFor(file));
    if (!reader)
    {
        message("Cannot read this audio file. Use WAV, AIFF or FLAC.");
        return;
    }
    if (reader->sampleRate < 8000 || reader->sampleRate > 192000 ||
        reader->lengthInSamples > reader->sampleRate * 30)
    {
        message("Use an audio file of 30 seconds or less, 8–192 kHz.");
        return;
    }
    AudioBuffer<float> b(int(reader->numChannels), int(reader->lengthInSamples));
    if (!reader->read(&b, 0, b.getNumSamples(), 0, true, true))
    {
        message("Audio file read failed.");
        return;
    }
    std::vector<float> mono(size_t(b.getNumSamples()), 0);
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
            mono[size_t(i)] += b.getSample(c, i) / b.getNumChannels();
    beginAnalysis(std::move(mono), reader->sampleRate);
}
void TapeBloomProcessor::reanalyse()
{
    if (busy || recording)
        return;
    auto s = snapshot();
    if (s)
        beginAnalysis(s->data, s->rate);
}
void TapeBloomProcessor::install(std::shared_ptr<tape::Sample> s)
{
    {
        const ScopedLock l(getCallbackLock());
        bank.reset();
        sample = s;
    }
    auto *p = params.getParameter("root");
    p->beginChangeGesture();
    p->setValueNotifyingHost(p->convertTo0to1(float(s->loop.midi)));
    p->endChangeGesture();
    message("Ready • detected MIDI " + String(s->loop.midi, 2) + " • loop " +
            String((s->loop.end - s->loop.start) / s->rate, 2) + " s");
}
void TapeBloomProcessor::timerCallback()
{
    if (recording && seconds >= 29.99f)
        stopRecording();
    if (busy && pending.valid() && pending.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
        auto r = pending.get();
        if (!discardAnalysis)
        {
            if (r.sample)
                install(r.sample);
            else
                message(r.error);
        }
        busy = false;
    }
}
void TapeBloomProcessor::editLoop(double a, double b)
{
    if (busy || recording)
        return;
    auto old = snapshot();
    if (!old)
        return;
    auto s = std::make_shared<tape::Sample>(*old);
    int n = int(s->data.size());
    s->loop.start = std::clamp(int(a * n), 0, n - 128);
    s->loop.end = std::clamp(int(b * n), s->loop.start + 128, n);
    s->loop.fade = std::min(int(s->rate * .015), (s->loop.end - s->loop.start) / 4);
    const ScopedLock l(getCallbackLock());
    bank.reset();
    sample = s;
}
void TapeBloomProcessor::midi(const MidiMessage &m)
{
    int ch = std::clamp(m.getChannel(), 1, 16);
    if (m.isNoteOn() && sample)
        bank.noteOn(m.getNoteNumber(), ch, m.getFloatVelocity());
    else if (m.isNoteOff())
        bank.noteOff(m.getNoteNumber(), ch, value("release"));
    else if (m.isSustainPedalOn())
        bank.pedal(ch, true, value("release"));
    else if (m.isSustainPedalOff())
        bank.pedal(ch, false, value("release"));
    else if (m.isPitchWheel())
        bank.pitch(ch, m.getPitchWheelValue());
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        bank.stopChannel(ch);
}
void TapeBloomProcessor::processBlock(AudioBuffer<float> &b, MidiBuffer &events)
{
    ScopedNoDenormals noDenormals;
    const ScopedTryLock lock(getCallbackLock());
    if (!lock.isLocked())
    {
        b.clear();
        events.clear();
        return;
    }
    int n = b.getNumSamples(), inputs = getTotalNumInputChannels();
    float peak = 0;
    for (int c = 0; c < inputs; ++c)
        peak = std::max(peak, b.getMagnitude(c, 0, n));
    inputLevel = peak;
    if (recording)
    {
        for (int i = 0; i < n && captureCount < int(capture.size()); ++i)
        {
            float x = 0;
            for (int c = 0; c < inputs; ++c)
                x += b.getSample(c, i);
            capture[size_t(captureCount++)] = inputs ? x / inputs : 0;
        }
        seconds = float(captureCount / sr);
    }
    b.clear();
    keyboard.processNextMidiBuffer(events, 0, n, true);
    filters[0].set(0, value("cutoff"), value("resonance"), 0, sr);
    filters[1].set(2, 180, .707, value("low"), sr);
    filters[2].set(1, 1000, .7, value("mid"), sr);
    filters[3].set(3, 5000, .707, value("high"), sr);
    gain.setTargetValue(Decibels::decibelsToGain(value("volume")));
    float a = value("attack"), d = value("decay"), s = value("sustain"), root = value("root"),
          wow = value("wow"), flutter = value("flutter"), drive = value("drive"), noise = value("noise");
    auto it = events.cbegin();
    float outPeak = 0;
    for (int i = 0; i < n; ++i)
    {
        while (it != events.cend() && (*it).samplePosition <= i)
        {
            midi((*it).getMessage());
            ++it;
        }
        float y = 0;
        int count = 0;
        double modulation = std::pow(2., (wow * std::sin(2 * tape::pi * .55 * phase) +
                                          flutter * (.7 * std::sin(2 * tape::pi * 7.3 * phase) +
                                                     .3 * std::sin(2 * tape::pi * 11.1 * phase))) /
                                             1200.);
        if (sample && !recording)
        {
            y = bank.render(*sample, root, modulation, a, d, s);
            count = bank.count();
        }
        activeVoices = count;
        if (count)
            y += (random.nextFloat() * 2 - 1) * noise * .008f;
        y = float(std::tanh(y * (1 + drive * 4)) / (1 + drive * 2));
        for (auto &f : filters)
            y = f.tick(y);
        y = std::clamp(y * gain.getNextValue(), -.98f, .98f);
        if (!std::isfinite(y))
            y = 0;
        outPeak = std::max(outPeak, std::abs(y));
        for (int c = 0; c < b.getNumChannels(); ++c)
            b.setSample(c, i, y);
        phase += 1 / sr;
        if (phase > 10000)
            phase -= 10000;
    }
    outputLevel = outPeak;
    events.clear();
}
void TapeBloomProcessor::getStateInformation(MemoryBlock &dest)
{
    auto state = params.copyState();
    auto s = snapshot();
    if (s)
    {
        MemoryBlock bytes(s->data.data(), s->data.size() * sizeof(float));
        state.setProperty("audio", bytes.toBase64Encoding(), nullptr);
        state.setProperty("rate", s->rate, nullptr);
        state.setProperty("start", s->loop.start, nullptr);
        state.setProperty("end", s->loop.end, nullptr);
        state.setProperty("fade", s->loop.fade, nullptr);
    }
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, dest);
}
void TapeBloomProcessor::setStateInformation(const void *data, int size)
{
    if (size <= 0 || size > 48 * 1024 * 1024)
        return;
    auto xml = getXmlFromBinary(data, size);
    if (!xml)
        return;
    auto state = ValueTree::fromXml(*xml);
    if (!state.hasType("TapeBloom"))
        return;
    std::shared_ptr<tape::Sample> restored;
    if (state.hasProperty("audio"))
    {
        MemoryBlock bytes;
        if (!bytes.fromBase64Encoding(state["audio"].toString()) || bytes.getSize() % sizeof(float))
            return;
        double rate = state["rate"];
        int n = int(bytes.getSize() / sizeof(float));
        if (rate < 8000 || rate > 192000 || n < 128 || n > rate * 30)
            return;
        restored = std::make_shared<tape::Sample>();
        restored->rate = rate;
        restored->data.resize(size_t(n));
        std::memcpy(restored->data.data(), bytes.getData(), bytes.getSize());
        for (float x : restored->data)
            if (!std::isfinite(x) || std::abs(x) > 100)
                return;
        auto &l = restored->loop;
        l.start = int(state["start"]);
        l.end = int(state["end"]);
        l.fade = int(state["fade"]);
        if (l.start < 0 || l.end > n || l.end - l.start < 128 || l.fade < 0 || l.fade > (l.end - l.start) / 4)
            return;
    }
    // Host restores supersede an in-flight analysis. Keep the future alive until it finishes.
    discardAnalysis = true;
    recording = false;
    state.removeProperty("audio", nullptr);
    state.removeProperty("rate", nullptr);
    state.removeProperty("start", nullptr);
    state.removeProperty("end", nullptr);
    state.removeProperty("fade", nullptr);
    params.replaceState(state);
    {
        const ScopedLock lock(getCallbackLock());
        bank.reset();
        sample = restored;
    }
    message(restored ? "Tape and settings restored" : "Settings restored");
}
AudioProcessorEditor *TapeBloomProcessor::createEditor()
{
    return new TapeBloomEditor(*this);
}
AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new TapeBloomProcessor();
}
