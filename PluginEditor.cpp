#include "PluginEditor.h"

using namespace juce;

namespace
{
    const Rectangle<int> kScreen (304, 76, 640, 540);       // CRT screen (PLAY + FX)
    const Rectangle<int> kEditRect (46, 66, 1188, 662);     // EDIT page screen
    constexpr int kKeyW = 116, kKeyH = 88, kKeyGap = 8, kKeysX = 40, kKeysY = 92;

    String stepId (int i, const char* what) { return "s" + String (i) + "_" + what; }

    bool isAudioFile (const String& path)
    {
        return File (path).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg");
    }

    const char* kPlayKeyNames[8] = { "HARD BELL", "DARK PLUCK", "HALFTIME GHOST", "SLIDE MONO",
                                     "CRUSHED ROLL", "DICE HARD", "RANDOMIZE", "RESET" };
    const int kKeyToType[8] = { 1, 2, 0, 7, 3, 4, 5, 6 };   // FX key (column-major) -> effect type

    Image makeScreenImage (Rectangle<int> r, int margin)
    {
        const int W = r.getWidth() + 2 * margin, H = r.getHeight() + 2 * margin;
        Image img (Image::ARGB, W, H, true);
        Graphics g (img);

        const auto outer = Rectangle<float> (0.f, 0.f, (float) W, (float) H);
        const auto glass = Rectangle<float> ((float) margin, (float) margin, (float) r.getWidth(), (float) r.getHeight());

        g.setColour (Colour (0xff09090b));
        g.fillRoundedRectangle (outer, 24.f);
        g.setColour (ui::white (0.10f));
        g.drawRoundedRectangle (outer.reduced (1.f), 23.f, 1.5f);
        g.setColour (Colours::black);
        g.drawRoundedRectangle (glass.expanded (3.f), 19.f, 3.f);

        g.setGradientFill (ColourGradient (Colour (0xff0e1727), glass.getCentreX(), glass.getCentreY() * 0.8f,
                                           Colour (0xff020307), glass.getX(), glass.getY(), true));
        g.fillRoundedRectangle (glass, 16.f);

        {
            Graphics::ScopedSaveState ss (g);
            Path clip;
            clip.addRoundedRectangle (glass, 16.f);
            g.reduceClipRegion (clip);

            g.setColour (Colours::black.withAlpha (0.22f));
            for (float y = glass.getY(); y < glass.getBottom(); y += 3.f)
                g.fillRect (glass.getX(), y, glass.getWidth(), 1.f);

            g.setGradientFill (ColourGradient (ui::white (0.07f), glass.getX(), glass.getY(),
                                               Colours::transparentWhite, glass.getX() + glass.getWidth() * 0.7f,
                                               glass.getY() + glass.getHeight() * 0.5f, false));
            g.fillRect (glass);

            g.setColour (Colours::black.withAlpha (0.55f));
            g.drawRoundedRectangle (glass.reduced (4.f), 14.f, 9.f);
        }
        return img;
    }
}

//==============================================================================
void applyNiceText (Slider& slider, AudioProcessorValueTreeState& a, const String& id, const String& suffix)
{
    auto* p = a.getParameter (id);
    if (p == nullptr) return;

    const bool isInt = dynamic_cast<AudioParameterInt*> (p) != nullptr;
    bool isPercent = false;
    if (auto* fp = dynamic_cast<RangedAudioParameter*> (p))
        isPercent = fp->getNormalisableRange().end <= 1.f && fp->getNormalisableRange().start >= 0.f;

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
    const int dev = (int) s.getProperties().getWithDefault ("dev", 0);
    const float angle = startAngle + pos * (endAngle - startAngle);

    if (dev > 0)
    {
        auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.f);
        const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();

        Path arc;
        arc.addCentredArc (c.x, c.y, r, r, 0.f, startAngle, angle, true);
        g.setColour (ui::white (0.10f));
        Path track;
        track.addCentredArc (c.x, c.y, r, r, 0.f, startAngle, endAngle, true);
        g.strokePath (track, PathStrokeType (1.2f));
        g.setColour (ui::accent.withAlpha (0.85f));
        g.strokePath (arc, PathStrokeType (1.8f));

        const float rb = r * 0.90f;
        g.setGradientFill (ColourGradient (Colour (0xff45454c), c.x - rb * 0.5f, c.y - rb * 0.7f,
                                           Colour (0xff08080a), c.x + rb * 0.6f, c.y + rb * 0.9f, true));
        g.fillEllipse (c.x - rb, c.y - rb, rb * 2.f, rb * 2.f);

        const float ri = rb * 0.80f;
        g.setGradientFill (ColourGradient (Colour (0xff202026), c.x - ri * 0.3f, c.y - ri * 0.5f,
                                           Colour (0xff050506), c.x + ri * 0.6f, c.y + ri * 0.8f, true));
        g.fillEllipse (c.x - ri, c.y - ri, ri * 2.f, ri * 2.f);
        g.setColour (ui::white (0.07f));
        for (int k = 1; k <= 3; ++k)
            g.drawEllipse (c.x - ri * k / 4.f, c.y - ri * k / 4.f, ri * k / 2.f, ri * k / 2.f, 1.f);

        const float dr = ri * 0.72f, dd = dev == 1 ? 4.6f : 3.4f;
        const float dx = c.x + std::sin (angle) * dr, dy = c.y - std::cos (angle) * dr;
        g.setColour (ui::accent.withAlpha (0.25f));
        g.fillEllipse (dx - dd * 1.9f, dy - dd * 1.9f, dd * 3.8f, dd * 3.8f);
        g.setColour (Colours::white);
        g.fillEllipse (dx - dd, dy - dd, dd * 2.f, dd * 2.f);
        return;
    }

    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (5.f);
    const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
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

        if ((bool) s.getProperties().getWithDefault ("cell", false))
        {
            b = b.reduced (1.f);
            g.setColour (Colour (0xff0b111a));
            g.fillRoundedRectangle (b, 3.f);
            const float prop = (float) s.valueToProportionOfLength (s.getValue());
            g.setColour (ui::accent.withAlpha (0.20f));
            g.fillRoundedRectangle (b.withWidth (jmax (3.f, b.getWidth() * prop)), 3.f);
            g.setColour (ui::white (0.16f));
            g.drawRoundedRectangle (b, 3.f, 1.f);
            g.setFont (ui::mono (10.5f));
            g.setColour (ui::white (0.5f));
            g.drawText (s.getProperties()["label"].toString() + ":", b.reduced (7.f, 0.f), Justification::centredLeft);
            g.setColour (ui::accent);
            g.drawText (s.getTextFromValue (s.getValue()), b.reduced (7.f, 0.f), Justification::centredRight);
            return;
        }

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
    const bool on = btn.getToggleState();

    if ((bool) btn.getProperties().getWithDefault ("power", false))
    {
        auto b = btn.getLocalBounds().toFloat().reduced (1.f);
        g.setColour (Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (b, 5.f);
        g.setColour (ui::white (highlighted ? 0.4f : 0.2f));
        g.drawRoundedRectangle (b, 5.f, 1.f);
        const float cy = b.getCentreY(), cx = b.getX() + 14.f;
        if (on)
        {
            g.setColour (ui::accent.withAlpha (0.28f));
            g.fillEllipse (cx - 9.f, cy - 9.f, 18.f, 18.f);
        }
        g.setColour (on ? ui::accent : Colour (0xff2a2d33));
        g.fillEllipse (cx - 4.f, cy - 4.f, 8.f, 8.f);
        g.setColour (on ? ui::white (0.95f) : ui::white (0.4f));
        g.setFont (ui::mono (11.5f, true));
        g.drawText (on ? "ON" : "OFF", b.withTrimmedLeft (26.f), Justification::centred);
        return;
    }

    auto b = btn.getLocalBounds().toFloat().reduced (1.5f);

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

void RiverrLnF::drawButtonBackground (Graphics& g, Button& btn, const Colour&, bool highlighted, bool down)
{
    const int kind = (int) btn.getProperties().getWithDefault ("key", 0);

    if (kind > 0)
    {
        const bool big = kind == 1;
        const bool lit = btn.getToggleState();
        const bool sel = (bool) btn.getProperties().getWithDefault ("sel", false);
        auto b = btn.getLocalBounds().toFloat().reduced (big ? 3.f : 2.f);
        if (down) b = b.translated (0.f, 2.f);
        const float rad = big ? 9.f : 6.f;

        g.setColour (Colours::black.withAlpha (0.75f));
        g.fillRoundedRectangle (b.translated (0.f, big ? 4.f : 2.5f), rad);

        if (lit) g.setGradientFill (ColourGradient (Colour (0xffe6e2d9), 0.f, b.getY(), Colour (0xff9a968d), 0.f, b.getBottom(), false));
        else     g.setGradientFill (ColourGradient (Colour (0xff303036), 0.f, b.getY(), Colour (0xff141417), 0.f, b.getBottom(), false));
        g.fillRoundedRectangle (b, rad);

        g.setColour (Colours::white.withAlpha (lit ? 0.55f : 0.14f));
        g.drawLine (b.getX() + rad, b.getY() + 1.f, b.getRight() - rad, b.getY() + 1.f, 1.f);
        g.setColour (Colours::black.withAlpha (0.5f));
        g.drawRoundedRectangle (b, rad, 1.f);
        if (sel || highlighted)
        {
            g.setColour (sel ? ui::accent : ui::white (0.35f));
            g.drawRoundedRectangle (b.reduced (1.f), rad, sel ? 1.6f : 1.f);
        }

        g.setColour (lit ? Colour (0xff25231e) : ui::white (0.82f));
        g.setFont (big ? ui::font (11.5f, 0.16f) : ui::mono (11.f, true));
        g.drawFittedText (btn.getButtonText(), b.reduced (6.f, 4.f).toNearestInt(), Justification::centred, 2);
        return;
    }

    auto r = btn.getLocalBounds().toFloat().reduced (0.5f);
    const bool warn = (bool) btn.getProperties().getWithDefault ("warn", false);
    if (down)
    {
        g.setColour (ui::white (0.08f));
        g.fillRoundedRectangle (r, 3.f);
    }
    g.setColour (warn ? Colour (0xffff7a6a).withAlpha (down ? 0.95f : (highlighted ? 0.8f : 0.5f))
                      : ui::white (down ? 0.8f : (highlighted ? 0.55f : 0.28f)));
    g.drawRoundedRectangle (r, 3.f, 1.f);
}

void RiverrLnF::drawButtonText (Graphics& g, TextButton& b, bool highlighted, bool down)
{
    if ((int) b.getProperties().getWithDefault ("key", 0) > 0) return;
    LookAndFeel_V4::drawButtonText (g, b, highlighted, down);
}

void RiverrLnF::drawComboBox (Graphics& g, int width, int height, bool, int, int, int, int, ComboBox& box)
{
    if ((bool) box.getProperties().getWithDefault ("cell", false))
    {
        auto b = Rectangle<float> (0.f, 0.f, (float) width, (float) height).reduced (1.f);
        g.setColour (Colour (0xff0b111a));
        g.fillRoundedRectangle (b, 3.f);
        g.setColour (ui::white (box.isPopupActive() ? 0.5f : 0.16f));
        g.drawRoundedRectangle (b, 3.f, 1.f);
        g.setFont (ui::mono (10.5f));
        g.setColour (ui::white (0.5f));
        g.drawText (box.getProperties()["label"].toString() + ":", b.reduced (7.f, 0.f), Justification::centredLeft);
        Path p;
        const float cx = (float) width - 12.f, cy = (float) height * 0.5f + 1.f;
        p.addTriangle (cx - 3.5f, cy - 2.f, cx + 3.5f, cy - 2.f, cx, cy + 2.f);
        g.setColour (ui::accent);
        g.fillPath (p);
        return;
    }

    g.setColour (ui::white (box.isPopupActive() ? 0.65f : 0.28f));
    g.drawHorizontalLine (height - 1, 0.f, (float) width);

    Path p;
    const float cx = (float) width - 9.f, cy = (float) height * 0.5f + 1.f;
    p.addTriangle (cx - 3.5f, cy - 2.f, cx + 3.5f, cy - 2.f, cx, cy + 2.f);
    g.setColour (ui::white (0.6f));
    g.fillPath (p);
}

void RiverrLnF::positionComboBoxText (ComboBox& box, Label& label)
{
    if ((bool) box.getProperties().getWithDefault ("cell", false))
    {
        label.setBounds ((int) (box.getWidth() * 0.40f), 1, (int) (box.getWidth() * 0.60f) - 20, box.getHeight() - 2);
        label.setFont (ui::mono (10.5f));
        label.setJustificationType (Justification::centredRight);
        return;
    }
    label.setBounds (1, 1, box.getWidth() - 22, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

Font RiverrLnF::getTextButtonFont (TextButton&, int) { return ui::font (10.5f); }
Font RiverrLnF::getComboBoxFont (ComboBox&)          { return ui::font (11.f, 0.05f); }
Font RiverrLnF::getPopupMenuFont()                   { return ui::font (11.f, 0.05f); }

//==============================================================================
Knob::Knob (AudioProcessorValueTreeState& a, const String& id, const String& text, const String& suffix, int dev)
{
    label.setText (text, dontSendNotification);
    label.setJustificationType (Justification::centred);
    label.setFont (dev > 0 ? ui::mono (10.f, true) : ui::font (9.5f, 0.15f));
    label.setColour (Label::textColourId, ui::white (dev > 0 ? 0.75f : 0.6f));
    addAndMakeVisible (label);

    slider.getProperties().set ("dev", dev);
    slider.setColour (Slider::textBoxTextColourId, dev > 0 ? ui::accent : ui::white (0.5f));
    slider.setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    slider.setColour (Slider::textBoxHighlightColourId, ui::accent.withAlpha (0.4f));
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, dev == 1 ? 90 : 56, 15);
    addAndMakeVisible (slider);
    att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (a, id, slider);
    applyNiceText (slider, a, id, suffix);
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

CellCombo::CellCombo (AudioProcessorValueTreeState& a, const String& id, const String& label)
{
    getProperties().set ("cell", true);
    getProperties().set ("label", label);
    setColour (ComboBox::textColourId, ui::accent);
    if (auto* p = dynamic_cast<AudioParameterChoice*> (a.getParameter (id)))
        addItemList (p->choices, 1);
    att = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (a, id, *this);
}

//==============================================================================
WaveformView::WaveformView (RiverrProcessor& p, int s) : proc (p), slot (s)
{
    startP = p.apvts.getParameter (slot == 0 ? "start" : "bStart");
    endP = p.apvts.getParameter (slot == 0 ? "end" : "bEnd");
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
        g.setColour (ui::accent.withAlpha (0.10f));
        g.fillRoundedRectangle (b, 4.f);
    }
    g.setColour (dragOver ? ui::accent : ui::white (0.16f));
    g.drawRoundedRectangle (b.reduced (0.5f), 4.f, 1.f);

    g.setColour (ui::accent.withAlpha (0.8f));
    g.setFont (ui::mono (11.f, true));
    g.drawText (slot == 0 ? "A" : "B", b.reduced (7.f, 4.f), Justification::topLeft);

    auto s = proc.getSample (slot);
    if (s == nullptr)
    {
        g.setColour (ui::white (0.42f));
        g.setFont (ui::mono (11.5f));
        g.drawText (String ("DROP SOUND ") + (slot == 0 ? "A" : "B") + " HERE   /   LOAD " + (slot == 0 ? "A" : "B"),
                    b, Justification::centred);
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

    String tag = s->name;
    if (s->pitched)
        tag += "   " + MidiMessage::getMidiNoteName ((int) std::round (s->detectedMidi), true, true, 5) + " -> C  "
               + (s->rootShift >= 0.f ? "+" : "") + String (s->rootShift, 2) + "st";
    else
        tag += "   no pitch";
    g.setColour (ui::white (0.5f));
    g.setFont (ui::mono (9.5f));
    g.drawText (tag, b.reduced (10.f, 4.f), Justification::topRight);
}

void WaveformView::mouseDown (const MouseEvent& e)
{
    if (proc.getSample (slot) == nullptr) return;
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
    g.setFont (ui::mono (9.f));
    g.drawText (String (idx + 1), b.getX(), ui::kRowsEnd + 2, b.getWidth(), 12, Justification::centred);
}

//==============================================================================
void PlayScreen::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float cx = b.getCentreX(), cy = 212.f;
    const float R = 108.f * (1.f + 0.08f * pulse);
    const float tt = (float) t;
    const float pi = MathConstants<float>::pi;

    auto stroke = [&g] (const Path& p, float alpha, float w)
    {
        g.setColour (ui::accent.withAlpha (0.10f * alpha));
        g.strokePath (p, PathStrokeType (w * 4.f));
        g.setColour (ui::accent.withAlpha (0.85f * alpha));
        g.strokePath (p, PathStrokeType (w));
    };

    g.setFont (ui::mono (10.5f));
    g.setColour (ui::white (0.45f));
    g.drawText ("RIVERR // ARP SAMPLER", 24, 16, 260, 14, Justification::centredLeft);
    g.drawText (proc.apvts.getParameter ("key")->getCurrentValueAsText() + " "
                    + proc.apvts.getParameter ("scale")->getCurrentValueAsText().toUpperCase(),
                (int) b.getWidth() - 284, 16, 260, 14, Justification::centredRight);

    g.setFont (ui::mono (34.f, true));
    g.setColour (ui::white (0.95f));
    g.drawText (presetName, 0, 36, (int) b.getWidth(), 44, Justification::centred);
    g.setColour (ui::accent.withAlpha (0.3f));
    for (int i = 0; i < 14; ++i)
    {
        g.fillRect (24.f + (float) i * 9.f, 58.f, 3.f, 1.5f);
        g.fillRect (b.getWidth() - 24.f - (float) i * 9.f - 3.f, 58.f, 3.f, 1.5f);
    }

    // wireframe orb
    Path outline;
    outline.addEllipse (cx - R, cy - R, R * 2.f, R * 2.f);
    stroke (outline, 0.9f, 1.4f);

    for (int i = -3; i <= 3; ++i)
    {
        const float lat = (float) i * 0.36f;
        const float rr = R * std::cos (lat), yy = cy + R * std::sin (lat) * 0.92f;
        Path e;
        e.addEllipse (cx - rr, yy - rr * 0.22f, rr * 2.f, rr * 0.44f);
        stroke (e, 0.35f, 1.f);
    }
    for (int k = 0; k < 6; ++k)
    {
        const float ang = tt * 0.45f + (float) k * pi / 6.f;
        const float rw = R * std::abs (std::cos (ang));
        Path e;
        e.addEllipse (cx - rw, cy - R, rw * 2.f, R * 2.f);
        stroke (e, 0.12f + 0.4f * std::abs (std::sin (ang)), 1.f);
    }

    // 16-step ring
    for (int i = 0; i < 16; ++i)
    {
        const float a = -pi * 0.5f + (float) i * 2.f * pi / 16.f;
        const float rr = R + 44.f;
        const float dx = cx + std::cos (a) * rr, dy = cy + std::sin (a) * rr * 0.62f;
        const bool on = proc.apvts.getRawParameterValue (stepId (i, "on"))->load() > 0.5f;
        const bool cur = i == step;
        if (cur)
        {
            g.setColour (ui::accent.withAlpha (0.3f));
            g.fillEllipse (dx - 11.f, dy - 11.f, 22.f, 22.f);
        }
        g.setColour (cur ? Colours::white : (on ? ui::accent.withAlpha (0.75f) : ui::white (0.18f)));
        const float d = cur ? 5.f : 3.f;
        g.fillEllipse (dx - d, dy - d, d * 2.f, d * 2.f);
    }

    if (step < 0)
    {
        g.setFont (ui::mono (11.f, true));
        g.setColour (ui::white (0.35f + 0.25f * std::sin (tt * 2.2f)));
        g.drawText ("HOLD A CHORD ON YOUR KEYBOARD", 0, 340, (int) b.getWidth(), 16, Justification::centred);
    }

    g.setFont (ui::mono (9.5f));
    g.setColour (ui::white (0.4f));
    for (int slot = 0; slot < 2; ++slot)
        if (auto smp = proc.getSample (slot))
            g.drawText (String (slot == 0 ? "A  " : "B  ") + smp->info, 24, 372 + slot * 14, (int) b.getWidth() - 48, 13, Justification::centredLeft);
}

//==============================================================================
String FxPanel::titleOf (int type)
{
    static const char* names[] = { "FILTER", "DRIVE", "TAPE", "HALFTIME", "HALFSPEED", "DELAY", "REVERB", "GLUE" };
    return names[jlimit (0, 7, type)];
}

FxPanel::FxPanel (RiverrProcessor& p, int ty) : proc (p), type (ty)
{
    auto& A = p.apvts;
    const char* onId = nullptr;
    title = titleOf (ty);

    auto addBig   = [&] (const char* id, const char* label, const char* suf = "") { big = make<Knob> (A, id, label, suf, 1); };
    auto addSmall = [&] (const char* id, const char* label, const char* suf = "") { smalls.push_back (make<Knob> (A, id, label, suf, 2)); };
    auto cell     = [&] (const char* id, const char* label) { cells.push_back (make<CellSlider> (A, id, label)); };
    auto cellC    = [&] (const char* id, const char* label) { cells.push_back (make<CellCombo> (A, id, label)); };

    switch (ty)
    {
        case 0:
            blurb = "DARKENS THE SOUND. TURN CUTOFF DOWN FOR DARK ARPS."; onId = "fltOn";
            addBig ("cut", "CUTOFF", "Hz"); addSmall ("res", "RESO"); addSmall ("lfoRate", "LFO RATE", "Hz"); addSmall ("lfoDepth", "LFO AMT");
            cell ("cut", "cutoff"); cell ("res", "reso"); cell ("lfoRate", "lfo rate"); cell ("lfoDepth", "lfo depth");
            break;
        case 1:
            blurb = "ADDS GRIT AND WEIGHT. THIS IS THE HARD SOUND."; onId = "drvOn";
            addBig ("drvAmt", "DRIVE"); addSmall ("drvTone", "TONE"); addSmall ("drvMix", "MIX"); addSmall ("drvOut", "OUT", "dB");
            cellC ("drvType", "type"); cell ("drvAmt", "drive"); cell ("drvTone", "tone"); cell ("drvMix", "mix"); cell ("drvOut", "out");
            break;
        case 2:
            blurb = "OLD TAPE FEEL: HISS, WOBBLE, WARMTH."; onId = "tapeOn";
            addBig ("tapeNoise", "NOISE"); addSmall ("drift", "WOW"); addSmall ("tapeFlutter", "FLUTTER"); addSmall ("tapeSat", "SAT");
            cell ("tapeNoise", "noise"); cell ("drift", "wow"); cell ("tapeFlutter", "flutter"); cell ("tapeSat", "saturation"); cell ("tapeTone", "tone");
            break;
        case 3:
            blurb = "SLOWS THE WHOLE SOUND TO HALF SPEED. GHOSTLY, HEAVY."; onId = "htOn";
            addBig ("htMix", "MIX"); addSmall ("htTone", "TONE"); addSmall ("htFlux", "FLUX");
            cellC ("htLen", "length"); cell ("htMix", "mix"); cell ("htTone", "tone"); cell ("htFlux", "flux");
            break;
        case 4:
            blurb = "SOME HITS DIVE DOWN TO HALF SPEED - TAPE-STOP FEEL."; onId = "hsOn";
            addBig ("hsAmt", "AMOUNT"); addSmall ("hsDive", "DIVE"); addSmall ("hsSpeed", "SPEED");
            cell ("hsAmt", "amount"); cell ("hsDive", "dive"); cell ("hsSpeed", "speed");
            break;
        case 5:
            blurb = "PING-PONG ECHOES THAT GET DARKER AND DARKER."; onId = "dlyOn";
            addBig ("dlyMix", "MIX"); addSmall ("dlyFb", "FEEDBACK"); addSmall ("dlyTone", "TONE"); addSmall ("dlyDrift", "DRIFT");
            cellC ("dlyTime", "time"); cell ("dlyMix", "mix"); cell ("dlyFb", "feedback"); cell ("dlyTone", "tone"); cell ("dlyDrift", "drift");
            break;
        case 6:
            blurb = "BIG DARK SPACE. MORE MIX = MORE AMBIENT."; onId = "revOn";
            addBig ("revMix", "MIX"); addSmall ("revSize", "SIZE"); addSmall ("revDamp", "DAMP"); addSmall ("revPre", "PRE", "ms");
            cell ("revMix", "mix"); cell ("revSize", "size"); cell ("revDamp", "damp"); cell ("revPre", "pre-delay"); cell ("revWidth", "width");
            break;
        default:
            blurb = "MAKES CHORD NOTES REACT TO EACH OTHER: CUT, DUCK, SLIDE."; onId = nullptr;
            addBig ("choke", "CHOKE"); addSmall ("duck", "DUCK"); addSmall ("glide", "GLIDE");
            cell ("choke", "choke"); cell ("duck", "duck"); cell ("glide", "glide");
            break;
    }

    if (onId != nullptr) power = make<PowerButton> (A, onId);
}

void FxPanel::resized()
{
    const Rectangle<int> S (0, 12, 640, 540);
    if (power != nullptr) power->setBounds (S.getRight() - 108, S.getY() + 14, 90, 26);

    const int n = (int) cells.size(), rows = jmax (1, (n + 3) / 4), cw = 144, ch = 26;
    const int y0 = S.getBottom() - 18 - rows * (ch + 6);
    for (int i = 0; i < n; ++i)
        cells[(size_t) i]->setBounds (S.getX() + 24 + (i % 4) * (cw + 8), y0 + (i / 4) * (ch + 6), cw, ch);

    if (big != nullptr) big->setBounds (720, 70, 160, 178);

    const int ns = (int) smalls.size();
    const int startX = 664 + (272 - ns * 88) / 2;
    for (int i = 0; i < ns; ++i)
        smalls[(size_t) i]->setBounds (startX + i * 88, 300, 84, 108);
}

void FxPanel::paint (Graphics& g)
{
    const Rectangle<int> S (0, 12, 640, 540);
    const bool on = power == nullptr || static_cast<ToggleButton*> (power)->getToggleState();

    g.setFont (ui::mono (36.f, true));
    g.setColour (ui::white (on ? 0.96f : 0.4f));
    g.drawText (title, S.getX(), S.getY() + 14, S.getWidth(), 44, Justification::centred);
    g.setColour (ui::accent.withAlpha (0.3f));
    for (int i = 0; i < 10; ++i)
    {
        g.fillRect ((float) (S.getX() + 24 + i * 9), (float) (S.getY() + 36), 3.f, 1.5f);
        g.fillRect ((float) (S.getX() + 366 + i * 9), (float) (S.getY() + 36), 3.f, 1.5f);
    }
    g.setFont (ui::mono (10.f));
    g.setColour (ui::white (0.5f));
    g.drawText (blurb, S.getX(), S.getY() + 62, S.getWidth(), 14, Justification::centred);

    Graphics::ScopedSaveState ss (g);
    g.setOpacity (on ? 1.f : 0.4f);
    drawAnim (g, Rectangle<float> (28.f, (float) S.getY() + 90.f, 584.f, 362.f));
}

void FxPanel::drawAnim (Graphics& g, Rectangle<float> a)
{
    const float W = a.getWidth(), H = a.getHeight(), cx = a.getCentreX(), cy = a.getCentreY();
    const float tt = (float) t;
    const float pi = MathConstants<float>::pi, twoPi = MathConstants<float>::twoPi;

    g.setColour (ui::accent.withAlpha (0.07f));
    for (float x = a.getX(); x <= a.getRight(); x += 36.5f) g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
    for (float y = a.getY(); y <= a.getBottom(); y += 36.2f) g.drawHorizontalLine ((int) y, a.getX(), a.getRight());

    auto glowStroke = [&g] (const Path& p, float alpha = 1.f)
    {
        g.setColour (ui::accent.withAlpha (0.10f * alpha));
        g.strokePath (p, PathStrokeType (7.f));
        g.setColour (ui::accent.withAlpha (0.28f * alpha));
        g.strokePath (p, PathStrokeType (3.f));
        g.setColour (ui::white (0.92f * alpha));
        g.strokePath (p, PathStrokeType (1.3f));
    };

    switch (type)
    {
        case 0:   // filter response with the LFO sweeping it
        {
            const float cut = v ("cut"), res = v ("res"), ld = v ("lfoDepth"), lr = v ("lfoRate");
            auto curve = [&] (float fc, Path& p)
            {
                for (int i = 0; i <= 120; ++i)
                {
                    const float f = 30.f * std::pow (600.f, (float) i / 120.f);
                    const float r = f / fc, q = 0.6f + res * 1.6f;
                    const float mag = 1.f / std::sqrt ((1.f - r * r) * (1.f - r * r) + (r / q) * (r / q));
                    const float db = jlimit (-42.f, 18.f, 20.f * std::log10 (jmax (1e-4f, mag)));
                    const float x = a.getX() + W * (float) i / 120.f;
                    const float y = a.getBottom() - 8.f - (db + 42.f) / 60.f * (H - 24.f);
                    if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
                }
            };
            for (int k = 3; k >= 0; --k)
            {
                const float ph = tt - 0.12f * (float) k;
                const float fc = jlimit (30.f, 18000.f, cut * std::pow (2.f, ld * 3.f * std::sin (ph * (0.4f + lr * 0.6f))));
                Path p;
                curve (fc, p);
                if (k == 0) glowStroke (p);
                else { g.setColour (ui::accent.withAlpha (0.16f / (float) k)); g.strokePath (p, PathStrokeType (1.f)); }
            }
            break;
        }
        case 1:   // drive: sine in, shaped sine out
        {
            const int ty = (int) v ("drvType");
            const float pre = 1.f + v ("drvAmt") * 24.f;
            Path in, out;
            for (int i = 0; i <= 220; ++i)
            {
                const float x = a.getX() + W * (float) i / 220.f;
                const float s = std::sin (twoPi * ((float) i / 220.f * 2.f + tt * 0.35f)) * 0.9f;
                float o = s * pre;
                o = ty == 1 ? jlimit (-1.f, 1.f, o) : (ty == 2 ? std::sin (o * 1.5708f) : std::tanh (o));
                const float yi = cy - s * H * 0.36f, yo = cy - o * H * 0.36f;
                if (i == 0) { in.startNewSubPath (x, yi); out.startNewSubPath (x, yo); }
                else { in.lineTo (x, yi); out.lineTo (x, yo); }
            }
            g.setColour (ui::accent.withAlpha (0.3f));
            g.strokePath (in, PathStrokeType (1.f));
            glowStroke (out);
            break;
        }
        case 2:   // tape: two reels, wobbling tape, hiss
        {
            const float wow = v ("drift"), fl = v ("tapeFlutter"), nz = v ("tapeNoise");
            for (int s = -1; s <= 1; s += 2)
            {
                const float rx = cx + (float) s * 150.f, ry = cy - 30.f;
                g.setColour (ui::white (0.5f));
                g.drawEllipse (rx - 62.f, ry - 62.f, 124.f, 124.f, 1.5f);
                g.drawEllipse (rx - 14.f, ry - 14.f, 28.f, 28.f, 1.5f);
                for (int k = 0; k < 3; ++k)
                {
                    const float ang = tt * 1.6f * (float) s + (float) k * twoPi / 3.f;
                    g.setColour (ui::accent.withAlpha (0.8f));
                    g.drawLine (rx + std::cos (ang) * 16.f, ry + std::sin (ang) * 16.f,
                                rx + std::cos (ang) * 58.f, ry + std::sin (ang) * 58.f, 2.f);
                }
            }
            Path tape;
            for (int i = 0; i <= 120; ++i)
            {
                const float x = cx - 150.f + 300.f * (float) i / 120.f;
                const float y = cy + 46.f + std::sin (x * 0.03f + tt * 1.3f) * wow * 12.f + std::sin (x * 0.25f + tt * 13.f) * fl * 3.f;
                if (i == 0) tape.startNewSubPath (x, y); else tape.lineTo (x, y);
            }
            glowStroke (tape);
            Random r ((int) (tt * 30.f));
            for (int i = 0; i < (int) (nz * 160.f); ++i)
            {
                g.setColour (ui::white (0.15f + 0.4f * r.nextFloat()));
                g.fillRect (a.getX() + r.nextFloat() * W, a.getBottom() - 60.f + r.nextFloat() * 50.f, 1.5f, 1.5f);
            }
            break;
        }
        case 3:   // halftime: fast waveform on top, stretched copy below
        {
            for (int row = 0; row < 2; ++row)
            {
                const float y0 = a.getY() + H * (row == 0 ? 0.28f : 0.72f);
                const float fr = row == 0 ? 0.33f : 0.165f;
                Path p;
                for (int i = 0; i <= 300; ++i)
                {
                    const float x = a.getX() + W * (float) i / 300.f;
                    const float env = std::exp (-(float) i / 300.f * 3.f * (row == 0 ? 1.f : 0.5f));
                    const float y = y0 - std::sin ((float) i * fr) * env * H * 0.2f;
                    if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
                }
                if (row == 1) glowStroke (p); else { g.setColour (ui::accent.withAlpha (0.45f)); g.strokePath (p, PathStrokeType (1.2f)); }
                const float cur = a.getX() + W * std::fmod (tt * (row == 0 ? 0.5f : 0.25f), 1.f);
                g.setColour (ui::white (0.6f));
                g.drawLine (cur, y0 - H * 0.24f, cur, y0 + H * 0.24f, 1.f);
                g.setFont (ui::mono (11.f));
                g.setColour (ui::white (0.5f));
                g.drawText (row == 0 ? "IN  1x" : "OUT 0.5x", a.getX() + 6.f, y0 - H * 0.26f, 120.f, 14.f, Justification::centredLeft);
            }
            break;
        }
        case 4:   // halfspeed: pitch dives down to the target speed
        {
            const float spd = v ("hsSpeed"), tau = (0.02f + v ("hsDive") * 0.5f) / 3.f;
            auto yAt = [&] (float ts) { return a.getBottom() - 16.f - (1.f - (1.f - spd) * (1.f - std::exp (-ts / tau))) * (H - 40.f) * 0.9f; };
            Path p;
            for (int i = 0; i <= 220; ++i)
            {
                const float x = a.getX() + W * (float) i / 220.f, y = yAt ((float) i / 220.f * 1.5f);
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            glowStroke (p);
            const float u = std::fmod (tt * 0.8f, 1.6f);
            g.setColour (Colours::white);
            g.fillEllipse (a.getX() + W * (u / 1.5f) - 5.f, yAt (jmin (u, 1.5f)) - 5.f, 10.f, 10.f);
            g.setFont (ui::mono (12.f, true));
            g.setColour (ui::white (0.6f));
            g.drawText ("SPEED " + String (spd, 2) + "x", a.getX() + 8.f, a.getY() + 4.f, 200.f, 16.f, Justification::centredLeft);
            break;
        }
        case 5:   // delay: decaying ping-pong echoes
        {
            static const float beats[] = { 1.f, 0.75f, 0.5f, 0.25f };
            const float fb = v ("dlyFb"), spacing = 40.f + 60.f * beats[jlimit (0, 3, (int) v ("dlyTime"))];
            for (int k = 0; k < 10; ++k)
            {
                const float x = a.getX() + 30.f + (float) k * spacing;
                if (x > a.getRight() - 12.f) break;
                const float amp = std::pow (fb, (float) k), h = amp * H * 0.36f;
                g.setColour (ui::accent.withAlpha (0.25f + 0.6f * amp));
                g.fillRoundedRectangle (x - 5.f, (k % 2 == 0) ? cy - h : cy, 10.f, h, 3.f);
            }
            const float px = a.getX() + 30.f + std::fmod (tt * 0.6f, 1.f) * spacing * 8.f;
            g.setColour (ui::white (0.6f));
            g.drawLine (px, a.getY() + 10.f, px, a.getBottom() - 10.f, 1.f);
            g.setColour (ui::white (0.2f));
            g.drawHorizontalLine ((int) cy, a.getX(), a.getRight());
            break;
        }
        case 6:   // reverb: expanding rings
        {
            const float size = v ("revSize"), damp = v ("revDamp"), wid = v ("revWidth");
            for (int k = 0; k < 6; ++k)
            {
                const float ph = std::fmod (tt * (0.55f - size * 0.25f) + (float) k / 6.f, 1.f);
                const float rr = ph * H * (0.45f + size * 0.4f);
                Path e;
                e.addEllipse (cx - rr * (0.6f + 0.4f * wid) * 1.4f, cy - rr, rr * 2.f * (0.6f + 0.4f * wid) * 1.4f, rr * 2.f);
                g.setColour (ui::accent.withAlpha (0.8f * std::pow (1.f - ph, 1.f + damp * 2.f)));
                g.strokePath (e, PathStrokeType (1.4f));
            }
            g.setColour (Colours::white);
            g.fillEllipse (cx - 4.f, cy - 4.f, 8.f, 8.f);
            break;
        }
        default:  // glue: notes cutting / ducking / sliding into each other
        {
            const float choke = v ("choke"), duck = v ("duck"), glide = v ("glide");
            const float step = (W - 60.f) / 6.f;
            float px = 0.f, py = 0.f;
            for (int k = 0; k < 6; ++k)
            {
                const float x = a.getX() + 30.f + (float) k * step;
                const float len = step * (1.f + (1.f - choke) * 1.4f);
                const float y = cy + std::sin ((float) k * 1.9f) * H * 0.22f;
                const float h = 14.f * (1.f - duck * 0.5f);
                g.setColour (ui::accent.withAlpha (0.55f));
                g.fillRoundedRectangle (x, y - h * 0.5f, len, h, 4.f);
                if (glide > 0.f && k > 0)
                {
                    Path c;
                    c.startNewSubPath (px, py);
                    c.quadraticTo ((px + x) * 0.5f, py, x, y);
                    g.setColour (ui::white (0.35f + 0.5f * glide));
                    g.strokePath (c, PathStrokeType (1.4f));
                }
                px = x + step * 0.9f; py = y;
            }
            const float pxh = a.getX() + 30.f + std::fmod (tt * 0.5f, 1.f) * (W - 60.f);
            g.setColour (ui::white (0.6f));
            g.drawLine (pxh, a.getY() + 10.f, pxh, a.getBottom() - 10.f, 1.f);
            break;
        }
    }
    juce::ignoreUnused (pi);
}

//==============================================================================
RiverrEditor::RiverrEditor (RiverrProcessor& p)
    : AudioProcessorEditor (&p), proc (p), playScreen (p),
      playWaveA (p, 0), playWaveB (p, 1), editWaveA (p, 0), editWaveB (p, 1)
{
    setLookAndFeel (&laf);
    auto& A = p.apvts;
    buildImages();

    // ---- header ----
    for (auto* b : { &tabPlay, &tabEdit, &tabFx })
    {
        addAndMakeVisible (b);
        b->setClickingTogglesState (true);
        b->setRadioGroupId (77);
    }
    tabPlay.onClick = [this] { setPage (0); };
    tabEdit.onClick = [this] { setPage (1); };
    tabFx.onClick   = [this] { setPage (2); };

    addAndMakeVisible (presetBox);
    presetBox.setTextWhenNothingSelected ("PRESETS");
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id >= 1000)
        {
            proc.loadPreset (presetBox.getText());
            presetName = presetBox.getText().toUpperCase();
            activeKey = -1;
        }
        else if (id >= 1)
            pressPlayKey (id - 1);
    };
    for (auto* b : { &saveBtn, &loadABtn, &loadBBtn, &resetBtn }) addAndMakeVisible (b);
    resetBtn.getProperties().set ("warn", true);
    saveBtn.onClick = [this] { savePresetDialog(); };
    loadABtn.onClick = [this] { openFileChooser (0); };
    loadBBtn.onClick = [this] { openFileChooser (1); };
    resetBtn.onClick = [this] { pressPlayKey (7); };
    refreshPresets();
    presetBox.setText ("HARD BELL", dontSendNotification);

    // ---- PLAY page ----
    curList = &playComps;
    addAndMakeVisible (playScreen);              playComps.push_back (&playScreen);
    addAndMakeVisible (playWaveA);               playComps.push_back (&playWaveA);
    addAndMakeVisible (playWaveB);               playComps.push_back (&playWaveB);
    for (int i = 0; i < 8; ++i)
    {
        playKeys.push_back (std::make_unique<KeyButton> (kPlayKeyNames[i]));
        addAndMakeVisible (*playKeys.back());
        playComps.push_back (playKeys.back().get());
        playKeys.back()->onClick = [this, i] { pressPlayKey (i); };
    }
    gainKnob = make<Knob> (A, "gain", "OUTPUT", "dB", 1);
    macroKnobs.push_back (make<Knob> (A, "mDark",  "DARK",  "", 2));
    macroKnobs.push_back (make<Knob> (A, "mSpace", "SPACE", "", 2));
    macroKnobs.push_back (make<Knob> (A, "mGrit",  "GRIT",  "", 2));
    macroKnobs.push_back (make<Knob> (A, "mMove",  "MOVE",  "", 2));
    macroKnobs.push_back (make<Knob> (A, "mGlue",  "GLUE",  "", 2));
    macroKnobs.push_back (make<Knob> (A, "mHalf",  "HALF",  "", 2));
    playKeys[0]->setToggleState (true, dontSendNotification);

    // ---- EDIT page ----
    curList = &editComps;
    addAndMakeVisible (editWaveA);  editComps.push_back (&editWaveA);
    addAndMakeVisible (editWaveB);  editComps.push_back (&editWaveB);
    for (auto id : { "aLevel", "octave", "tune", "aPan" })
        slotA.push_back (make<Knob> (A, id, String (id) == "aLevel" ? "LEVEL" : String (id) == "octave" ? "OCT" : String (id) == "tune" ? "TUNE" : "PAN",
                                     String (id) == "aLevel" ? "dB" : String (id) == "tune" ? "st" : ""));
    for (auto id : { "bLevel", "bOct", "bTune", "bPan" })
        slotB.push_back (make<Knob> (A, id, String (id) == "bLevel" ? "LEVEL" : String (id) == "bOct" ? "OCT" : String (id) == "bTune" ? "TUNE" : "PAN",
                                     String (id) == "bLevel" ? "dB" : String (id) == "bTune" ? "st" : ""));

    arpRow.push_back ({ make<Pad> (A, "arpOn", "ARP"), 56 });
    arpRow.push_back ({ make<Combo> (A, "dir", "DIRECTION"), 96 });
    arpRow.push_back ({ make<Combo> (A, "octRange", "OCTAVES"), 62 });
    arpRow.push_back ({ make<Combo> (A, "rate", "RATE"), 70 });
    arpRow.push_back ({ make<Combo> (A, "key", "KEY"), 54 });
    arpRow.push_back ({ make<Combo> (A, "scale", "SCALE"), 104 });
    arpRow.push_back ({ make<Knob> (A, "gate", "GATE"), 54 });
    arpRow.push_back ({ make<Knob> (A, "swing", "SWING"), 54 });
    arpRow.push_back ({ make<Knob> (A, "humT", "HUM T"), 54 });
    arpRow.push_back ({ make<Knob> (A, "humV", "HUM V"), 54 });
    arpRow.push_back ({ make<Knob> (A, "evolve", "EVOLVE"), 54 });
    arpRow.push_back ({ make<Knob> (A, "jump", "JUMP"), 54 });
    arpRow.push_back ({ make<Combo> (A, "layer", "LAYER"), 66 });
    arpRow.push_back ({ make<Knob> (A, "layerLvl", "LVL"), 54 });
    addAndMakeVisible (randBtn);  editComps.push_back (&randBtn);
    addAndMakeVisible (diceBtn);  editComps.push_back (&diceBtn);
    arpRow.push_back ({ &randBtn, 92 });
    arpRow.push_back ({ &diceBtn, 92 });
    randBtn.onClick = [this] { proc.randomizeSteps(); };
    diceBtn.onClick = [this] { proc.diceHard(); };

    for (int i = 0; i < 16; ++i)
    {
        steps.push_back (std::make_unique<StepColumn> (A, i));
        addAndMakeVisible (*steps.back());
        editComps.push_back (steps.back().get());
    }
    for (const char* id : { "steps", "lenPit", "lenVel", "lenGate", "lenProb", "lenRat" })
    {
        lenBoxes.push_back (std::make_unique<LenBox> (A, id));
        addAndMakeVisible (*lenBoxes.back());
        editComps.push_back (lenBoxes.back().get());
    }

    soundRow.push_back ({ make<Combo> (A, "abMode", "A / B MODE"), 80 });
    soundRow.push_back ({ make<Pad> (A, "autotune", "AUTO C5"), 60 });
    soundRow.push_back ({ make<Pad> (A, "reverse", "REVERSE"), 60 });
    soundRow.push_back ({ make<Knob> (A, "scan", "SCAN"), 50 });
    soundRow.push_back ({ make<Knob> (A, "spread", "SPREAD"), 50 });
    soundRow.push_back ({ make<Knob> (A, "atk", "ATTACK", "s"), 50 });
    soundRow.push_back ({ make<Knob> (A, "dec", "DECAY", "s"), 50 });
    soundRow.push_back ({ make<Knob> (A, "sus", "SUSTAIN"), 50 });
    soundRow.push_back ({ make<Knob> (A, "rel", "RELEASE", "s"), 50 });
    soundRow.push_back ({ make<Knob> (A, "gain", "GAIN", "dB"), 50 });

    filterRow.push_back ({ make<Knob> (A, "cut", "CUTOFF", "Hz"), 54 });
    filterRow.push_back ({ make<Knob> (A, "res", "RESO"), 50 });
    filterRow.push_back ({ make<Knob> (A, "lfoRate", "LFO HZ", "Hz"), 54 });
    filterRow.push_back ({ make<Knob> (A, "lfoDepth", "LFO AMT"), 54 });

    interactRow.push_back ({ make<Knob> (A, "choke", "CHOKE"), 54 });
    interactRow.push_back ({ make<Knob> (A, "duck", "DUCK"), 54 });
    interactRow.push_back ({ make<Knob> (A, "glide", "GLIDE"), 54 });

    // ---- FX page ----
    curList = &fxComps;
    for (int i = 0; i < 8; ++i)
    {
        fxKeys.push_back (std::make_unique<KeyButton> (FxPanel::titleOf (kKeyToType[i])));
        addAndMakeVisible (*fxKeys.back());
        fxComps.push_back (fxKeys.back().get());
        const int type = kKeyToType[i];
        fxKeys.back()->onClick = [this, type]
        {
            static const char* onIds[8] = { "fltOn", "drvOn", "tapeOn", "htOn", "hsOn", "dlyOn", "revOn", nullptr };
            const bool same = (type == fxSel);
            selectFx (type);
            if (const char* id = onIds[type])
                if (auto* prm = proc.apvts.getParameter (id))
                {
                    const bool isOn = prm->getValue() > 0.5f;
                    if (same || ! isOn)
                    {
                        prm->beginChangeGesture();
                        prm->setValueNotifyingHost (isOn ? 0.f : 1.f);
                        prm->endChangeGesture();
                    }
                }
        };
    }
    for (int ty = 0; ty < 8; ++ty)
    {
        fxPanels.push_back (std::make_unique<FxPanel> (p, ty));
        addChildComponent (*fxPanels.back());
    }
    curList = nullptr;

    proc.addChangeListener (this);
    startTimerHz (30);
    setSize (ui::kW, ui::kH);
    selectFx (1);
    setPage (0);
}

RiverrEditor::~RiverrEditor()
{
    stopTimer();
    proc.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void RiverrEditor::buildImages()
{
    shell = Image (Image::ARGB, ui::kW, ui::kH, true);
    {
        Graphics g (shell);
        const int W = ui::kW, H = ui::kH;
        g.fillAll (Colour (0xff020204));

        Random r (11);
        for (int i = 0; i < 260; ++i)
        {
            g.setColour (Colours::white.withAlpha (0.08f + 0.5f * r.nextFloat() * r.nextFloat()));
            g.fillRect (r.nextFloat() * (float) W, r.nextFloat() * (float) H, r.nextFloat() < 0.1f ? 2.f : 1.f, 1.f);
        }

        const auto body = Rectangle<float> (14.f, 14.f, (float) W - 28.f, (float) H - 28.f);
        g.setGradientFill (ColourGradient (Colour (0xff2a2a30), 0.f, 14.f, Colour (0xff111114), 0.f, (float) H, false));
        g.fillRoundedRectangle (body, 26.f);

        {
            Graphics::ScopedSaveState ss (g);
            Path clip;
            clip.addRoundedRectangle (body, 26.f);
            g.reduceClipRegion (clip);
            for (int i = 0; i < 5000; ++i)
            {
                g.setColour (r.nextBool() ? Colours::white.withAlpha (0.025f) : Colours::black.withAlpha (0.06f));
                g.fillRect (r.nextFloat() * (float) W, r.nextFloat() * (float) H, 1.5f, 1.5f);
            }
            for (int i = 0; i < 60; ++i)
            {
                const float x = r.nextFloat() * (float) W, y = r.nextFloat() * (float) H;
                const float ang = r.nextFloat() * MathConstants<float>::twoPi, len = 20.f + r.nextFloat() * 90.f;
                g.setColour (Colours::white.withAlpha (0.03f + 0.04f * r.nextFloat()));
                g.drawLine (x, y, x + std::cos (ang) * len, y + std::sin (ang) * len, 0.8f);
            }
            for (int i = 0; i < 6; ++i)
            {
                g.setColour (Colours::black.withAlpha (0.55f));
                g.fillRoundedRectangle (1090.f, 676.f + (float) i * 8.f, 140.f, 3.f, 1.5f);
                g.setColour (ui::white (0.07f));
                g.fillRoundedRectangle (1090.f, 679.f + (float) i * 8.f, 140.f, 1.2f, 0.6f);
            }
        }

        g.setColour (ui::white (0.12f));
        g.drawRoundedRectangle (body.reduced (1.f), 25.f, 1.f);
        g.setColour (Colours::black.withAlpha (0.85f));
        g.drawRoundedRectangle (body, 26.f, 2.f);

        auto screw = [&g] (float cx, float cy)
        {
            g.setGradientFill (ColourGradient (Colour (0xff5c5c63), cx - 3.f, cy - 3.f, Colour (0xff17171a), cx + 4.f, cy + 4.f, true));
            g.fillEllipse (cx - 6.f, cy - 6.f, 12.f, 12.f);
            g.setColour (Colours::black.withAlpha (0.8f));
            g.drawEllipse (cx - 6.f, cy - 6.f, 12.f, 12.f, 1.f);
            g.drawLine (cx - 3.5f, cy + 2.5f, cx + 3.5f, cy - 2.5f, 1.6f);
        };
        screw (34.f, 34.f);            screw ((float) W - 34.f, 34.f);
        screw (34.f, (float) H - 34.f); screw ((float) W - 34.f, (float) H - 34.f);
    }

    screenImg = makeScreenImage (kScreen, 16);
    editImg = makeScreenImage (kEditRect, 6);
}

//==============================================================================
bool RiverrEditor::fxIsOn (int type) const
{
    auto pv = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
    switch (type)
    {
        case 0:  return pv ("fltOn") > 0.5f;
        case 1:  return pv ("drvOn") > 0.5f || pv ("mGrit") > 0.02f;
        case 2:  return pv ("tapeOn") > 0.5f;
        case 3:  return pv ("htOn") > 0.5f || pv ("mHalf") > 0.02f;
        case 4:  return pv ("hsOn") > 0.5f || pv ("mHalf") > 0.02f;
        case 5:  return pv ("dlyOn") > 0.5f;
        case 6:  return pv ("revOn") > 0.5f;
        default: return pv ("choke") + pv ("duck") + pv ("glide") > 0.01f || pv ("mGlue") > 0.02f;
    }
}

void RiverrEditor::selectFx (int type)
{
    fxSel = jlimit (0, 7, type);
    for (int i = 0; i < 8; ++i)
        fxKeys[(size_t) i]->getProperties().set ("sel", kKeyToType[i] == fxSel);
    for (int ty = 0; ty < 8; ++ty)
        fxPanels[(size_t) ty]->setVisible (page == 2 && ty == fxSel);
    for (auto& k : fxKeys) k->repaint();
}

void RiverrEditor::setPage (int p)
{
    page = p;
    for (auto* c : playComps) c->setVisible (p == 0);
    for (auto* c : editComps) c->setVisible (p == 1);
    for (auto* c : fxComps)   c->setVisible (p == 2);
    for (int ty = 0; ty < 8; ++ty)
        fxPanels[(size_t) ty]->setVisible (p == 2 && ty == fxSel);

    tabPlay.setToggleState (p == 0, dontSendNotification);
    tabEdit.setToggleState (p == 1, dontSendNotification);
    tabFx.setToggleState (p == 2, dontSendNotification);
    repaint();
}

void RiverrEditor::advance (double seconds)
{
    for (int i = 0; i < (int) (seconds * 30.0); ++i) timerCallback();
}

void RiverrEditor::pressPlayKey (int i)
{
    static const char* names[] = { "HARD BELL", "DARK PLUCK", "HALFTIME GHOST", "SLIDE MONO", "CRUSHED ROLL" };
    if (i < 5)      { proc.applyFactoryPreset (i); presetName = names[i]; activeKey = i; }
    else if (i == 5) { proc.diceHard();            presetName = "DICE HARD"; activeKey = 5; }
    else if (i == 6) { proc.randomizeSteps();      presetName = "RANDOM";    activeKey = 6; }
    else             { proc.resetToDefault();      presetName = "HARD BELL"; activeKey = 0; }

    for (int k = 0; k < 8; ++k)
        playKeys[(size_t) k]->setToggleState (k == activeKey, dontSendNotification);
    if (i < 5 || i == 7) presetBox.setText (presetName, dontSendNotification);
}

void RiverrEditor::changeListenerCallback (ChangeBroadcaster*)
{
    repaint();
}

void RiverrEditor::timerCallback()
{
    t += 1.0 / 30.0;
    const int hits = proc.hitCount.load();
    if (hits != lastHits) { lastHits = hits; pulse = 1.f; }
    else pulse *= 0.86f;

    if (page == 0)
    {
        playScreen.setState (t, pulse, proc.laneStep[0].load(), presetName);
        playScreen.repaint();
        repaint (520, 690, 240, 26);
    }
    else if (page == 2)
    {
        fxPanels[(size_t) fxSel]->setTime (t);
        fxPanels[(size_t) fxSel]->repaint();
        for (int i = 0; i < 8; ++i)
            fxKeys[(size_t) i]->setToggleState (fxIsOn (kKeyToType[i]), dontSendNotification);
        repaint (520, 690, 240, 26);
    }
    else
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
}

//==============================================================================
void RiverrEditor::setDragHighlight (bool b)
{
    for (auto* w : { &playWaveA, &playWaveB, &editWaveA, &editWaveB }) w->setDragOver (b);
}

bool RiverrEditor::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files)
        if (isAudioFile (f)) return true;
    return false;
}

void RiverrEditor::filesDropped (const StringArray& files, int x, int y)
{
    setDragHighlight (false);
    int slot = 0;
    if (page == 0 && playWaveB.getBounds().contains (x, y)) slot = 1;
    if (page == 1 && editWaveB.getBounds().contains (x, y)) slot = 1;

    for (auto& f : files)
        if (isAudioFile (f) && proc.loadSample (File (f), slot))
            break;
}

void RiverrEditor::openFileChooser (int slot)
{
    chooser = std::make_unique<FileChooser> ("Choose a one-shot", File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    Component::SafePointer<RiverrEditor> safe (this);
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [safe, slot] (const FileChooser& fc)
                          {
                              if (safe == nullptr) return;
                              const auto f = fc.getResult();
                              if (f.existsAsFile()) safe->proc.loadSample (f, slot);
                          });
}

void RiverrEditor::refreshPresets (const String& select)
{
    static const char* fac[] = { "HARD BELL", "DARK PLUCK", "HALFTIME GHOST", "SLIDE MONO", "CRUSHED ROLL" };
    presetBox.clear (dontSendNotification);
    presetBox.addSectionHeading ("FACTORY");
    for (int i = 0; i < 5; ++i) presetBox.addItem (fac[i], 1 + i);

    const auto users = proc.listPresets();
    if (users.size() > 0)
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("USER");
        for (int i = 0; i < users.size(); ++i) presetBox.addItem (users[i], 1000 + i);
    }
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
    g.drawImageAt (shell, 0, 0);

    // engraved logo + author
    g.setGradientFill (ColourGradient (ui::white (0.85f), 0.f, 738.f, ui::white (0.30f), 0.f, 766.f, false));
    g.setFont (Font (FontOptions (28.f, Font::bold)).withExtraKerningFactor (0.35f));
    g.drawText ("RIVERR", ui::kW - 300, 736, 256, 34, Justification::centredRight);
    g.setColour (ui::accent.withAlpha (0.6f));
    g.setFont (ui::mono (11.f));
    g.drawText ("autor: love4xclsv", 46, 746, 260, 16, Justification::centredLeft);

    if (page == 0 || page == 2)
        g.drawImageAt (screenImg, kScreen.getX() - 16, kScreen.getY() - 16);
    else
        g.drawImageAt (editImg, kEditRect.getX() - 6, kEditRect.getY() - 6);

    if (page == 0 || page == 2)
    {
        // little status lights under the screen
        const bool hit = pulse > 0.15f;
        auto led = [&g] (float x, Colour c, bool lit)
        {
            g.setColour (Colours::black.withAlpha (0.8f));
            g.fillRoundedRectangle (x - 1.f, 689.f, 62.f, 16.f, 3.f);
            if (lit) { g.setColour (c.withAlpha (0.3f)); g.fillRoundedRectangle (x - 4.f, 686.f, 68.f, 22.f, 6.f); }
            g.setColour (lit ? c : c.withAlpha (0.18f));
            g.fillRoundedRectangle (x, 691.f, 60.f, 12.f, 2.f);
        };
        led (528.f, ui::accent, hit || std::fmod (t, 2.0) < 1.0);
        led (596.f, Colour (0xffff8fb4), hit && pulse > 0.5f);
        led (664.f, Colour (0xffff2a2a), page == 0 && activeKey >= 0);
    }

    if (page == 0)
    {
        g.setFont (ui::mono (10.5f));
        g.setColour (ui::white (0.42f));
        g.drawText ("1  DROP A SOUND ON THE SCREEN", kKeysX, 490, 250, 16, Justification::centredLeft);
        g.drawText ("2  PICK A KEY", kKeysX, 510, 250, 16, Justification::centredLeft);
        g.drawText ("3  HOLD A CHORD", kKeysX, 530, 250, 16, Justification::centredLeft);
        g.drawText ("4  TWIST THE MACROS", kKeysX, 550, 250, 16, Justification::centredLeft);
        g.setColour (ui::white (0.5f));
        g.drawText ("MACROS", 976, 284, 200, 14, Justification::centredLeft);
    }
    else if (page == 2)
    {
        g.setFont (ui::mono (10.5f));
        g.setColour (ui::white (0.42f));
        g.drawText ("CLICK A KEY = OPEN + TURN ON", kKeysX, 490, 250, 16, Justification::centredLeft);
        g.drawText ("CLICK IT AGAIN = TURN OFF", kKeysX, 510, 250, 16, Justification::centredLeft);
        g.drawText ("THEN TWIST THE KNOBS", kKeysX, 530, 250, 16, Justification::centredLeft);
        g.drawText ("LIT KEY = EFFECT IS ON", kKeysX, 550, 250, 16, Justification::centredLeft);
    }
    else if (page == 1)
    {
        auto section = [&g] (int x, int y, int w, const String& t)
        {
            g.setColour (ui::white (0.5f));
            g.setFont (ui::mono (10.f, true));
            g.drawText (t, x, y, 120, 12, Justification::centredLeft);
            g.setColour (ui::white (0.10f));
            g.drawHorizontalLine (y + 6, (float) (x + 84), (float) (x + w));
        };
        const int x0 = kEditRect.getX() + 14;
        section (x0, yArp, editW, "ARP");
        section (x0, ySeq, editW, "SEQUENCE");
        section (x0, ySound, filterX - x0 - 10, "SOUND");
        section (filterX, ySound, interactX - filterX - 10, "FILTER");
        section (interactX, ySound, x0 + editW - interactX, "GLUE");

        static const char* names[] = { "ON", "PITCH", "VEL", "GATE", "PROB", "RATCH" };
        g.setColour (ui::white (0.5f));
        g.setFont (ui::mono (9.5f));
        for (int l = 0; l < ui::kLanes; ++l)
            g.drawText (names[l], x0, lanesTop + ui::rowTop (l), 46, ui::kRowH[l], Justification::centredLeft);
    }
}

void RiverrEditor::resized()
{
    // header strip
    tabPlay.setBounds (44, 24, 64, 30);
    tabEdit.setBounds (112, 24, 64, 30);
    tabFx.setBounds (180, 24, 64, 30);
    presetBox.setBounds (270, 26, 250, 26);
    saveBtn.setBounds (530, 26, 64, 26);
    loadABtn.setBounds (ui::kW - 44 - 76 - 8 - 84 - 8 - 84, 26, 84, 26);
    loadBBtn.setBounds (ui::kW - 44 - 76 - 8 - 84, 26, 84, 26);
    resetBtn.setBounds (ui::kW - 44 - 76, 26, 76, 26);

    // PLAY page
    playScreen.setBounds (kScreen);
    playWaveA.setBounds (kScreen.getX() + 22, kScreen.getY() + 410, 596, 52);
    playWaveB.setBounds (kScreen.getX() + 22, kScreen.getY() + 468, 596, 52);
    for (int i = 0; i < 8; ++i)
    {
        const int col = i / 4, row = i % 4;
        const auto r = Rectangle<int> (kKeysX + col * (kKeyW + kKeyGap), kKeysY + row * (kKeyH + kKeyGap), kKeyW, kKeyH);
        playKeys[(size_t) i]->setBounds (r);
        fxKeys[(size_t) i]->setBounds (r);
    }
    gainKnob->setBounds (1024, 96, 160, 178);
    for (int i = 0; i < 6; ++i)
        macroKnobs[(size_t) i]->setBounds (976 + (i % 3) * 88, 306 + (i / 3) * 128, 84, 112);

    // FX page
    for (auto& fp : fxPanels) fp->setBounds (304, 64, 936, 660);

    // EDIT page
    auto r = kEditRect.reduced (14, 10);
    editW = r.getWidth();
    const int x0 = r.getX();

    auto waveRow = r.removeFromTop (136);
    auto aRow = waveRow.removeFromTop (64);
    waveRow.removeFromTop (8);
    auto bRow = waveRow;
    const int knobsW = 4 * 56 + 8;
    auto aKnobs = aRow.removeFromRight (knobsW);  aRow.removeFromRight (8);
    auto bKnobs = bRow.removeFromRight (knobsW);  bRow.removeFromRight (8);
    editWaveA.setBounds (aRow);
    editWaveB.setBounds (bRow);
    for (int i = 0; i < 4; ++i)
    {
        slotA[(size_t) i]->setBounds (aKnobs.getX() + i * 58, aKnobs.getY(), 56, 64);
        slotB[(size_t) i]->setBounds (bKnobs.getX() + i * 58, bKnobs.getY(), 56, 64);
    }
    r.removeFromTop (6);

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
    placeRow (r.removeFromTop (84), arpRow, 4);
    r.removeFromTop (6);

    ySeq = r.getY();
    r.removeFromTop (20);
    auto seq = r.removeFromTop (ui::kColH);
    lanesTop = seq.getY();
    seq.removeFromLeft (92);
    const int cw = seq.getWidth() / 16;
    for (int i = 0; i < 16; ++i)
        steps[(size_t) i]->setBounds (seq.getX() + i * cw, lanesTop, cw, ui::kColH);
    for (int l = 0; l < ui::kLanes; ++l)
        lenBoxes[(size_t) l]->setBounds (x0 + 48, lanesTop + ui::rowTop (l) + (ui::kRowH[l] - 16) / 2, 36, 16);
    r.removeFromTop (6);

    ySound = r.getY();
    r.removeFromTop (20);
    auto bottom = r.removeFromTop (84);
    auto soundArea = bottom.removeFromLeft (576);
    bottom.removeFromLeft (10);
    filterX = bottom.getX();
    auto filterArea = bottom.removeFromLeft (222);
    bottom.removeFromLeft (10);
    interactX = bottom.getX();
    placeRow (soundArea, soundRow, 2);
    placeRow (filterArea, filterRow, 2);
    placeRow (bottom, interactRow, 2);
}
