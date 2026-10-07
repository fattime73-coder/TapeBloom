#pragma once
#include "VoiceBank.h"
#include <JuceHeader.h>
#include <future>
class TapeBloomProcessor : public juce::AudioProcessor, private juce::Timer
{
  public:
    TapeBloomProcessor();
    ~TapeBloomProcessor() override;
    juce::AudioProcessorValueTreeState params;
    juce::MidiKeyboardState keyboard;
    juce::AudioFormatManager formats;
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
    bool isBusesLayoutSupported(const BusesLayout &) const override;
    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "TapeBloom"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 10; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "User Tape"; }
    void changeProgramName(int, const juce::String &) override {}
    void getStateInformation(juce::MemoryBlock &) override;
    void setStateInformation(const void *, int) override;
    bool startRecording();
    void stopRecording();
    void loadAudio(const juce::File &);
    void reanalyse();
    void editLoop(double start, double end);
    void panic();
    std::shared_ptr<const tape::Sample> snapshot();
    juce::String statusText();
    std::atomic<bool> recording{false}, busy{false};
    std::atomic<float> inputLevel{0}, outputLevel{0}, seconds{0};
    std::atomic<int> activeVoices{0};

  private:
    tape::VoiceBank bank;
    std::shared_ptr<const tape::Sample> sample;
    std::vector<float> capture;
    int captureCount = 0;
    double sr = 44100, phase = 0;
    juce::String status = "Ready — record a sustained note or import audio";
    juce::CriticalSection statusLock;
    struct Result
    {
        std::shared_ptr<tape::Sample> sample;
        juce::String error;
    };
    std::future<Result> pending;
    std::atomic<bool> discardAnalysis{false};
    std::array<tape::Biquad, 4> filters;
    juce::Random random;
    juce::SmoothedValue<float> gain;
    void timerCallback() override;
    void beginAnalysis(std::vector<float>, double);
    void install(std::shared_ptr<tape::Sample>);
    void message(const juce::String &);
    void midi(const juce::MidiMessage &);
    float value(const char *id) const { return params.getRawParameterValue(id)->load(); }
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TapeBloomProcessor)
};
