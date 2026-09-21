#pragma once
#include "PluginProcessor.h"

// RIVERR look: dark retro device - gunmetal body, CRT screens, keycaps, light-blue glow.
namespace ui
{
    const juce::Colour accent { 0xffa9dcff };   // light blue
    inline juce::Colour white (float a) { return juce::Colours::white.withAlpha (a); }
    inline juce::Font font (float h = 10.f, float kern = 0.12f)
    {
        return juce::Font (juce::FontOptions (h)).withExtraKerningFactor (kern);
    }
    inline juce::Font mono (float h, bool bold = false)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h,
                                              bold ? juce::Font::bold : juce::Font::plain));
    }

    constexpr int kW = 1280, kH = 784;

    // EDIT page lanes
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
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
};

void applyNiceText (juce::Slider&, juce::AudioProcessorValueTreeState&, const juce::String& id, const juce::String& suffix);

//==============================================================================
// dev: 0 = minimal ring, 1 = big device knob, 2 = small device knob
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& id, const juce::String& text,
          const juce::String& suffix = {}, int dev = 0);
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

class PowerButton : public juce::ToggleButton
{
public:
    PowerButton (juce::AudioProcessorValueTreeState& a, const juce::String& id)
        : juce::ToggleButton ("POWER"), att (a, id, *this) { getProperties().set ("power", true); }

private:
    juce::AudioProcessorValueTreeState::ButtonAttachment att;
};

class KeyButton : public juce::TextButton
{
public:
    explicit KeyButton (const juce::String& text, int kind = 1) : juce::TextButton (text)
    {
        getProperties().set ("key", kind);
    }
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

// "label: value" cell on a CRT screen
class CellSlider : public juce::Slider
{
public:
    CellSlider (juce::AudioProcessorValueTreeState& a, const juce::String& id, const juce::String& label)
        : juce::Slider (juce::Slider::LinearBar, juce::Slider::NoTextBox), att (a, id, *this)
    {
        getProperties().set ("cell", true);
        getProperties().set ("label", label);
        setMouseDragSensitivity (160);
        applyNiceText (*this, a, id, {});
    }

private:
    juce::AudioProcessorValueTreeState::SliderAttachment att;
};

class CellCombo : public juce::ComboBox
{
public:
    CellCombo (juce::AudioProcessorValueTreeState& a, const juce::String& id, const juce::String& label);

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};

//==============================================================================
class WaveformView : public juce::Component, private juce::Timer
{
public:
    WaveformView (RiverrProcessor&, int slot);
    ~WaveformView() override { stopTimer(); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void setDragOver (bool b) { dragOver = b; repaint(); }

private:
    void timerCallback() override { if (isShowing()) repaint(); }
    float normToX (float n) const;

    RiverrProcessor& proc;
    int slot;
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
// animated orb + step ring on the PLAY screen
class PlayScreen : public juce::Component
{
public:
    explicit PlayScreen (RiverrProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics&) override;
    void setState (double tt, float pl, int st, const juce::String& name) { t = tt; pulse = pl; step = st; presetName = name; }

private:
    RiverrProcessor& proc;
    double t = 0.0;
    float pulse = 0.f;
    int step = -1;
    juce::String presetName;
};

// one effect on the FX page: animated screen + knobs + cells
class FxPanel : public juce::Component
{
public:
    // types: 0 filter, 1 drive, 2 tape, 3 halftime, 4 halfspeed, 5 delay, 6 reverb, 7 glue
    FxPanel (RiverrProcessor&, int type);
    void paint (juce::Graphics&) override;
    void resized() override;
    void setTime (double tt) { t = tt; }
    static juce::String titleOf (int type);

private:
    void drawAnim (juce::Graphics&, juce::Rectangle<float>);
    float v (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }

    template <typename T, typename... Args>
    T* make (Args&&... args)
    {
        auto* c = new T (std::forward<Args> (args)...);
        owned.add (c);
        addAndMakeVisible (c);
        return c;
    }

    RiverrProcessor& proc;
    int type;
    double t = 0.0;
    juce::String title, blurb;
    juce::OwnedArray<juce::Component> owned;
    Knob* big = nullptr;
    std::vector<Knob*> smalls;
    std::vector<juce::Component*> cells;
    juce::Component* power = nullptr;
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
    void fileDragEnter (const juce::StringArray&, int, int) override { setDragHighlight (true); }
    void fileDragExit (const juce::StringArray&) override { setDragHighlight (false); }

    void setPage (int p);   // 0 play, 1 edit, 2 fx
    void selectFx (int type);
    void advance (double seconds);   // used by the offscreen test to fake the passage of time

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void buildImages();
    void refreshPresets (const juce::String& select = {});
    void savePresetDialog();
    void openFileChooser (int slot);
    void setDragHighlight (bool b);
    void pressPlayKey (int i);
    bool fxIsOn (int type) const;

    template <typename T, typename... Args>
    T* make (Args&&... args)
    {
        auto* c = new T (std::forward<Args> (args)...);
        owned.add (c);
        addAndMakeVisible (c);
        if (curList != nullptr) curList->push_back (c);
        return c;
    }

    RiverrLnF laf;
    RiverrProcessor& proc;

    juce::Image shell, screenImg, editImg;
    int page = 0, fxSel = 1, activeKey = 0, lastHits = 0;
    double t = 0.0;
    float pulse = 0.f;
    juce::String presetName { "HARD BELL" };

    // header
    KeyButton tabPlay { "PLAY", 2 }, tabEdit { "EDIT", 2 }, tabFx { "FX", 2 };
    juce::ComboBox presetBox;
    juce::TextButton saveBtn { "SAVE" }, loadABtn { "LOAD A" }, loadBBtn { "LOAD B" }, resetBtn { "RESET" };
    std::unique_ptr<juce::FileChooser> chooser;

    juce::OwnedArray<juce::Component> owned;

    // play page
    PlayScreen playScreen;
    WaveformView playWaveA, playWaveB;
    std::vector<std::unique_ptr<KeyButton>> playKeys;
    std::vector<Knob*> macroKnobs;
    Knob* gainKnob = nullptr;

    // edit page
    WaveformView editWaveA, editWaveB;
    std::vector<std::pair<juce::Component*, int>> arpRow, soundRow, filterRow, interactRow;
    std::vector<Knob*> slotA, slotB;
    std::vector<std::unique_ptr<StepColumn>> steps;
    std::vector<std::unique_ptr<LenBox>> lenBoxes;
    juce::TextButton randBtn { "RANDOMIZE" }, diceBtn { "DICE HARD" };
    int yArp = 0, ySeq = 0, ySound = 0, lanesTop = 0, filterX = 0, interactX = 0, editW = 0;

    // fx page
    std::vector<std::unique_ptr<KeyButton>> fxKeys;
    std::vector<std::unique_ptr<FxPanel>> fxPanels;

    std::vector<juce::Component*> playComps, editComps, fxComps;
    std::vector<juce::Component*>* curList = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RiverrEditor)
};
