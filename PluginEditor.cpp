#include "PluginEditor.h"

using namespace juce;

namespace
{
    constexpr int kW = 1280, kH = 712;

    String stepId (int i, const char* what) { return "s" + String (i) + "_" + what; }

    bool isAudioFile (const String& path)
    {
        return File (path).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg");
    }
}

//==============================================================================
RiverrLnF::RiverrLnF()
{
    const Colour popup (0xff0a0a0a);
    setColour (ResizableWindow::backgroundColourId, Colours::black);
    setColour (Label::textColourId, ui::white (0.6f));
    setColour (ComboBox::backgroundColourId, Colours::transparentBlack);
    setColour (ComboBox::textColourId, ui::white (0.9f));
    setColour (ComboBox::outlineColourId, Colours::transparentBlack);
    setColour (ComboBox::arrowColourId, ui::white (0.6f));
    setColour (PopupMenu::backgroundColourId, popup);
    setColour (PopupMenu::textColourId, ui::white (0.85f));
    setColour (PopupMenu::highlightedBackgroundColourId, ui::white (0.12f));
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (Slider::textBoxTextColourId, ui::white (0.5f));
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (TextButton::buttonColourId, Colours::transparentBlack);
    setColour (TextButton::textColourOffId, ui::white (0.85f));
    setColour (TextButton::textColourOnId, Colours::white);
    setColour (AlertWindow::backgroundColourId, popup);
    setColour (AlertWindow::textColourId, ui::white (0.85f));
    setColour (TextEditor::backgroundColourId, Colours::black);
    setColour (TextEditor::textColourId, Colours::white);
    setColour (TextEditor::outlineColourId, ui::white (0.3f));
    setColour (TextEditor::focusedOutlineColourId, ui::accent);
    setColour (TextEditor::highlightColourId, ui::accent.withAlpha (0.35f));
    setColour (TooltipWindow::backgroundColourId, popup);
    setColour (TooltipWindow::textColourId, Colours::white);
    setColour (TooltipWindow::outlineColourId, ui::white (0.3f));
}

void RiverrLnF::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                  float startAngle, float endAngle, Slider& s)
{
    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (5.f);
    const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;

    Path track;
    track.addCentredArc (c.x, c.y, r, r, 0.f, startAngle, endAngle, true);
    g.setColour (ui::white (0.16f));
    g.strokePath (track, PathStrokeType (1.2f));

    const float zeroAngle = bipolar ? startAngle + (float) s.valueToProportionOfLength (0.0) * (endAngle - startAngle)
                                    : startAngle;
    Path val;
    val.addCentredArc (c.x, c.y, r, r, 0.f, jmin (zeroAngle, angle), jmax (zeroAngle, angle), true);
    g.setColour (ui::white (0.92f));
    g.strokePath (val, PathStrokeType (1.6f));

    g.setColour (ui::accent);
    g.fillEllipse (c.x + std::sin (angle) * r - 2.2f, c.y - std::cos (angle) * r - 2.2f, 4.4f, 4.4f);
}

Slider::SliderLayout RiverrLnF::getSliderLayout (Slider& s)
{
    if (s.getSliderStyle() == Slider::LinearVertical)
    {
        Slider::SliderLayout l;
        l.sliderBounds = s.getLocalBounds();
        return l;
    }
    return LookAndFeel_V4::getSliderLayout (s);
}

void RiverrLnF::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float,
                                  float minPos, float maxPos, Slider::SliderStyle style, Slider& s)
{
    if (style == Slider::LinearBar)
    {
        auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        g.setColour (ui::white (0.28f));
        g.drawLine (b.getX(), b.getBottom() - 0.5f, b.getRight(), b.getBottom() - 0.5f, 1.f);
        g.setColour (ui::white (0.9f));
        g.setFont (ui::font (10.5f, 0.05f));
        g.drawText (s.getTextFromValue (s.getValue()), b, Justification::centred);
        return;
    }

    if (style != Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, 0.f, minPos, maxPos, style, s);
        return;
    }

    const float bw = jmin ((float) w - 8.f, 18.f);
    auto b = Rectangle<float> ((float) x + ((float) w - bw) * 0.5f, (float) y, bw, (float) h);
    g.setColour (ui::white (0.05f));
    g.fillRect (b);

    const float prop = (float) s.valueToProportionOfLength (s.getValue());
    const bool bipolar = s.getMinimum() < 0.0;
    const float yv = b.getBottom() - prop * b.getHeight();
    const float y0 = bipolar ? b.getBottom() - (float) s.valueToProportionOfLength (0.0) * b.getHeight()
                             : b.getBottom();

    const bool blue = (bool) s.getProperties().getWithDefault ("accent", false);
    const Colour c = blue ? ui::accent : ui::white (0.9f);

    g.setColour (c.withAlpha (0.45f));
    g.fillRect (b.getX(), jmin (yv, y0), b.getWidth(), jmax (1.f, std::abs (yv - y0)));
    g.setColour (c);
    g.fillRect (b.getX(), yv - 1.f, b.getWidth(), 2.f);

    if (bipolar)
    {
        g.setColour (ui::white (0.3f));
        g.drawHorizontalLine ((int) y0, b.getX() - 3.f, b.getRight() + 3.f);
    }
}

void RiverrLnF::drawToggleButton (Graphics& g, ToggleButton& btn, bool highlighted, bool)
{
    auto b = btn.getLocalBounds().toFloat().reduced (1.5f);
    const bool on = btn.getToggleState();

    if (btn.getButtonText().isEmpty())
    {
        g.setColour (ui::white (on ? 0.9f : (highlighted ? 0.45f : 0.22f)));
        if (on) g.fillRoundedRectangle (b, 2.f);
        else    g.drawRoundedRectangle (b, 2.f, 1.f);
        return;
    }

    g.setColour (ui::white (on ? 0.9f : (highlighted ? 0.55f : 0.28f)));
    g.drawRoundedRectangle (b, 3.f, 1.f);
    g.setColour (ui::white (on ? 1.f : 0.55f));
    g.setFont (ui::font (10.5f));
    g.drawText (btn.getButtonText(), b, Justification::centred);
    if (on)
    {
        g.setColour (ui::accent);
        g.fillRect (b.getCentreX() - 8.f, b.getBottom() - 3.f, 16.f, 1.5f);
    }
}

void RiverrLnF::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    if (down)
    {
        g.setColour (ui::white (0.08f));
        g.fillRoundedRectangle (r, 3.f);
    }
    g.setColour (ui::white (down ? 0.8f : (highlighted ? 0.55f : 0.28f)));
    g.drawRoundedRectangle (r, 3.f, 1.f);
}

void RiverrLnF::drawComboBox (Graphics& g, int width, int height, bool, int, int, int, int, ComboBox& box)
{
    g.setColour (ui::white (box.isPopupActive() ? 0.65f : 0.28f));
    g.drawHorizontalLine (height - 1, 0.f, (float) width);

    Path p;
    const float cx = (float) width - 9.f, cy = (float) height * 0.5f + 1.f;
    p.addTriangle (cx - 3.5f, cy - 2.f, cx + 3.5f, cy - 2.f, cx, cy + 2.f);
    g.setColour (ui::white (0.6f));
    g.fillPath (p);
}

Font RiverrLnF::getTextButtonFont (TextButton&, int) { return ui::font (10.5f); }
Font RiverrLnF::getComboBoxFont (ComboBox&)          { return ui::font (11.f, 0.05f); }
Font RiverrLnF::getPopupMenuFont()                   { return ui::font (11.f, 0.05f); }

//==============================================================================
Knob::Knob (AudioProcessorValueTreeState& a, const String& id, const String& text, const String& suffix)
{
    label.setText (text, dontSendNotification);
    label.setJustificationType (Justification::centred);
    label.setFont (ui::font (9.5f, 0.15f));
    label.setColour (Label::textColourId, ui::white (0.6f));
    addAndMakeVisible (label);

    slider.setColour (Slider::textBoxTextColourId, ui::white (0.5f));
    slider.setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    slider.setColour (Slider::textBoxHighlightColourId, ui::accent.withAlpha (0.4f));
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, 50, 15);
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
            if (isInt)                                   t = String (roundToInt (v));
            else if (isPercent)                          t = String (roundToInt (v * 100.0)) + "%";
            else if (suffix == "s" && v < 1.0)           return String (roundToInt (v * 1000.0)) + " ms";
            else if (std::abs (v) >= 1000.0)             t = String (roundToInt (v));
            else if (std::abs (v) >= 100.0)              t = String (v, 0);
            else if (std::abs (v) >= 10.0)               t = String (v, 1);
            else                                         t = String (v, 2);
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
    label.setFont (ui::font (9.5f, 0.15f));
    label.setColour (Label::textColourId, ui::white (0.6f));
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
    box.setBounds (b.removeFromTop (26));
}

//==============================================================================
WaveformView::WaveformView (RiverrProcessor& p) : proc (p)
{
    startP = p.apvts.getParameter ("start");
    endP = p.apvts.getParameter ("end");
    startTimerHz (20);
}

float WaveformView::normToX (float n) const
{
    return 8.f + n * ((float) getWidth() - 16.f);
}

void WaveformView::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    if (dragOver)
    {
        g.setColour (ui::accent.withAlpha (0.07f));
        g.fillRoundedRectangle (b, 4.f);
    }
    g.setColour (dragOver ? ui::accent : ui::white (0.14f));
    g.drawRoundedRectangle (b.reduced (0.5f), 4.f, 1.f);

    auto s = proc.getSample();
    if (s == nullptr)
    {
        g.setColour (ui::white (0.4f));
        g.setFont (ui::font (12.f, 0.25f));
        g.drawText ("DROP A ONE-SHOT HERE     /     LOAD SOUND", b, Justification::centred);
        return;
    }

    const float mid = b.getCentreY();
    const float half = b.getHeight() * 0.40f;
    const float x0 = normToX (0.f), x1 = normToX (1.f);
    const int cols = (int) s->peakMax.size();
    const int W = (int) (x1 - x0);

    g.setColour (ui::white (0.08f));
    g.drawHorizontalLine ((int) mid, x0, x1);

    g.setColour (ui::white (0.85f));
    for (int px = 0; px < W; px += 2)
    {
        const int c = jlimit (0, cols - 1, (int) ((float) px / (float) W * (float) cols));
        const float y1 = mid - s->peakMax[(size_t) c] * half;
        const float y2 = mid - s->peakMin[(size_t) c] * half;
        g.fillRect (x0 + (float) px, y1, 1.f, jmax (1.f, y2 - y1));
    }

    const float sx = normToX (startP->getValue()), ex = normToX (endP->getValue());
    g.setColour (Colours::black.withAlpha (0.72f));
    g.fillRect (x0, b.getY() + 1.f, jmax (0.f, sx - x0), b.getHeight() - 2.f);
    g.fillRect (ex, b.getY() + 1.f, jmax (0.f, x1 - ex), b.getHeight() - 2.f);

    g.setColour (ui::accent);
    g.drawLine (sx, b.getY() + 1.f, sx, b.getBottom() - 1.f, 1.2f);
    g.drawLine (ex, b.getY() + 1.f, ex, b.getBottom() - 1.f, 1.2f);
    g.setFont (ui::font (9.f, 0.2f));
    g.drawText ("START", Rectangle<float> (sx + 4.f, b.getY() + 4.f, 44.f, 12.f), Justification::centredLeft);
    g.drawText ("END", Rectangle<float> (ex - 48.f, b.getY() + 4.f, 44.f, 12.f), Justification::centredRight);

    g.setColour (ui::white (0.5f));
    g.setFont (ui::font (10.f, 0.05f));
    g.drawText (s->info, b.reduced (12.f, 6.f), Justification::bottomLeft);
    g.setColour (ui::white (0.35f));
    g.drawText (s->name, b.reduced (12.f, 6.f), Justification::bottomRight);
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
    const float n = jlimit (0.f, 1.f, (e.position.x - 8.f) / ((float) getWidth() - 16.f));

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
      aGate (a, stepId (index, "gate"), gate),
      aProb (a, stepId (index, "prob"), prob),
      aRat (a, stepId (index, "rat"), rat)
{
    addAndMakeVisible (on);
    for (auto* s : { &pit, &vel, &gate, &prob, &rat })
    {
        addAndMakeVisible (s);
        s->setPopupDisplayEnabled (true, true, nullptr);
    }
    pit.getProperties().set ("accent", true);
    pit.setDoubleClickReturnValue (true, 0.0);
    vel.setDoubleClickReturnValue (true, 0.85);
    gate.setDoubleClickReturnValue (true, 1.0);
    prob.setDoubleClickReturnValue (true, 1.0);
    rat.setDoubleClickReturnValue (true, 1.0);
}

void StepColumn::resized()
{
    const auto b = getLocalBounds().reduced (8, 0);
    on.setBounds (b.getCentreX() - 12, ui::rowTop (0), 24, ui::kRowH[0]);

    Slider* rows[] = { nullptr, &pit, &vel, &gate, &prob, &rat };
    for (int l = 1; l < ui::kLanes; ++l)
        rows[l]->setBounds (b.getX(), ui::rowTop (l), b.getWidth(), ui::kRowH[l]);
}

void StepColumn::setState (int play, int dim)
{
    if (play == playMask && dim == dimMask) return;
    playMask = play;
    dimMask = dim;

    Component* rows[] = { &on, &pit, &vel, &gate, &prob, &rat };
    for (int l = 0; l < ui::kLanes; ++l)
        rows[l]->setAlpha (((dimMask >> l) & 1) ? 0.25f : 1.f);
    repaint();
}

void StepColumn::paint (Graphics& g)
{
    const auto b = getLocalBounds();

    if (idx % 4 == 0 && idx > 0)
    {
        g.setColour (ui::white (0.07f));
        g.drawVerticalLine (0, 0.f, (float) ui::kRowsEnd);
    }

    for (int l = 0; l < ui::kLanes; ++l)
        if ((playMask >> l) & 1)
        {
            g.setColour (ui::accent);
            g.fillRect (b.getX() + 14, ui::rowTop (l) - 3, b.getWidth() - 28, 2);
        }

    g.setColour (ui::white ((playMask & 1) ? 0.95f : 0.3f));
    g.setFont (ui::font (9.f, 0.05f));
    g.drawText (String (idx + 1), b.getX(), ui::kRowsEnd + 2, b.getWidth(), 12, Justification::centred);
}

//==============================================================================
RiverrEditor::RiverrEditor (RiverrProcessor& p)
    : AudioProcessorEditor (&p), proc (p), wave (p)
{
    setLookAndFeel (&laf);
    auto& A = p.apvts;

    addAndMakeVisible (wave);

    addAndMakeVisible (presetBox);
    presetBox.setTextWhenNothingSelected ("PRESETS");
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
    arpRow.push_back ({ make<Pad> (A, "arpOn", "ARP"), 56 });
    arpRow.push_back ({ make<Combo> (A, "dir", "DIRECTION"), 96 });
    arpRow.push_back ({ make<Combo> (A, "octRange", "OCTAVES"), 62 });
    arpRow.push_back ({ make<Combo> (A, "rate", "RATE"), 70 });
    arpRow.push_back ({ make<Combo> (A, "key", "KEY"), 54 });
    arpRow.push_back ({ make<Combo> (A, "scale", "SCALE"), 116 });
    arpRow.push_back ({ make<Knob> (A, "gate", "GATE"), 54 });
    arpRow.push_back ({ make<Knob> (A, "swing", "SWING"), 54 });
    arpRow.push_back ({ make<Knob> (A, "humT", "HUM T"), 54 });
    arpRow.push_back ({ make<Knob> (A, "humV", "HUM V"), 54 });
    arpRow.push_back ({ make<Knob> (A, "evolve", "EVOLVE"), 54 });
    arpRow.push_back ({ make<Knob> (A, "jump", "JUMP"), 54 });
    arpRow.push_back ({ make<Combo> (A, "layer", "LAYER"), 66 });
    arpRow.push_back ({ make<Knob> (A, "layerLvl", "LVL"), 54 });
    arpRow.push_back ({ &randBtn, 100 });

    // sequencer
    for (int i = 0; i < 16; ++i)
    {
        steps.push_back (std::make_unique<StepColumn> (A, i));
        addAndMakeVisible (*steps.back());
    }
    for (const char* id : { "steps", "lenPit", "lenVel", "lenGate", "lenProb", "lenRat" })
    {
        lenBoxes.push_back (std::make_unique<LenBox> (A, id));
        addAndMakeVisible (*lenBoxes.back());
    }

    // sound row
    soundRow.push_back ({ make<Knob> (A, "octave", "OCTAVE"), 50 });
    soundRow.push_back ({ make<Knob> (A, "tune", "TUNE", "st"), 50 });
    soundRow.push_back ({ make<Pad> (A, "autotune", "AUTO C5"), 60 });
    soundRow.push_back ({ make<Pad> (A, "reverse", "REVERSE"), 60 });
    soundRow.push_back ({ make<Knob> (A, "scan", "SCAN"), 50 });
    soundRow.push_back ({ make<Knob> (A, "drift", "DRIFT"), 50 });
    soundRow.push_back ({ make<Knob> (A, "spread", "SPREAD"), 50 });
    soundRow.push_back ({ make<Knob> (A, "atk", "ATTACK", "s"), 50 });
    soundRow.push_back ({ make<Knob> (A, "dec", "DECAY", "s"), 50 });
    soundRow.push_back ({ make<Knob> (A, "sus", "SUSTAIN"), 50 });
    soundRow.push_back ({ make<Knob> (A, "rel", "RELEASE", "s"), 50 });
    soundRow.push_back ({ make<Knob> (A, "gain", "GAIN", "dB"), 50 });

    // fx row
    fxRow.push_back ({ make<Knob> (A, "cut", "CUTOFF", "Hz"), 54 });
    fxRow.push_back ({ make<Knob> (A, "res", "RESO"), 50 });
    fxRow.push_back ({ make<Knob> (A, "lfoRate", "LFO HZ", "Hz"), 54 });
    fxRow.push_back ({ make<Knob> (A, "lfoDepth", "LFO AMT"), 54 });
    fxRow.push_back ({ make<Combo> (A, "dlyTime", "DELAY"), 64 });
    fxRow.push_back ({ make<Knob> (A, "dlyFb", "FDBK"), 50 });
    fxRow.push_back ({ make<Knob> (A, "dlyMix", "DLY"), 50 });
    fxRow.push_back ({ make<Knob> (A, "revSize", "SIZE"), 50 });
    fxRow.push_back ({ make<Knob> (A, "revDamp", "DAMP"), 50 });
    fxRow.push_back ({ make<Knob> (A, "revMix", "REVERB"), 50 });

    proc.addChangeListener (this);
    startTimerHz (30);
    setSize (kW, kH);
}

RiverrEditor::~RiverrEditor()
{
    stopTimer();
    proc.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void RiverrEditor::changeListenerCallback (ChangeBroadcaster*)
{
    wave.repaint();
}

void RiverrEditor::timerCallback()
{
    int ls[ui::kLanes], len[ui::kLanes];
    for (int l = 0; l < ui::kLanes; ++l)
    {
        ls[l] = proc.laneStep[(size_t) l].load();
        len[l] = proc.getLaneLength (l);
    }

    for (int i = 0; i < (int) steps.size(); ++i)
    {
        int play = 0, dim = 0;
        for (int l = 0; l < ui::kLanes; ++l)
        {
            if (ls[l] == i) play |= 1 << l;
            if (i >= len[l]) dim |= 1 << l;
        }
        steps[(size_t) i]->setState (play, dim);
    }
}

//==============================================================================
bool RiverrEditor::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files)
        if (isAudioFile (f)) return true;
    return false;
}

void RiverrEditor::filesDropped (const StringArray& files, int, int)
{
    wave.setDragOver (false);
    for (auto& f : files)
        if (isAudioFile (f) && proc.loadSample (File (f)))
            break;
}

void RiverrEditor::openFileChooser()
{
    chooser = std::make_unique<FileChooser> ("Choose a one-shot", File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    Component::SafePointer<RiverrEditor> safe (this);
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [safe] (const FileChooser& fc)
                          {
                              if (safe == nullptr) return;
                              const auto f = fc.getResult();
                              if (f.existsAsFile()) safe->proc.loadSample (f);
                          });
}

void RiverrEditor::refreshPresets (const String& select)
{
    presetBox.clear (dontSendNotification);
    presetBox.addItemList (proc.listPresets(), 1);
    if (select.isNotEmpty())
        presetBox.setText (select, dontSendNotification);
}

void RiverrEditor::savePresetDialog()
{
    auto* w = new AlertWindow ("Save preset", "Preset name:", MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", presetBox.getText().isEmpty() ? String ("My Preset") : presetBox.getText());
    w->addButton ("Save", 1, KeyPress (KeyPress::returnKey));
    w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));

    Component::SafePointer<RiverrEditor> safe (this);
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
void RiverrEditor::paint (Graphics& g)
{
    g.fillAll (Colours::black);

    g.setColour (Colours::white);
    g.setFont (Font (FontOptions (22.f)).withExtraKerningFactor (0.5f));
    g.drawText ("RIVERR", 16, 10, 220, 28, Justification::centredLeft);
    g.setColour (ui::white (0.35f));
    g.setFont (ui::font (9.f, 0.25f));
    g.drawText ("ARP SAMPLER", 17, 38, 200, 12, Justification::centredLeft);

    auto section = [&] (int x, int y, int w, const String& t)
    {
        g.setColour (ui::white (0.5f));
        g.setFont (ui::font (9.5f, 0.3f));
        g.drawText (t, x, y, 120, 12, Justification::centredLeft);
        g.setColour (ui::white (0.10f));
        g.drawHorizontalLine (y + 6, (float) (x + 82), (float) (x + w));
    };

    section (16, yArp, contentW, "ARP");
    section (16, ySeq, contentW, "SEQUENCE");
    section (16, ySound, fxX - 16 - 12, "SOUND");
    section (fxX, ySound, contentW - (fxX - 16), "FX");

    static const char* names[] = { "ON", "PITCH", "VEL", "GATE", "PROB", "RATCH" };
    g.setColour (ui::white (0.5f));
    g.setFont (ui::font (9.5f, 0.15f));
    for (int l = 0; l < ui::kLanes; ++l)
        g.drawText (names[l], 16, lanesTop + ui::rowTop (l), 46, ui::kRowH[l], Justification::centredLeft);
}

void RiverrEditor::resized()
{
    auto r = getLocalBounds().reduced (16, 12);
    contentW = r.getWidth();

    auto head = r.removeFromTop (36);
    head.removeFromLeft (250);
    presetBox.setBounds (head.removeFromLeft (220).reduced (0, 6));
    head.removeFromLeft (10);
    saveBtn.setBounds (head.removeFromLeft (64).reduced (0, 5));
    loadBtn.setBounds (head.removeFromRight (110).reduced (0, 5));
    r.removeFromTop (8);

    wave.setBounds (r.removeFromTop (134));
    r.removeFromTop (12);

    auto placeRow = [] (Rectangle<int> area, const std::vector<std::pair<Component*, int>>& row, int gap)
    {
        int x = area.getX();
        for (auto& [c, w] : row)
        {
            if (dynamic_cast<ToggleButton*> (c) != nullptr || dynamic_cast<TextButton*> (c) != nullptr)
                c->setBounds (x, area.getY() + 20, w, 27);
            else if (dynamic_cast<Combo*> (c) != nullptr)
                c->setBounds (x, area.getY(), w, 46);
            else
                c->setBounds (x, area.getY(), w, area.getHeight());
            x += w + gap;
        }
    };

    yArp = r.getY();
    r.removeFromTop (20);
    placeRow (r.removeFromTop (84), arpRow, 8);
    r.removeFromTop (12);

    ySeq = r.getY();
    r.removeFromTop (20);
    auto seq = r.removeFromTop (ui::kColH);
    lanesTop = seq.getY();
    seq.removeFromLeft (92);
    const int cw = seq.getWidth() / 16;
    for (int i = 0; i < 16; ++i)
        steps[(size_t) i]->setBounds (seq.getX() + i * cw, lanesTop, cw, ui::kColH);
    for (int l = 0; l < ui::kLanes; ++l)
        lenBoxes[(size_t) l]->setBounds (16 + 48, lanesTop + ui::rowTop (l) + (ui::kRowH[l] - 16) / 2, 36, 16);
    r.removeFromTop (12);

    ySound = r.getY();
    r.removeFromTop (20);
    auto bottom = r.removeFromTop (84);
    auto soundArea = bottom.removeFromLeft (662);
    bottom.removeFromLeft (14);
    fxX = bottom.getX();
    placeRow (soundArea, soundRow, 2);
    placeRow (bottom, fxRow, 2);
}
