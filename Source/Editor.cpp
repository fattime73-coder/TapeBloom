#include "Editor.h"
using namespace juce;
static Colour cream(0xffe2d4b7), ink(0xff292d2b), green(0xff91b883), amber(0xffefb461);
TapeLook::TapeLook()
{
    setColour(TextButton::buttonColourId, Colour(0xff474a40));
    setColour(TextButton::textColourOffId, cream);
    setColour(Slider::textBoxTextColourId, ink);
    setColour(Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour(Label::textColourId, ink);
    setColour(PopupMenu::backgroundColourId, ink);
    setColour(PopupMenu::textColourId, cream);
}
void TapeLook::drawRotarySlider(Graphics &g, int x, int y, int w, int h, float pos, float from, float to,
                                Slider &)
{
    auto r = Rectangle<float>(float(x), float(y), float(w), float(h)).reduced(10);
    float size = jmin(r.getWidth(), r.getHeight());
    r = r.withSizeKeepingCentre(size, size);
    auto c = r.getCentre();
    float rad = size / 2;
    for (int i = 0; i <= 10; ++i)
    {
        float a = from + (to - from) * i / 10;
        g.setColour(ink.withAlpha(.65f));
        g.drawLine(c.x + std::sin(a) * (rad + 3), c.y - std::cos(a) * (rad + 3),
                   c.x + std::sin(a) * (rad + 6), c.y - std::cos(a) * (rad + 6), 1);
    }
    g.setColour(Colours::black.withAlpha(.25f));
    g.fillEllipse(r.translated(2, 4));
    g.setGradientFill(
        ColourGradient(Colour(0xff50544c), r.getTopLeft(), Colour(0xff171b19), r.getBottomRight(), false));
    g.fillEllipse(r);
    g.setColour(Colour(0xffa19372));
    g.drawEllipse(r, 2);
    g.setColour(cream);
    float a = from + (to - from) * pos;
    g.drawLine(c.x + std::sin(a) * rad * .4f, c.y - std::cos(a) * rad * .4f, c.x + std::sin(a) * rad * .82f,
               c.y - std::cos(a) * rad * .82f, 3);
}
void TapeLook::drawLinearSlider(Graphics &g, int x, int y, int w, int h, float pos, float, float,
                                Slider::SliderStyle, Slider &)
{
    float cx = x + w * .5f;
    g.setColour(ink);
    g.fillRoundedRectangle(cx - 3, float(y), 6, float(h), 3);
    g.setColour(ink.withAlpha(.45f));
    for (int i = 0; i < 9; ++i)
    {
        float yy = y + h * i / 8.f;
        g.drawHorizontalLine(int(yy), cx - 20, cx - 9);
    }
    Rectangle<float> cap(cx - 16, pos - 10, 32, 20);
    g.setColour(ink);
    g.fillRoundedRectangle(cap, 3);
    g.setColour(cream);
    g.drawHorizontalLine(int(pos), cx - 12, cx + 12);
}
TapeBloomEditor::TapeBloomEditor(TapeBloomProcessor &proc)
    : AudioProcessorEditor(proc), p(proc), keys(p.keyboard, MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&look);
    setSize(1100, 740);
    for (auto *b : {&record, &stop, &autoLoop, &audition, &import, &save, &load, &panic, &down, &up})
        addAndMakeVisible(b);
    record.setColour(TextButton::buttonColourId, Colour(0xffb3432a));
    autoLoop.setColour(TextButton::buttonColourId, Colour(0xff526845));
    record.onClick = [this]
    {
        if (playing)
        {
            p.keyboard.noteOff(1, auditionNote, 0);
            playing = false;
        }
        p.startRecording();
    };
    stop.onClick = [this] { p.stopRecording(); };
    autoLoop.onClick = [this] { p.reanalyse(); };
    import.onClick = [this] { choose(false); };
    save.onClick = [this] { choose(true, true); };
    load.onClick = [this] { choose(true); };
    panic.onClick = [this]
    {
        p.panic();
        playing = false;
    };
    audition.onClick = [this]
    {
        if (playing)
            p.keyboard.noteOff(1, auditionNote, 0);
        else
        {
            auditionNote = roundToInt(p.params.getRawParameterValue("root")->load());
            p.keyboard.noteOn(1, auditionNote, .7f);
        }
        playing = !playing;
    };
    down.onClick = [this]
    {
        octave = jmax(0, octave - 12);
        keys.setLowestVisibleKey(octave);
    };
    up.onClick = [this]
    {
        octave = jmin(96, octave + 12);
        keys.setLowestVisibleKey(octave);
    };
    addAndMakeVisible(status);
    status.setColour(Label::textColourId, cream);
    status.setFont(FontOptions(14));
    addAndMakeVisible(keys);
    keys.setAvailableRange(0, 127);
    keys.setLowestVisibleKey(octave);
    keys.setKeyWidth(29);
    keys.setColour(MidiKeyboardComponent::whiteNoteColourId, cream);
    keys.setColour(MidiKeyboardComponent::blackNoteColourId, ink);
    keys.setColour(MidiKeyboardComponent::keyDownOverlayColourId, amber.withAlpha(.8f));
    addControl("attack", "A", true);
    addControl("decay", "D", true);
    addControl("sustain", "S", true);
    addControl("release", "R", true);
    addControl("cutoff", "CUTOFF");
    addControl("resonance", "RESONANCE");
    addControl("wow", "WOW");
    addControl("flutter", "FLUTTER");
    addControl("drive", "DRIVE");
    addControl("noise", "NOISE");
    addControl("low", "LOW");
    addControl("mid", "MID");
    addControl("high", "HIGH");
    addControl("volume", "VOLUME");
    addControl("root", "ROOT MIDI");
    resized();
    startTimerHz(30);
}
TapeBloomEditor::~TapeBloomEditor()
{
    stopTimer();
    if (playing)
        p.keyboard.noteOff(1, auditionNote, 0);
    setLookAndFeel(nullptr);
}
void TapeBloomEditor::addControl(const char *id, const char *text, bool drawbar)
{
    Control c;
    c.slider = std::make_unique<Slider>();
    c.label = std::make_unique<Label>();
    c.slider->setSliderStyle(drawbar ? Slider::LinearVertical : Slider::RotaryHorizontalVerticalDrag);
    c.slider->setTextBoxStyle(Slider::TextBoxBelow, false, 76, 18);
    c.slider->setNumDecimalPlacesToDisplay(2);
    c.slider->setTooltip(String(text) + " — double-click to reset");
    auto *param = p.params.getParameter(id);
    c.slider->setDoubleClickReturnValue(true, param->convertFrom0to1(param->getDefaultValue()));
    c.label->setText(text, dontSendNotification);
    c.label->setFont(FontOptions(12, Font::bold));
    c.label->setJustificationType(Justification::centred);
    addAndMakeVisible(*c.slider);
    addAndMakeVisible(*c.label);
    c.attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.params, id, *c.slider);
    controls.push_back(std::move(c));
}
void TapeBloomEditor::resized()
{
    load.setBounds(865, 29, 70, 32);
    save.setBounds(945, 29, 70, 32);
    panic.setBounds(1018, 29, 60, 32);
    record.setBounds(53, 161, 126, 75);
    stop.setBounds(188, 179, 70, 45);
    autoLoop.setBounds(305, 306, 145, 39);
    audition.setBounds(462, 306, 145, 39);
    import.setBounds(619, 306, 145, 39);
    keys.setBounds(153, 565, 908, 134);
    down.setBounds(43, 586, 90, 36);
    up.setBounds(43, 632, 90, 36);
    status.setBounds(35, 704, 1030, 27);
    if (controls.size() != 15)
        return;
    auto place = [&](int i, int x, int y, int w, int h)
    {
        controls[size_t(i)].label->setBounds(x, y, w, 18);
        controls[size_t(i)].slider->setBounds(x, y + 19, w, h - 19);
    };
    for (int i = 0; i < 4; ++i)
        place(i, 43 + i * 53, 397, 50, 143);
    place(4, 277, 412, 98, 123);
    place(5, 379, 412, 100, 123);
    place(6, 511, 399, 75, 73);
    place(7, 590, 399, 75, 73);
    place(8, 511, 474, 75, 73);
    place(9, 590, 474, 75, 73);
    for (int i = 0; i < 3; ++i)
        place(10 + i, 696 + i * 79, 412, 77, 124);
    place(13, 946, 405, 110, 136);
    place(14, 783, 282, 90, 75);
}
void TapeBloomEditor::paint(Graphics &g)
{
    g.fillAll(Colour(0xff593c2b));
    for (int x = 0; x < 1100; x += 5)
    {
        g.setColour(Colour(0xff251e15).withAlpha(.15f));
        g.drawLine(float(x), 0, float(x + 8), 740, 1);
    }
    g.setColour(cream);
    g.fillRoundedRectangle(22, 15, 1056, 537, 8);
    g.setColour(ink);
    g.setFont(FontOptions("Georgia", 38, Font::bold));
    g.drawText("TapeBloom", 43, 25, 260, 46, Justification::centredLeft);
    g.setFont(FontOptions(11));
    g.drawText("TAPE SAMPLING INSTRUMENT", 313, 37, 230, 24, Justification::centredLeft);
    g.setColour(ink);
    g.fillRoundedRectangle(557, 29, 289, 33, 4);
    g.setColour(cream);
    g.setFont(FontOptions(15));
    g.drawText(display ? "My recorded tape" : "Record your first tape", 562, 31, 279, 29,
               Justification::centred);
    for (auto r : {Rectangle<float>(35, 90, 245, 269), Rectangle<float>(292, 90, 481, 269),
                   Rectangle<float>(785, 90, 280, 185)})
    {
        g.setColour(ink);
        g.fillRoundedRectangle(r, 7);
        g.setColour(Colour(0xff857659));
        g.drawRoundedRectangle(r.reduced(3), 5, 1);
    }
    g.setFont(FontOptions(14, Font::bold));
    g.setColour(cream);
    g.drawText("RECORDING", 53, 102, 160, 30, Justification::centredLeft);
    g.setFont(FontOptions(13));
    g.drawText(String(p.seconds.load(), 1) + " / 30 s", 54, 256, 125, 25, Justification::centredLeft);
    g.drawText("INPUT", 201, 273, 60, 24, Justification::centred);
    for (int i = 0; i < 12; ++i)
    {
        float t = (i + 1) / 12.f;
        g.setColour(p.inputLevel.load() > t * t ? (i > 9 ? Colours::orangered : green) : Colour(0xff454a3b));
        g.fillRect(207, 259 - i * 10, 28, 6);
    }
    g.setColour(Colour(0xff0f1b16));
    g.fillRect(wave);
    auto view = display;
    if (view)
    {
        if (!dragging)
        {
            loopA = double(view->loop.start) / view->data.size();
            loopB = double(view->loop.end) / view->data.size();
        }
        float a = wave.getX() + float(loopA) * wave.getWidth(),
              b = wave.getX() + float(loopB) * wave.getWidth();
        g.setColour(green.withAlpha(.18f));
        g.fillRect(Rectangle<float>(a, wave.getY(), b - a, wave.getHeight()));
        g.setColour(green.withAlpha(.9f));
        for (int x = 0; x < 454; ++x)
            g.drawVerticalLine(int(wave.getX()) + x, wave.getCentreY() - waveMax[size_t(x)] * 52,
                               wave.getCentreY() - waveMin[size_t(x)] * 52);
        for (float x : {a, b})
        {
            g.drawVerticalLine(int(x), wave.getY(), wave.getBottom());
            g.fillRoundedRectangle(x - 5, wave.getY() - 5, 10, 19, 3);
        }
        g.setFont(FontOptions(12));
        g.drawText("LOOP " + String((view->loop.end - view->loop.start) / view->rate, 2) +
                       " s    CROSSFADE " + String(view->loop.fade / view->rate * 1000, 0) + " ms",
                   307, 270, 454, 23, Justification::centredLeft);
    }
    else
    {
        g.setColour(green);
        g.setFont(FontOptions(15));
        g.drawText(p.busy ? "ANALYSING…" : "Record a steady note, or drop an audio file", wave,
                   Justification::centred);
    }
    g.setColour(cream);
    g.setFont(FontOptions(13));
    g.drawText("TAPE TRANSPORT", 807, 99, 235, 22, Justification::centred);
    for (float cx : {859.f, 992.f})
    {
        g.setColour(Colour(0xffad9d7c));
        g.fillEllipse(cx - 42, 130, 84, 84);
        g.setColour(Colour(0xff3b3529));
        g.fillEllipse(cx - 10, 162, 20, 20);
        for (int i = 0; i < 3; ++i)
        {
            double a = angle + 2 * tape::pi * i / 3;
            float x = cx + float(std::cos(a)) * 25, y = 172 + float(std::sin(a)) * 25;
            g.fillEllipse(x - 12, y - 9, 24, 18);
        }
    }
    g.setColour(amber);
    g.fillRoundedRectangle(811, 231, 226, 22, 3);
    g.setColour(ink);
    g.fillRect(816, 236, int(jlimit(0.f, 1.f, p.outputLevel.load()) * 216), 12);
    g.setColour(ink);
    g.setFont(FontOptions(14, Font::bold));
    g.drawText("ENVELOPE", 44, 370, 220, 22, Justification::centredLeft);
    g.drawText("FILTER", 280, 370, 180, 22, Justification::centredLeft);
    g.drawText("TAPE", 517, 370, 150, 22, Justification::centredLeft);
    g.drawText("EQ", 703, 370, 190, 22, Justification::centredLeft);
    g.setColour(Colour(0xff9d9079));
    for (int x : {264, 495, 681, 938})
        g.drawVerticalLine(x, 373, 538);
    g.setColour(ink);
    g.fillRoundedRectangle(25, 556, 1050, 180, 5);
}
void TapeBloomEditor::timerCallback()
{
    auto current = p.snapshot();
    if (current != display)
    {
        display = current;
        waveMin.fill(0);
        waveMax.fill(0);
        if (display)
            for (size_t i = 0; i < display->data.size(); ++i)
            {
                size_t x = std::min(size_t(453), i * 454 / display->data.size());
                waveMin[x] = std::min(waveMin[x], display->data[i]);
                waveMax[x] = std::max(waveMax[x], display->data[i]);
            }
    }
    status.setText(p.statusText() + "   |   " + String(p.activeVoices.load()) + " / 32 voices",
                   dontSendNotification);
    record.setEnabled(!p.busy && !p.recording);
    stop.setEnabled(p.recording);
    import.setEnabled(!p.busy && !p.recording);
    autoLoop.setEnabled(display != nullptr && !p.busy && !p.recording);
    load.setEnabled(!p.busy && !p.recording);
    audition.setEnabled(display != nullptr && !p.recording && !p.busy);
    audition.setButtonText(playing ? "RELEASE" : "AUDITION");
    if (p.activeVoices > 0 || p.recording)
        angle += .055;
    repaint();
}
void TapeBloomEditor::choose(bool preset, bool write)
{
    chooser = std::make_unique<FileChooser>(write ? "Save tape and settings" : "Open audio or tape", File{},
                                            preset ? "*.tapebloom" : "*.wav;*.aif;*.aiff;*.flac");
    auto safe = Component::SafePointer<TapeBloomEditor>(this);
    int flags = (write ? FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting
                       : FileBrowserComponent::openMode) |
                FileBrowserComponent::canSelectFiles;
    chooser->launchAsync(
        flags,
        [safe, preset, write](const FileChooser &fc)
        {
            if (!safe)
                return;
            auto file = fc.getResult();
            if (file == File{})
                return;
            if (!preset)
            {
                safe->p.loadAudio(file);
                return;
            }
            if (write)
            {
                file = file.withFileExtension("tapebloom");
                MemoryBlock bytes;
                safe->p.getStateInformation(bytes);
                if (!file.replaceWithData(bytes.getData(), bytes.getSize()))
                    AlertWindow::showMessageBoxAsync(MessageBoxIconType::WarningIcon, "Save failed",
                                                     "Could not write this file.");
            }
            else
            {
                MemoryBlock bytes;
                if (file.getSize() > 48 * 1024 * 1024 || !file.loadFileAsData(bytes))
                {
                    AlertWindow::showMessageBoxAsync(MessageBoxIconType::WarningIcon, "Load failed",
                                                     "Could not read this preset.");
                    return;
                }
                safe->p.setStateInformation(bytes.getData(), int(bytes.getSize()));
            }
        });
}
bool TapeBloomEditor::isInterestedInFileDrag(const StringArray &f)
{
    return f.size() == 1 && (f[0].endsWithIgnoreCase(".wav") || f[0].endsWithIgnoreCase(".aif") ||
                             f[0].endsWithIgnoreCase(".aiff") || f[0].endsWithIgnoreCase(".flac"));
}
void TapeBloomEditor::filesDropped(const StringArray &f, int, int)
{
    if (f.size())
        p.loadAudio(File(f[0]));
}
void TapeBloomEditor::mouseDown(const MouseEvent &e)
{
    if (!display || !wave.expanded(8).contains(e.position) || p.busy || p.recording)
        return;
    double x = (e.position.x - wave.getX()) / wave.getWidth();
    dragging = std::abs(x - loopA) < std::abs(x - loopB) ? 1 : 2;
    mouseDrag(e);
}
void TapeBloomEditor::mouseDrag(const MouseEvent &e)
{
    if (!dragging || !display)
        return;
    double x = jlimit(0., 1., double((e.position.x - wave.getX()) / wave.getWidth()));
    double gap = jmax(.005, 128. / display->data.size());
    if (dragging == 1)
        loopA = jmin(x, loopB - gap);
    else
        loopB = jmax(x, loopA + gap);
    repaint();
}
void TapeBloomEditor::mouseUp(const MouseEvent &)
{
    if (dragging)
    {
        p.editLoop(loopA, loopB);
        dragging = 0;
    }
}
