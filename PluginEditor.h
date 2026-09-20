#pragma once
#include "PluginProcessor.h"

// RIVERR look: pure black, hairlines, transparent white buttons, a touch of light blue.
namespace ui
{
    const juce::Colour accent { 0xffa9dcff };   // light blue
    inline juce::Colour white (float a) { return juce::Colours::white.withAlpha (a); }
    inline juce::Font font (float h = 10.f, float kern = 0.12f)
    {
        return juce::Font (juce::FontOptions (h)).withExtraKerningFactor (kern);
    }

    constexpr int kLanes = 6;
    constexpr int kRowH[kLanes] = { 20, 60, 38, 38, 30, 30 };
    constexpr int kRowGap = 4;
    constexpr int kPad = 4;
    constexpr int rowTop (int lane) { int y = kPad; for (int i = 0; i < lane; ++i) y += kRowH[i] + kRowGap; return y; }
    constexpr int kRowsEnd = 4 + 20 + 4 + 60 + 4 + 38 + 4 + 38 + 4 + 30 + 4 + 30;   // 240
    constexpr int kColH = kRowsEnd + 16;
}

//==============================================================================
class RiverrLnF : public juce::LookAndFeel_V4
{
public:
    RiverrLnF();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos,
                           float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
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

// tiny horizontal number box used for lane lengths
class LenBox : public juce::Slider
{
public:
    LenBox (juce::AudioProcessorValueTreeState& a, const juce::String& id)
        : juce::Slider (juce::Slider::LinearBar, juce::Slider::NoTextBox), att (a, id, *this)
    {
        setDoubleClickReturnValue (true, 16.0);
        setMouseDragSensitivity (90);
    }

private:
    juce::AudioProcessorValueTreeState::SliderAttachment att;
};

//==============================================================================
class WaveformView : public juce::Component, private juce::Timer
{
public:
    explicit WaveformView (RiverrProcessor&);
    ~WaveformView() override { stopTimer(); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void setDragOver (bool b) { dragOver = b; repaint(); }

private:
    void timerCallback() override { repaint(); }
    float normToX (float n) const;

    RiverrProcessor& proc;
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
    void setState (int playMask, int dimMask);

private:
    int idx;
    int playMask = 0, dimMask = 0;
    juce::ToggleButton on;
    juce::Slider pit  { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::Slider vel  { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::Slider gate { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::Slider prob { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::Slider rat  { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::AudioProcessorValueTreeState::ButtonAttachment aOn;
    juce::AudioProcessorValueTreeState::SliderAttachment aPit, aVel, aGate, aProb, aRat;
};

//==============================================================================
class RiverrEditor : public juce::AudioProcessorEditor,
                     public juce::FileDragAndDropTarget,
                     private juce::ChangeListener,
                     private juce::Timer
{
public:
    explicit RiverrEditor (RiverrProcessor&);
    ~RiverrEditor() override;

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

    RiverrLnF laf;
    RiverrProcessor& proc;

    WaveformView wave;
    juce::ComboBox presetBox;
    juce::TextButton saveBtn { "SAVE" }, loadBtn { "LOAD SOUND" }, randBtn { "RANDOMIZE" };
    std::unique_ptr<juce::FileChooser> chooser;

    juce::OwnedArray<juce::Component> owned;
    std::vector<std::pair<juce::Component*, int>> arpRow, soundRow, fxRow;
    std::vector<std::unique_ptr<StepColumn>> steps;
    std::vector<std::unique_ptr<LenBox>> lenBoxes;

    int yArp = 0, ySeq = 0, ySound = 0, lanesTop = 0, fxX = 0, contentW = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RiverrEditor)
};
