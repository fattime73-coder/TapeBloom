#pragma once
#include "Processor.h"
class TapeLook : public juce::LookAndFeel_V4
{
  public:
    TapeLook();
    void drawRotarySlider(juce::Graphics &, int, int, int, int, float, float, float, juce::Slider &) override;
    void drawLinearSlider(juce::Graphics &, int, int, int, int, float, float, float,
                          juce::Slider::SliderStyle, juce::Slider &) override;
};
class TapeBloomEditor : public juce::AudioProcessorEditor,
                        private juce::Timer,
                        public juce::FileDragAndDropTarget
{
  public:
    explicit TapeBloomEditor(TapeBloomProcessor &);
    ~TapeBloomEditor() override;
    void paint(juce::Graphics &) override;
    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray &) override;
    void filesDropped(const juce::StringArray &, int, int) override;
    void mouseDown(const juce::MouseEvent &) override;
    void mouseDrag(const juce::MouseEvent &) override;
    void mouseUp(const juce::MouseEvent &) override;

  private:
    TapeBloomProcessor &p;
    TapeLook look;
    juce::MidiKeyboardComponent keys;
    juce::TextButton record{"RECORD"}, stop{"STOP"}, autoLoop{"AUTO LOOP"}, audition{"AUDITION"},
        import{"IMPORT"}, save{"SAVE"}, load{"LOAD"}, panic{"PANIC"}, down{"OCT −"}, up{"OCT +"};
    juce::Label status;
    struct Control
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    std::vector<Control> controls;
    std::unique_ptr<juce::FileChooser> chooser;
    std::shared_ptr<const tape::Sample> display;
    std::array<float, 454> waveMin{}, waveMax{};
    juce::Rectangle<float> wave{307, 132, 454, 129};
    double loopA = 0, loopB = 1, angle = 0;
    int dragging = 0, octave = 48, auditionNote = 60;
    bool playing = false;
    void addControl(const char *, const char *, bool = false);
    void choose(bool preset, bool write = false);
    void timerCallback() override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TapeBloomEditor)
};
