#pragma once
#include "PluginProcessor.h"

namespace ui
{
    const juce::Colour bg      { 0xff09090f };
    const juce::Colour panel   { 0xff14121f };
    const juce::Colour panel2  { 0xff1c1a2b };
    const juce::Colour accent  { 0xff8a6bff };   // violet
    const juce::Colour accent2 { 0xff3fd0c9 };   // teal
    const juce::Colour text    { 0xffc9c5da };
    const juce::Colour dim     { 0xff6d6a80 };
}

//==============================================================================
class DarkLnF : public juce::LookAndFeel_V4
{
public:
    DarkLnF();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos,
                           float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;
};

//==============================================================================
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& id, const juce::String& text,
          const juce::String& suffix = {});
    void resized() override;

private:
    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

class Combo : public juce::Component
{
public:
    Combo (juce::AudioProcessorValueTreeState&, const juce::String& id, const juce::String& text);
    void resized() override;

private:
    juce::ComboBox box;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};

class Pad : public juce::ToggleButton
{
public:
    Pad (juce::AudioProcessorValueTreeState& a, const juce::String& id, const juce::String& caption)
        : juce::ToggleButton (caption), att (a, id, *this) {}

private:
    juce::AudioProcessorValueTreeState::ButtonAttachment att;
};

//==============================================================================
class WaveformView : public juce::Component, private juce::Timer
{
public:
    explicit WaveformView (DarkArpProcessor&);
    ~WaveformView() override { stopTimer(); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void setDragOver (bool b) { dragOver = b; repaint(); }

private:
    void timerCallback() override { repaint(); }
    float normToX (float n) const;

    DarkArpProcessor& proc;
    juce::RangedAudioParameter *startP, *endP;
    int dragging = 0;   // 0 none, 1 start, 2 end
    bool dragOver = false;
};

//==============================================================================
class StepColumn : public juce::Component
{
public:
    StepColumn (juce::AudioProcessorValueTreeState&, int index);
    void resized() override;
    void paint (juce::Graphics&) override;
    void setPlaying (bool b) { if (b != playing) { playing = b; repaint(); } }

private:
    int idx;
    bool playing = false;
    juce::ToggleButton on;
    juce::Slider pit { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::Slider vel { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::Slider prob { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::AudioProcessorValueTreeState::ButtonAttachment aOn;
    juce::AudioProcessorValueTreeState::SliderAttachment aPit, aVel, aProb;
};

//==============================================================================
class DarkArpEditor : public juce::AudioProcessorEditor,
                      public juce::FileDragAndDropTarget,
                      private juce::ChangeListener,
                      private juce::Timer
{
public:
    explicit DarkArpEditor (DarkArpProcessor&);
    ~DarkArpEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { wave.setDragOver (true); }
    void fileDragExit (const juce::StringArray&) override { wave.setDragOver (false); }

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void refreshPresets (const juce::String& select = {});
    void savePresetDialog();
    void openFileChooser();

    template <typename T, typename... Args>
    T* make (Args&&... args)
    {
        auto* c = new T (std::forward<Args> (args)...);
        owned.add (c);
        addAndMakeVisible (c);
        return c;
    }

    DarkLnF laf;
    DarkArpProcessor& proc;

    WaveformView wave;
    juce::ComboBox presetBox;
    juce::TextButton saveBtn { "SAVE" }, loadBtn { "LOAD SOUND" }, randBtn { "RANDOMIZE" };
    std::unique_ptr<juce::FileChooser> chooser;

    juce::OwnedArray<juce::Component> owned;
    std::vector<std::pair<juce::Component*, int>> arpRow, soundRow, fxRow;
    std::vector<std::unique_ptr<StepColumn>> steps;

    juce::Rectangle<int> arpPanel, stepPanel, soundPanel, fxPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DarkArpEditor)
};
