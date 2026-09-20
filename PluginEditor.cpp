#include "PluginEditor.h"

using namespace juce;

namespace
{
    constexpr int kW = 1280, kH = 700;

    String stepId (int i, const char* what) { return "s" + String (i) + "_" + what; }

    bool isAudioFile (const String& path)
    {
        return File (path).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg");
    }
}

//==============================================================================
DarkLnF::DarkLnF()
{
    setColour (ResizableWindow::backgroundColourId, ui::bg);
    setColour (Label::textColourId, ui::text);
    setColour (ComboBox::backgroundColourId, ui::panel2);
    setColour (ComboBox::textColourId, ui::text);
    setColour (ComboBox::outlineColourId, Colours::transparentBlack);
    setColour (ComboBox::arrowColourId, ui::accent);
    setColour (PopupMenu::backgroundColourId, ui::panel);
    setColour (PopupMenu::textColourId, ui::text);
    setColour (PopupMenu::highlightedBackgroundColourId, ui::accent.withAlpha (0.35f));
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (Slider::textBoxTextColourId, ui::dim);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (TextButton::buttonColourId, ui::panel2);
    setColour (TextButton::textColourOffId, ui::text);
    setColour (AlertWindow::backgroundColourId, ui::panel);
    setColour (AlertWindow::textColourId, ui::text);
    setColour (TextEditor::backgroundColourId, ui::bg);
    setColour (TextEditor::textColourId, ui::text);
}

void DarkLnF::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                float startAngle, float endAngle, Slider& s)
{
    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.f);
    const float d = jmin (b.getWidth(), b.getHeight());
    const auto c = b.getCentre();
    const float r = d * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;

    Path track;
    track.addCentredArc (c.x, c.y, r - 3.f, r - 3.f, 0.f, startAngle, endAngle, true);
    g.setColour (Colour (0xff272438));
    g.strokePath (track, PathStrokeType (3.f, PathStrokeType::curved, PathStrokeType::rounded));

    const float zeroAngle = bipolar ? startAngle + (float) s.valueToProportionOfLength (0.0) * (endAngle - startAngle)
                                    : startAngle;
    Path val;
    val.addCentredArc (c.x, c.y, r - 3.f, r - 3.f, 0.f, jmin (zeroAngle, angle), jmax (zeroAngle, angle), true);
    g.setColour (s.isEnabled() ? ui::accent : ui::dim);
    g.strokePath (val, PathStrokeType (3.f, PathStrokeType::curved, PathStrokeType::rounded));

    g.setColour (ui::panel2);
    g.fillEllipse (c.x - r * 0.55f, c.y - r * 0.55f, r * 1.1f, r * 1.1f);

    Path p;
    p.addRoundedRectangle (-1.5f, -r * 0.55f, 3.f, r * 0.42f, 1.f);
    g.setColour (ui::text);
    g.fillPath (p, AffineTransform::rotation (angle).translated (c.x, c.y));
}

void DarkLnF::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float,
                                float minPos, float maxPos, Slider::SliderStyle style, Slider& s)
{
    if (style != Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, 0.f, minPos, maxPos, style, s);
        return;
    }

    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced ((float) w * 0.25f, 1.f);
    g.setColour (Colour (0xff1e1c2c));
    g.fillRoundedRectangle (b, 3.f);

    const float prop = (float) s.valueToProportionOfLength (s.getValue());
    const bool bipolar = s.getMinimum() < 0.0;
    const float yv = b.getBottom() - prop * b.getHeight();
    const float y0 = bipolar ? b.getBottom() - (float) s.valueToProportionOfLength (0.0) * b.getHeight()
                             : b.getBottom();

    g.setColour (s.isEnabled() ? ui::accent2.withAlpha (0.85f) : ui::dim);
    g.fillRect (b.getX(), jmin (yv, y0), b.getWidth(), jmax (1.f, std::abs (yv - y0)));

    g.setColour (Colours::white.withAlpha (0.9f));
    g.fillRect (b.getX() - 1.f, yv - 1.5f, b.getWidth() + 2.f, 3.f);
}

Slider::SliderLayout DarkLnF::getSliderLayout (Slider& s)
{
    if (s.getSliderStyle() == Slider::LinearVertical)
    {
        Slider::SliderLayout l;
        l.sliderBounds = s.getLocalBounds();
        return l;
    }
    return LookAndFeel_V4::getSliderLayout (s);
}

void DarkLnF::drawToggleButton (Graphics& g, ToggleButton& btn, bool highlighted, bool)
{
    auto b = btn.getLocalBounds().toFloat().reduced (1.5f);
    const bool on = btn.getToggleState();

    g.setColour (on ? ui::accent.withAlpha (0.92f) : Colour (0xff211f30));
    g.fillRoundedRectangle (b, 5.f);
    if (highlighted)
    {
        g.setColour (Colours::white.withAlpha (0.08f));
        g.fillRoundedRectangle (b, 5.f);
    }

    if (btn.getButtonText().isNotEmpty())
    {
        g.setColour (on ? Colours::black : ui::text);
        g.setFont (11.5f);
        g.drawText (btn.getButtonText(), b, Justification::centred);
    }
}

//==============================================================================
Knob::Knob (AudioProcessorValueTreeState& a, const String& id, const String& text, const String& suffix)
{
    label.setText (text, dontSendNotification);
    label.setJustificationType (Justification::centred);
    label.setFont (Font (FontOptions (11.f)));
    label.setColour (Label::textColourId, ui::text);
    addAndMakeVisible (label);

    slider.setColour (Slider::textBoxTextColourId, ui::dim);
    slider.setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    slider.setColour (Slider::textBoxHighlightColourId, ui::accent.withAlpha (0.4f));
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, 56, 15);
    addAndMakeVisible (slider);
    att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (a, id, slider);

    if (auto* p = a.getParameter (id))
    {
        const bool isInt = dynamic_cast<AudioParameterInt*> (p) != nullptr;
        bool isPercent = false;
        if (auto* fp = dynamic_cast<RangedAudioParameter*> (p))
            isPercent = fp->getNormalisableRange().end <= 1.f;

        slider.textFromValueFunction = [isInt, isPercent, suffix] (double v)
        {
            String t;
            if (isInt)                    t = String (roundToInt (v));
            else if (isPercent)           t = String (roundToInt (v * 100.0)) + "%";
            else if (suffix == "s" && v < 1.0) return String (roundToInt (v * 1000.0)) + " ms";
            else if (std::abs (v) >= 1000.0) t = String (roundToInt (v));
            else if (std::abs (v) >= 100.0)  t = String (v, 0);
            else if (std::abs (v) >= 10.0)   t = String (v, 1);
            else                             t = String (v, 2);
            return suffix.isEmpty() ? t : t + " " + suffix;
        };
        slider.valueFromTextFunction = [isPercent] (const String& t)
        {
            const double v = t.getDoubleValue();
            return isPercent ? v / 100.0 : v;
        };
        slider.updateText();
    }
}

void Knob::resized()
{
    auto b = getLocalBounds();
    label.setBounds (b.removeFromTop (14));
    slider.setBounds (b);
}

Combo::Combo (AudioProcessorValueTreeState& a, const String& id, const String& text)
{
    label.setText (text, dontSendNotification);
    label.setJustificationType (Justification::centred);
    label.setFont (Font (FontOptions (11.f)));
    addAndMakeVisible (label);

    if (auto* p = dynamic_cast<AudioParameterChoice*> (a.getParameter (id)))
        box.addItemList (p->choices, 1);
    addAndMakeVisible (box);
    att = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (a, id, box);
}

void Combo::resized()
{
    auto b = getLocalBounds();
    label.setBounds (b.removeFromTop (16));
    b.removeFromTop (4);
    box.setBounds (b.removeFromTop (28));
}

//==============================================================================
WaveformView::WaveformView (DarkArpProcessor& p) : proc (p)
{
    startP = p.apvts.getParameter ("start");
    endP = p.apvts.getParameter ("end");
    startTimerHz (20);
}

float WaveformView::normToX (float n) const
{
    return 6.f + n * ((float) getWidth() - 12.f);
}

void WaveformView::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (Colour (0xff0d0c16));
    g.fillRoundedRectangle (b, 8.f);

    if (dragOver)
    {
        g.setColour (ui::accent2.withAlpha (0.18f));
        g.fillRoundedRectangle (b, 8.f);
        g.setColour (ui::accent2);
        g.drawRoundedRectangle (b.reduced (1.f), 8.f, 2.f);
    }

    auto s = proc.getSample();
    if (s == nullptr)
    {
        g.setColour (ui::dim);
        g.setFont (17.f);
        g.drawText ("Drop a one-shot here   -   or press LOAD SOUND", b, Justification::centred);
        return;
    }

    const float mid = b.getCentreY() + 4.f;
    const float half = b.getHeight() * 0.38f;
    const float x0 = normToX (0.f), x1 = normToX (1.f);
    const int cols = (int) s->peakMax.size();

    Path p;
    for (int i = 0; i <= (int) (x1 - x0); ++i)
    {
        const int c = jlimit (0, cols - 1, (int) ((float) i / (x1 - x0) * (float) cols));
        const float y = mid - s->peakMax[(size_t) c] * half;
        if (i == 0) p.startNewSubPath (x0, y); else p.lineTo (x0 + (float) i, y);
    }
    for (int i = (int) (x1 - x0); i >= 0; --i)
    {
        const int c = jlimit (0, cols - 1, (int) ((float) i / (x1 - x0) * (float) cols));
        p.lineTo (x0 + (float) i, mid - s->peakMin[(size_t) c] * half);
    }
    p.closeSubPath();

    g.setGradientFill (ColourGradient (ui::accent.withAlpha (0.95f), 0.f, b.getY(),
                                       ui::accent2.withAlpha (0.55f), 0.f, b.getBottom(), false));
    g.fillPath (p);

    const float sx = normToX (startP->getValue()), ex = normToX (endP->getValue());
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillRect (x0, b.getY(), jmax (0.f, sx - x0), b.getHeight());
    g.fillRect (ex, b.getY(), jmax (0.f, x1 - ex), b.getHeight());

    g.setColour (ui::accent2);
    g.drawLine (sx, b.getY(), sx, b.getBottom(), 2.f);
    g.drawLine (ex, b.getY(), ex, b.getBottom(), 2.f);
    g.fillRect (sx, b.getY(), 36.f, 14.f);
    g.fillRect (ex - 30.f, b.getY(), 30.f, 14.f);
    g.setColour (Colours::black);
    g.setFont (10.f);
    g.drawText ("START", Rectangle<float> (sx, b.getY(), 36.f, 14.f), Justification::centred);
    g.drawText ("END", Rectangle<float> (ex - 30.f, b.getY(), 30.f, 14.f), Justification::centred);

    g.setColour (ui::text);
    g.setFont (12.5f);
    g.drawText (s->info, b.reduced (12.f, 4.f), Justification::bottomLeft);
    g.setColour (ui::dim);
    g.drawText (s->name, b.reduced (12.f, 4.f), Justification::bottomRight);
}

void WaveformView::mouseDown (const MouseEvent& e)
{
    if (proc.getSample() == nullptr) return;
    const float dS = std::abs (e.position.x - normToX (startP->getValue()));
    const float dE = std::abs (e.position.x - normToX (endP->getValue()));

    dragging = 0;
    if (jmin (dS, dE) < 12.f)
        dragging = dS <= dE ? 1 : 2;

    if (dragging == 1) startP->beginChangeGesture();
    if (dragging == 2) endP->beginChangeGesture();
}

void WaveformView::mouseDrag (const MouseEvent& e)
{
    if (dragging == 0) return;
    const float n = jlimit (0.f, 1.f, (e.position.x - 6.f) / ((float) getWidth() - 12.f));

    if (dragging == 1) startP->setValueNotifyingHost (jmin (n, endP->getValue() - 0.01f));
    else               endP->setValueNotifyingHost (jmax (n, startP->getValue() + 0.01f));
}

void WaveformView::mouseUp (const MouseEvent&)
{
    if (dragging == 1) startP->endChangeGesture();
    if (dragging == 2) endP->endChangeGesture();
    dragging = 0;
}

//==============================================================================
StepColumn::StepColumn (AudioProcessorValueTreeState& a, int index)
    : idx (index),
      aOn (a, stepId (index, "on"), on),
      aPit (a, stepId (index, "pit"), pit),
      aVel (a, stepId (index, "vel"), vel),
      aProb (a, stepId (index, "prob"), prob)
{
    addAndMakeVisible (on);
    for (auto* s : { &pit, &vel, &prob })
    {
        addAndMakeVisible (s);
        s->setPopupDisplayEnabled (true, true, nullptr);
    }
    pit.setDoubleClickReturnValue (true, 0.0);
    vel.setDoubleClickReturnValue (true, 0.85);
    prob.setDoubleClickReturnValue (true, 1.0);
}

void StepColumn::resized()
{
    auto b = getLocalBounds().reduced (6, 0);
    on.setBounds (b.removeFromTop (22));   b.removeFromTop (4);
    pit.setBounds (b.removeFromTop (72));  b.removeFromTop (4);
    vel.setBounds (b.removeFromTop (46));  b.removeFromTop (4);
    prob.setBounds (b.removeFromTop (34));
}

void StepColumn::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.f, 0.f);
    if ((idx / 4) % 2 == 0)
    {
        g.setColour (ui::panel2.withAlpha (0.55f));
        g.fillRoundedRectangle (b, 5.f);
    }
    if (playing)
    {
        g.setColour (ui::accent2);
        g.drawRoundedRectangle (b.reduced (0.5f), 5.f, 1.6f);
    }

    g.setColour (playing ? ui::accent2 : ui::dim);
    g.setFont (11.f);
    g.drawText (String (idx + 1), getLocalBounds().removeFromBottom (14), Justification::centred);
}

//==============================================================================
DarkArpEditor::DarkArpEditor (DarkArpProcessor& p)
    : AudioProcessorEditor (&p), proc (p), wave (p)
{
    setLookAndFeel (&laf);
    auto& A = p.apvts;

    addAndMakeVisible (wave);

    // header
    addAndMakeVisible (presetBox);
    presetBox.setTextWhenNothingSelected ("Presets");
    presetBox.onChange = [this]
    {
        const auto name = presetBox.getText();
        if (name.isNotEmpty()) proc.loadPreset (name);
    };
    addAndMakeVisible (saveBtn);
    addAndMakeVisible (loadBtn);
    addAndMakeVisible (randBtn);
    saveBtn.onClick = [this] { savePresetDialog(); };
    loadBtn.onClick = [this] { openFileChooser(); };
    randBtn.onClick = [this] { proc.randomizeSteps(); };
    refreshPresets();

    // arp row
    arpRow.push_back ({ make<Pad> (A, "arpOn", "ARP ON"), 84 });
    arpRow.push_back ({ make<Combo> (A, "dir", "DIRECTION"), 120 });
    arpRow.push_back ({ make<Combo> (A, "octRange", "OCTAVES"), 84 });
    arpRow.push_back ({ make<Combo> (A, "rate", "RATE"), 96 });
    arpRow.push_back ({ make<Knob> (A, "steps", "STEPS"), 84 });
    arpRow.push_back ({ make<Knob> (A, "gate", "GATE"), 84 });
    arpRow.push_back ({ make<Knob> (A, "swing", "SWING"), 84 });
    arpRow.push_back ({ make<Knob> (A, "humT", "HUMANIZE TIME"), 100 });
    arpRow.push_back ({ make<Knob> (A, "humV", "HUMANIZE VEL"), 100 });
    arpRow.push_back ({ &randBtn, 120 });

    // step sequencer
    for (int i = 0; i < 16; ++i)
    {
        steps.push_back (std::make_unique<StepColumn> (A, i));
        addAndMakeVisible (*steps.back());
    }

    // sound row
    soundRow.push_back ({ make<Knob> (A, "octave", "OCTAVE"), 58 });
    soundRow.push_back ({ make<Knob> (A, "tune", "TUNE", "st"), 58 });
    soundRow.push_back ({ make<Pad> (A, "autotune", "AUTO C5"), 62 });
    soundRow.push_back ({ make<Pad> (A, "reverse", "REVERSE"), 62 });
    soundRow.push_back ({ make<Knob> (A, "atk", "ATTACK", "s"), 58 });
    soundRow.push_back ({ make<Knob> (A, "dec", "DECAY", "s"), 58 });
    soundRow.push_back ({ make<Knob> (A, "sus", "SUSTAIN"), 58 });
    soundRow.push_back ({ make<Knob> (A, "rel", "RELEASE", "s"), 58 });
    soundRow.push_back ({ make<Knob> (A, "gain", "GAIN", "dB"), 58 });

    // fx row
    fxRow.push_back ({ make<Knob> (A, "cut", "CUTOFF", "Hz"), 56 });
    fxRow.push_back ({ make<Knob> (A, "res", "RESO"), 56 });
    fxRow.push_back ({ make<Knob> (A, "lfoRate", "LFO RATE", "Hz"), 56 });
    fxRow.push_back ({ make<Knob> (A, "lfoDepth", "LFO DEPTH"), 56 });
    fxRow.push_back ({ make<Combo> (A, "dlyTime", "DELAY"), 72 });
    fxRow.push_back ({ make<Knob> (A, "dlyFb", "FEEDBACK"), 56 });
    fxRow.push_back ({ make<Knob> (A, "dlyMix", "DLY MIX"), 56 });
    fxRow.push_back ({ make<Knob> (A, "revSize", "REV SIZE"), 56 });
    fxRow.push_back ({ make<Knob> (A, "revDamp", "REV DAMP"), 56 });
    fxRow.push_back ({ make<Knob> (A, "revMix", "REV MIX"), 56 });

    proc.addChangeListener (this);
    startTimerHz (30);
    setSize (kW, kH);
}

DarkArpEditor::~DarkArpEditor()
{
    stopTimer();
    proc.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void DarkArpEditor::changeListenerCallback (ChangeBroadcaster*)
{
    wave.repaint();
}

void DarkArpEditor::timerCallback()
{
    const int cur = proc.currentStep.load();
    for (int i = 0; i < (int) steps.size(); ++i)
        steps[(size_t) i]->setPlaying (i == cur);
}

//==============================================================================
bool DarkArpEditor::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files)
        if (isAudioFile (f)) return true;
    return false;
}

void DarkArpEditor::filesDropped (const StringArray& files, int, int)
{
    wave.setDragOver (false);
    for (auto& f : files)
        if (isAudioFile (f) && proc.loadSample (File (f)))
            break;
}

void DarkArpEditor::openFileChooser()
{
    chooser = std::make_unique<FileChooser> ("Choose a one-shot", File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    Component::SafePointer<DarkArpEditor> safe (this);
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [safe] (const FileChooser& fc)
                          {
                              if (safe == nullptr) return;
                              const auto f = fc.getResult();
                              if (f.existsAsFile()) safe->proc.loadSample (f);
                          });
}

void DarkArpEditor::refreshPresets (const String& select)
{
    presetBox.clear (dontSendNotification);
    presetBox.addItemList (proc.listPresets(), 1);
    if (select.isNotEmpty())
        presetBox.setText (select, dontSendNotification);
}

void DarkArpEditor::savePresetDialog()
{
    auto* w = new AlertWindow ("Save preset", "Preset name:", MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", presetBox.getText().isEmpty() ? String ("My Preset") : presetBox.getText());
    w->addButton ("Save", 1, KeyPress (KeyPress::returnKey));
    w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));

    Component::SafePointer<DarkArpEditor> safe (this);
    w->enterModalState (true,
                        ModalCallbackFunction::create ([safe, w] (int result)
                        {
                            if (result != 1 || safe == nullptr) return;
                            const auto name = w->getTextEditorContents ("name").trim();
                            if (name.isNotEmpty() && safe->proc.savePreset (name))
                                safe->refreshPresets (File::createLegalFileName (name));
                        }),
                        true);
}

//==============================================================================
void DarkArpEditor::paint (Graphics& g)
{
    g.setGradientFill (ColourGradient (Colour (0xff0b0a13), 0.f, 0.f, Colour (0xff141021), 0.f, (float) getHeight(), false));
    g.fillAll();

    g.setColour (ui::accent);
    g.setFont (Font (FontOptions (24.f, Font::bold)));
    g.drawText ("DARK ARP", 16, 10, 200, 30, Justification::centredLeft);
    g.setColour (ui::dim);
    g.setFont (11.f);
    g.drawText ("one-shot arp sampler", 132, 18, 160, 16, Justification::centredLeft);

    auto panel = [&] (Rectangle<int> r, const String& title)
    {
        g.setColour (ui::panel);
        g.fillRoundedRectangle (r.toFloat(), 10.f);
        g.setColour (ui::dim);
        g.setFont (11.f);
        g.drawText (title, r.getX() + 14, r.getY() + 6, 900, 14, Justification::centredLeft);
    };

    panel (arpPanel, "ARP");
    panel (stepPanel, "STEP SEQUENCER   -   pitch (semitones)  /  velocity  /  probability   -   double-click resets");
    panel (soundPanel, "SOUND");
    panel (fxPanel, "FX");

    g.setColour (ui::dim);
    g.setFont (10.5f);
    const int y0 = stepPanel.getY() + 24;
    g.drawText ("ON",    stepPanel.getX() + 8, y0,       44, 22, Justification::centredLeft);
    g.drawText ("PITCH", stepPanel.getX() + 8, y0 + 26,  44, 72, Justification::centredLeft);
    g.drawText ("VEL",   stepPanel.getX() + 8, y0 + 102, 44, 46, Justification::centredLeft);
    g.drawText ("PROB",  stepPanel.getX() + 8, y0 + 152, 44, 34, Justification::centredLeft);
}

void DarkArpEditor::resized()
{
    auto r = getLocalBounds().reduced (16, 10);

    // header
    auto head = r.removeFromTop (34);
    head.removeFromLeft (300);
    presetBox.setBounds (head.removeFromLeft (230).reduced (0, 3));
    head.removeFromLeft (6);
    saveBtn.setBounds (head.removeFromLeft (70).reduced (0, 3));
    loadBtn.setBounds (head.removeFromRight (120).reduced (0, 3));
    r.removeFromTop (8);

    wave.setBounds (r.removeFromTop (140));
    r.removeFromTop (8);

    auto placeRow = [] (Rectangle<int> area, const std::vector<std::pair<Component*, int>>& row, int gap)
    {
        int x = area.getX() + 6;
        for (auto& [c, w] : row)
        {
            if (dynamic_cast<Pad*> (c) != nullptr || dynamic_cast<TextButton*> (c) != nullptr)
                c->setBounds (x, area.getY() + 20, w, 32);
            else if (dynamic_cast<Combo*> (c) != nullptr)
                c->setBounds (x, area.getY() + 8, w, 52);
            else
                c->setBounds (x, area.getY(), w, area.getHeight());
            x += w + gap;
        }
    };

    arpPanel = r.removeFromTop (112);
    placeRow (arpPanel.reduced (10, 0).withTrimmedTop (26).withTrimmedBottom (6), arpRow, 10);
    r.removeFromTop (8);

    stepPanel = r.removeFromTop (232);
    {
        auto area = stepPanel.reduced (10, 0).withTrimmedTop (24).withTrimmedBottom (8);
        area.removeFromLeft (46);
        const int cw = area.getWidth() / 16;
        for (int i = 0; i < 16; ++i)
            steps[(size_t) i]->setBounds (area.getX() + i * cw, area.getY(), cw, area.getHeight());
    }
    r.removeFromTop (8);

    auto bottom = r.removeFromTop (112);
    soundPanel = bottom.removeFromLeft (bottom.getWidth() * 48 / 100);
    bottom.removeFromLeft (8);
    fxPanel = bottom;
    placeRow (soundPanel.reduced (10, 0).withTrimmedTop (26).withTrimmedBottom (6), soundRow, 4);
    placeRow (fxPanel.reduced (10, 0).withTrimmedTop (26).withTrimmedBottom (6), fxRow, 4);
}
