// SPDX-License-Identifier: GPL-3.0-only
#include "Editor.h"

namespace {
const juce::Colour kBack(0xff090c12), kPanel(0xff121823), kPanel2(0xff18202d), kLine(0xff263142), kText(0xffe6edf3),
    kDim(0xff8492a6), kFaint(0xff4a5568), kAccent(0xffffc857);
const char* const kEngine[EPK_ENG_COUNT] = { "PRUEBA", "GRAVE", "METAL", "GOLPE", "TAMBOR", "ACORDE", "DRONE", "MACRO" };
const char* const kRole[EPK_TRACKS] = { "BOMBO", "PLATOS", "ACORDE", "BAJO", "DRON", "PERC" };
// the engine's own names for NOTE, CHARACTER, TONE, DECAY, EXTRA1, EXTRA2, ATTACK (los nombres de cada motor)
const char* const kSound[EPK_ENG_COUNT][7] = {
    { "NOTA", "CARACTER", "TONO", "CAIDA", nullptr, nullptr, "ATAQUE" },
    { "NOTA", "DRIVE", "CLICK", "CAIDA", "BARRIDO", "RUMBLE", nullptr },
    { "AFINA", "METAL", "BRILLO", "CAIDA", "ANILLO", "VARIA", nullptr },
    { "AFINA", "TIPO", "BRILLO", "CAIDA", "CUERPO", "COLA", nullptr },
    { "AFINA", "TIMBRE", "FM", "CAIDA", "BARRIDO", "CLICK", nullptr },
    { "NOTA", "ACORDE", "TONO", "CAIDA", "TABLA", "ENVOLV", "ATAQUE" },
    { "NOTA", "TIMBRE", "TONO", "SOSTEN", "DERIVA", "INTERVALO", "ATAQUE" },
    { "NOTA", "TIMBRE", "TONO", "CAIDA", "MODELO", "ARMON", "ATAQUE" },
};
const char* const kGroup[] = { "SONIDO", "RITMO", "MEZCLA", "VIDA", "MASTER" };

juce::Font font(float size, bool bold = false)
{
    return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
}
int lengthOf(int track) { return juce::jlimit(1, 32, juce::roundToInt(epk_param_get(track, EPK_P_LENGTH) * 31.0f) + 1); }
juce::String noteName(float v)
{
    static const char* const n[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int note = 24 + juce::roundToInt(v * 60.0f);
    return juce::String(n[note % 12]) + juce::String(note / 12 - 1);
}
}

// ------------------------------------------------------------------ how the controls are drawn (el aspecto)
void EpkLook::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float pos, float a0, float a1,
                               juce::Slider& s)
{
    const auto box = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height);
    const float r = juce::jmin(box.getWidth(), box.getHeight()) * 0.5f - 4.0f;
    const auto c = box.getCentre();
    const float w = juce::jmax(2.5f, r * 0.16f);
    const bool on = s.isEnabled();
    juce::Path back, value;
    back.addCentredArc(c.x, c.y, r, r, 0.0f, a0, a1, true);
    g.setColour(kLine);
    g.strokePath(back, juce::PathStrokeType(w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (on) {
        const float a = a0 + pos * (a1 - a0);
        value.addCentredArc(c.x, c.y, r, r, 0.0f, a0, a, true);
        g.setColour(s.findColour(juce::Slider::rotarySliderFillColourId));
        g.strokePath(value, juce::PathStrokeType(w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(kText);
        g.fillEllipse(c.x + std::sin(a) * r - w * 0.55f, c.y - std::cos(a) * r - w * 0.55f, w * 1.1f, w * 1.1f);
    }
    g.setColour(on ? kText : kFaint);
    g.setFont(font(juce::jlimit(9.0f, 15.0f, r * 0.62f), true));
    const auto text = s.getProperties()["text"].toString();
    g.drawText(on ? (text.isNotEmpty() ? text : juce::String(juce::roundToInt(pos * 100.0f))) : juce::String("-"),
               box.toNearestInt(), juce::Justification::centred, false);
}

void EpkLook::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float, float,
                               juce::Slider::SliderStyle, juce::Slider& s)
{
    const float h = juce::jmin(8.0f, (float) height);
    const auto track = juce::Rectangle<float>((float) x, (float) y + ((float) height - h) * 0.5f, (float) width, h);
    g.setColour(kLine);
    g.fillRoundedRectangle(track, h * 0.5f);
    g.setColour(s.findColour(juce::Slider::trackColourId));
    g.fillRoundedRectangle(track.withRight(juce::jmax(track.getX() + h, sliderPos)), h * 0.5f);
    g.setColour(kText);
    g.fillEllipse(juce::jlimit(track.getX(), track.getRight() - h - 4.0f, sliderPos - h * 0.5f - 2.0f), track.getY() - 2.0f,
                  h + 4.0f, h + 4.0f);
}

void EpkLook::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced(0.5f);
    const bool on = b.getToggleState();
    const auto fill = on ? b.findColour(juce::TextButton::buttonOnColourId) : (down ? kLine : highlighted ? kPanel2.brighter(0.08f) : kPanel2);
    g.setColour(b.isEnabled() ? fill : fill.withAlpha(0.4f));
    g.fillRoundedRectangle(r, 6.0f);
    if (!on) {
        g.setColour(kLine);
        g.drawRoundedRectangle(r, 6.0f, 1.0f);
    }
}

juce::Font EpkLook::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    return font(juce::jlimit(10.0f, 13.5f, (float) buttonHeight * 0.44f), true);
}

// ------------------------------------------------------------------ the window (la ventana)
juce::Colour EpkEditor::trackColour(int track)
{
    static const juce::uint32 c[EPK_TRACKS] = { 0xffff5d5d, 0xffffc857, 0xff7bdff2, 0xffb388ff, 0xff5eead4, 0xffff9f6e };
    return juce::Colour(c[juce::jlimit(0, EPK_TRACKS - 1, track)]);
}

EpkEditor::Knob* EpkEditor::addKnob(juce::OwnedArray<Knob>& into, const juce::String& name, int group,
                                    std::function<float()> get, std::function<void(float)> set)
{
    auto* k = into.add(new Knob());
    k->name = name;
    k->group = group;
    k->get = std::move(get);
    k->set = std::move(set);
    k->slider.setRange(0.0, 1.0, 0.0);
    k->slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    k->slider.setColour(juce::Slider::rotarySliderFillColourId, kAccent);
    k->slider.setColour(juce::Slider::trackColourId, kAccent);
    k->slider.onValueChange = [k] { if (k->set) k->set((float) k->slider.getValue()); };
    addAndMakeVisible(k->slider);
    return k;
}

EpkEditor::EpkEditor(EpkProcessor& p) : AudioProcessorEditor(&p), proc(p)
{
    setLookAndFeel(&look);
    setOpaque(true);
    notice.setFont(font(14.0f));
    notice.setColour(juce::Label::textColourId, kAccent);
    notice.setJustificationType(juce::Justification::centred);
    addChildComponent(notice);
    if (!proc.isOwner()) {                 // the core is one engine: a second instance cannot sound (por ahora)
        notice.setText("Solo puede sonar una instancia de EPICYKLOS por proyecto (por ahora).", juce::dontSendNotification);
        notice.setVisible(true);
        setSize(560, 160);
        return;
    }

    auto style = [this] (juce::TextButton& b, juce::Colour on) {
        b.setColour(juce::TextButton::buttonOnColourId, on);
        b.setColour(juce::TextButton::textColourOffId, kText);
        b.setColour(juce::TextButton::textColourOnId, kBack);
        addAndMakeVisible(b);
    };
    style(play, juce::Colour(0xff5eead4));
    play.onClick = [] { epk_play(epk_playing() ? 0 : 1); };
    tempo.setRange(40.0, 200.0, 1.0);
    tempo.setSliderStyle(juce::Slider::LinearHorizontal);
    tempo.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    tempo.setColour(juce::Slider::trackColourId, kAccent);
    tempo.onValueChange = [this] { if (!proc.hostHasTransport.load()) epk_set_tempo((float) tempo.getValue()); };
    addAndMakeVisible(tempo);
    style(random, kAccent);
    random.onClick = [this] {
        seed = seed * 1664525u + 1013904223u + (uint32_t) juce::Time::getMillisecondCounter();
        epk_undo_save();
        epk_pattern_randomize(seed);
    };
    style(undo, kAccent);
    undo.onClick = [] { if (epk_undo_ready()) epk_undo_restore(); };
    scope.addItem("TODO", 1);
    scope.addItem("GROOVE  1 2 6", 2);
    scope.addItem("SINTES  3 4 5", 3);
    scope.setSelectedId(epk_random_scope_get() + 1, juce::dontSendNotification);
    scope.onChange = [this] { epk_random_scope(scope.getSelectedId() - 1); };
    scope.setColour(juce::ComboBox::backgroundColourId, kPanel2);
    scope.setColour(juce::ComboBox::textColourId, kText);
    scope.setColour(juce::ComboBox::outlineColourId, kLine);
    scope.setColour(juce::ComboBox::arrowColourId, kDim);
    addAndMakeVisible(scope);

    for (int b = 0; b < EPK_BANKS; b++) {
        auto* btn = banks.add(new juce::TextButton(juce::String::charToString((juce::juce_wchar) ('A' + b))));
        style(*btn, kAccent);
        btn->onClick = [b] { epk_pattern_select(b, epk_pattern_current()); };
    }
    for (int i = 0; i < EPK_PATTERNS; i++) {
        auto* btn = patterns.add(new juce::TextButton());
        style(*btn, kAccent);
        btn->onClick = [i] { epk_pattern_select(epk_bank_current(), i); };
    }
    for (int t = 0; t < EPK_TRACKS; t++) {
        auto* m = mutes.add(new juce::TextButton("M"));
        style(*m, juce::Colour(0xffff5d5d));
        m->onClick = [t] { epk_mute(t, epk_muted(t) ? 0 : 1); };
        auto* s = solos.add(new juce::TextButton("S"));
        style(*s, kAccent);
        s->onClick = [t] { epk_solo(t, epk_soloed(t) ? 0 : 1); };
        auto* k = addKnob(levels, "NIVEL", 2, [t] { return epk_param_get(t, EPK_P_LEVEL); },
                          [t] (float v) { epk_param_set(t, EPK_P_LEVEL, v); });
        k->slider.setSliderStyle(juce::Slider::LinearHorizontal);
        k->slider.setColour(juce::Slider::trackColourId, trackColour(t));
    }

    struct P { const char* name; int id; int group; };
    static const P kTrack[] = {
        { "NOTA", EPK_P_NOTE, 0 }, { "", EPK_P_CHARACTER, 0 }, { "", EPK_P_TONE, 0 }, { "", EPK_P_DECAY, 0 },
        { "", EPK_P_EXTRA1, 0 }, { "", EPK_P_EXTRA2, 0 }, { "", EPK_P_ATTACK, 0 },
        { "LONGITUD", EPK_P_LENGTH, 1 }, { "GOLPES", EPK_P_HITS, 1 }, { "ROTAR", EPK_P_ROTATE, 1 }, { "SWING", EPK_P_SWING, 1 },
        { "PAN", EPK_P_PAN, 2 }, { "A ECO", EPK_P_SEND_A, 2 }, { "A REVERB", EPK_P_SEND_B, 2 },
        { "MUTAR", EPK_P_MUTATE, 3 }, { "AUSENCIA", EPK_P_ABSENCE, 3 } };
    for (const auto& q : kTrack) {
        const int id = q.id;
        auto* k = addKnob(sound, q.name, q.group, [this, id] { return epk_param_get(proc.selectedTrack.load(), id); },
                          [this, id] (float v) { epk_param_set(proc.selectedTrack.load(), id, v); });
        k->param = id;
    }
    static const P kFx[] = { { "ECO", EPK_FX_DLY_LEVEL, 4 }, { "REVERB", EPK_FX_REV_LEVEL, 4 }, { "REV CAIDA", EPK_FX_REV_DECAY, 4 },
                             { "SHIMMER", EPK_FX_REV_SHIMMER, 4 }, { "FILTRO", EPK_FX_FLT, 4 }, { "RESO", EPK_FX_FLT_RESO, 4 },
                             { "COMP", EPK_FX_COMP, 4 } };
    for (const auto& q : kFx) {
        const int id = q.id;
        addKnob(fx, q.name, q.group, [id] { return epk_fx_get(id); }, [id] (float v) { epk_fx_set(id, v); });
    }

    setResizable(true, true);
    setResizeLimits(980, 640, 2000, 1300);
    setSize(1160, 720);
    timerCallback();                       // the first drawing already has its texts (ya con sus textos)
    startTimerHz(30);
}

EpkEditor::~EpkEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

juce::Rectangle<float> EpkEditor::cell(int track, int step, int length) const
{
    const auto a = stepsArea[track].toFloat();
    const float w = a.getWidth() / (float) juce::jmax(16, length);
    return juce::Rectangle<float>(a.getX() + w * (float) step, a.getY(), w, a.getHeight()).reduced(juce::jmin(2.0f, w * 0.12f), 0.0f);
}

void EpkEditor::drawOrbit(juce::Graphics& g, juce::Rectangle<float> box)
{
    g.setColour(kPanel);
    g.fillRoundedRectangle(box, 12.0f);
    const auto c = box.getCentre();
    const float R = juce::jmin(box.getWidth(), box.getHeight()) * 0.5f - 14.0f;
    const int sel = proc.selectedTrack.load();
    for (int t = 0; t < EPK_TRACKS; t++) {
        const float r = R * (1.0f - 0.125f * (float) t);     // track 1 outside (la pista 1, fuera)
        const int len = lengthOf(t), cur = epk_step(t);
        const uint32_t mask = epk_pattern(t);
        const bool heard = epk_audible(t) != 0;
        const auto col = trackColour(t).withAlpha(heard ? 1.0f : 0.25f);
        g.setColour((t == sel ? col.withAlpha(0.35f) : kLine.withAlpha(0.7f)));
        g.drawEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f, t == sel ? 1.6f : 1.0f);
        for (int s = 0; s < len; s++) {
            const float a = juce::MathConstants<float>::twoPi * (float) s / (float) len;
            const float px = c.x + std::sin(a) * r, py = c.y - std::cos(a) * r;
            const bool hit = (mask >> s & 1u) != 0, now = s == cur;
            const float d = hit ? juce::jmax(5.0f, R * 0.034f) : 2.2f;
            if (now) {
                g.setColour(kText.withAlpha(hit ? 0.95f : 0.45f));
                g.fillEllipse(px - d * 0.5f - 3.0f, py - d * 0.5f - 3.0f, d + 6.0f, d + 6.0f);
            }
            g.setColour(hit ? col : kFaint.withAlpha(0.8f));
            g.fillEllipse(px - d * 0.5f, py - d * 0.5f, d, d);
        }
    }
    const int bank = epk_bank_current(), pat = epk_pattern_current();
    g.setColour(kText);
    g.setFont(font(juce::jmax(13.0f, R * 0.115f), true));
    g.drawText(juce::String(epk_pattern_name(bank, pat)), box.withSizeKeepingCentre(R * 0.9f, 22.0f).translated(0.0f, -8.0f).toNearestInt(),
               juce::Justification::centred, false);
    g.setColour(kDim);
    g.setFont(font(juce::jmax(10.0f, R * 0.07f)));
    g.drawText(juce::String::charToString((juce::juce_wchar) ('A' + bank)) + juce::String(pat + 1).paddedLeft('0', 2) + "  ·  " +
                   juce::String(juce::roundToInt(epk_tempo())) + " BPM",
               box.withSizeKeepingCentre(R * 0.9f, 16.0f).translated(0.0f, 12.0f).toNearestInt(), juce::Justification::centred, false);
}

void EpkEditor::drawLane(juce::Graphics& g, int t)
{
    const auto row = laneArea[t].toFloat();
    const bool sel = proc.selectedTrack.load() == t, heard = epk_audible(t) != 0;
    const auto col = trackColour(t);
    g.setColour(sel ? kPanel2 : kPanel);
    g.fillRoundedRectangle(row, 8.0f);
    if (sel) {
        g.setColour(col);
        g.drawRoundedRectangle(row.reduced(0.75f), 8.0f, 1.5f);
    }
    g.setColour(col.withAlpha(heard ? 0.35f + 0.65f * flash[t] : 0.15f));
    g.fillRoundedRectangle(row.getX() + 8.0f, row.getY() + 8.0f, 5.0f, row.getHeight() - 16.0f, 2.5f);
    g.setColour(heard ? kText : kFaint);
    g.setFont(font(13.0f, true));
    g.drawText(juce::String(t + 1) + "  " + kRole[t], juce::Rectangle<float>(row.getX() + 22.0f, row.getY() + 5.0f, 110.0f, 18.0f).toNearestInt(),
               juce::Justification::centredLeft, false);
    g.setColour(kDim);
    g.setFont(font(10.5f));
    juce::String eng = kEngine[juce::jlimit(0, EPK_ENG_COUNT - 1, epk_engine(t))];
    if (epk_engine(t) == EPK_ENG_MACRO)
        eng << " " << epk_macro_name(juce::roundToInt(epk_param_get(t, EPK_P_EXTRA1) * (float) (EPK_MACRO_MODELS - 1)));
    g.drawText(eng, juce::Rectangle<float>(row.getX() + 22.0f, row.getBottom() - 21.0f, 110.0f, 16.0f).toNearestInt(),
               juce::Justification::centredLeft, false);

    const int len = lengthOf(t), cur = epk_step(t);
    const uint32_t mask = epk_pattern(t);
    for (int s = 0; s < len; s++) {
        const auto r = cell(t, s, len);
        const bool hit = (mask >> s & 1u) != 0;
        g.setColour(hit ? col.withAlpha(heard ? 1.0f : 0.3f) : (s % 4 == 0 ? kLine.brighter(0.12f) : kLine));
        g.fillRoundedRectangle(r, 3.0f);
        if (s == cur) {
            g.setColour(kText);
            g.drawRoundedRectangle(r.reduced(0.75f), 3.0f, 1.5f);
        }
    }
}

void EpkEditor::drawGroups(juce::Graphics& g)
{
    auto caption = [&g] (const juce::OwnedArray<Knob>& list, juce::Colour c) {
        int first = 0;
        while (first < list.size()) {
            int last = first;
            while (last + 1 < list.size() && list[last + 1]->group == list[first]->group
                   && list[last + 1]->slider.getY() == list[first]->slider.getY()) last++;
            const auto a = list[first]->slider.getBounds(), b = list[last]->slider.getBounds();
            const auto box = juce::Rectangle<int>(a.getX() - 4, a.getY() - 20, b.getRight() - a.getX() + 8, a.getHeight() + 40).toFloat();
            g.setColour(kPanel);
            g.fillRoundedRectangle(box, 8.0f);
            g.setColour(c);
            g.setFont(font(10.0f, true));
            g.drawText(kGroup[list[first]->group], box.withHeight(18.0f).reduced(8.0f, 0.0f).toNearestInt(),
                       juce::Justification::centredLeft, false);
            for (int i = first; i <= last; i++) {
                const auto s = list[i]->slider.getBounds();
                g.setColour(list[i]->slider.isEnabled() ? kDim : kFaint);
                g.setFont(font(10.0f));
                g.drawText(list[i]->name, s.withY(s.getBottom()).withHeight(16).expanded(8, 0), juce::Justification::centred, false);
            }
            first = last + 1;
        }
    };
    caption(sound, trackColour(proc.selectedTrack.load()));
    caption(fx, kAccent);
}

void EpkEditor::paint(juce::Graphics& g)
{
    g.fillAll(kBack);
    if (!proc.isOwner()) return;
    g.setColour(kText);
    g.setFont(font(21.0f, true));
    g.drawText("EPICYKLOS", header.withWidth(150), juce::Justification::centredLeft, false);
    g.setColour(kDim);
    g.setFont(font(13.0f, true));
    const bool host = proc.hostHasTransport.load();
    g.drawText(juce::String(juce::roundToInt(epk_tempo())) + (host ? " BPM  (DAW)" : " BPM"),
               juce::Rectangle<int>(play.getRight() + 12, header.getY(), 110, header.getHeight()), juce::Justification::centredLeft, false);
    drawOrbit(g, orbitArea.toFloat());
    for (int t = 0; t < EPK_TRACKS; t++) drawLane(g, t);
    drawGroups(g);
}

void EpkEditor::resized()
{
    auto area = getLocalBounds().reduced(14);
    if (!proc.isOwner()) {
        notice.setBounds(area);
        return;
    }
    header = area.removeFromTop(38);
    auto top = header;
    top.removeFromLeft(150);
    play.setBounds(top.removeFromLeft(84).reduced(0, 3));
    top.removeFromLeft(122);
    tempo.setBounds(top.removeFromLeft(150).reduced(0, 10));
    undo.setBounds(top.removeFromRight(100).reduced(0, 3));
    top.removeFromRight(6);
    random.setBounds(top.removeFromRight(110).reduced(0, 3));
    top.removeFromRight(6);
    scope.setBounds(top.removeFromRight(150).reduced(0, 5));
    area.removeFromTop(10);

    // below: the knobs, three rows (abajo, los mandos)
    const int knobRow = juce::jlimit(74, 104, area.getHeight() / 8);
    auto knobs = area.removeFromBottom(knobRow * 2 + 34);
    area.removeFromBottom(8);
    auto place = [] (juce::Rectangle<int> row, const juce::OwnedArray<Knob>& list, int from, int count, int slots) {
        const int w = row.getWidth() / slots;
        int lastGroup = -1, x = row.getX();
        for (int i = from; i < from + count && i < list.size(); i++) {
            if (lastGroup >= 0 && list[i]->group != lastGroup) x += 14;
            lastGroup = list[i]->group;
            list[i]->slider.setBounds(juce::Rectangle<int>(x, row.getY() + 20, w - 6, row.getHeight() - 40).reduced(6, 0));
            x += w - 6;
        }
    };
    auto row1 = knobs.removeFromTop(knobRow + 17), row2 = knobs;
    place(row1, sound, 0, 11, 12);          // SONIDO 7 + RITMO 4
    auto r2 = row2;
    place(r2.removeFromLeft(r2.getWidth() * 5 / 12 + 20), sound, 11, 5, 5);   // MEZCLA 3 + VIDA 2
    r2.removeFromLeft(14);
    place(r2, fx, 0, fx.size(), fx.size());

    // left: the orbit, then banks and patterns; right: the six lanes (a la derecha, las seis pistas)
    const int leftW = juce::jlimit(250, 400, juce::jmin(area.getWidth() * 34 / 100, area.getHeight() - 96));
    auto left = area.removeFromLeft(leftW);
    area.removeFromLeft(12);
    auto grid = left.removeFromBottom(92);
    left.removeFromBottom(8);
    orbitArea = left;
    auto bankRow = grid.removeFromTop(24);
    const int bw = bankRow.getWidth() / EPK_BANKS;
    for (auto* b : banks) b->setBounds(bankRow.removeFromLeft(bw).reduced(1));
    grid.removeFromTop(4);
    for (int row = 0; row < 4; row++) {
        auto r = grid.removeFromTop(16);
        const int pw = r.getWidth() / 4;
        for (int i = 0; i < 4; i++) patterns[row * 4 + i]->setBounds(r.removeFromLeft(pw).reduced(1, 0));
    }
    const int lh = area.getHeight() / EPK_TRACKS;
    for (int t = 0; t < EPK_TRACKS; t++) {
        auto row = area.removeFromTop(lh).reduced(0, 3);
        laneArea[t] = row;
        auto inner = row.reduced(8, 7);
        inner.removeFromLeft(128);
        auto right = inner.removeFromRight(190);
        solos[t]->setBounds(right.removeFromRight(30).reduced(1, juce::jmax(0, (right.getHeight() - 26) / 2)));
        mutes[t]->setBounds(right.removeFromRight(30).reduced(1, juce::jmax(0, (right.getHeight() - 26) / 2)));
        levels[t]->slider.setBounds(right.reduced(10, 0));
        inner.removeFromRight(6);
        stepsArea[t] = inner.reduced(0, juce::jmax(0, (inner.getHeight() - 30) / 2));
    }
}

void EpkEditor::mouseDown(const juce::MouseEvent& e)
{
    if (!proc.isOwner()) return;
    for (int t = 0; t < EPK_TRACKS; t++)
        if (laneArea[t].contains(e.getPosition())) {
            proc.selectedTrack.store(t);
            if (stepsArea[t].contains(e.getPosition())) {   // a click on a step turns it on or off (enciende o apaga)
                const int len = lengthOf(t);
                for (int s = 0; s < len; s++)
                    if (cell(t, s, len).expanded(2.0f, 0.0f).contains(e.position)) {
                        epk_step_toggle(t, s);
                        break;
                    }
            }
            repaint();
            return;
        }
}

void EpkEditor::timerCallback()
{
    const bool host = proc.hostHasTransport.load();
    play.setToggleState(epk_playing() != 0, juce::dontSendNotification);
    play.setEnabled(!host);
    tempo.setVisible(!host);
    if (!tempo.isMouseButtonDown()) tempo.setValue(epk_tempo(), juce::dontSendNotification);
    undo.setEnabled(epk_undo_ready() != 0);
    if (scope.getSelectedId() != epk_random_scope_get() + 1 && !scope.isPopupActive())
        scope.setSelectedId(epk_random_scope_get() + 1, juce::dontSendNotification);

    const int bank = epk_bank_current(), pat = epk_pattern_current(), sel = proc.selectedTrack.load();
    for (int b = 0; b < banks.size(); b++) banks[b]->setToggleState(b == bank, juce::dontSendNotification);
    for (int i = 0; i < patterns.size(); i++) {
        const juce::String name(epk_pattern_name(bank, i));
        if (patterns[i]->getButtonText() != name) patterns[i]->setButtonText(name);
        patterns[i]->setToggleState(i == pat, juce::dontSendNotification);
    }
    for (int t = 0; t < EPK_TRACKS; t++) {
        mutes[t]->setToggleState(epk_muted(t) != 0, juce::dontSendNotification);
        solos[t]->setToggleState(epk_soloed(t) != 0, juce::dontSendNotification);
        const uint32_t trig = epk_triggers(t);
        if (trig != lastTrig[t]) { lastTrig[t] = trig; flash[t] = 1.0f; }
        else flash[t] *= 0.8f;
    }

    // the selected track's knobs take its colour and its engine's names (color y nombres del motor)
    const int eng = juce::jlimit(0, EPK_ENG_COUNT - 1, epk_engine(sel));
    if (sel != lastSel || eng != lastEngine) {
        lastSel = sel;
        lastEngine = eng;
        for (int i = 0; i < sound.size(); i++) {
            auto* k = sound[i];
            k->slider.setColour(juce::Slider::rotarySliderFillColourId, trackColour(sel));
            if (i < 7) {
                const char* n = kSound[eng][i];
                k->name = n != nullptr ? n : "";
                k->slider.setEnabled(n != nullptr);
            }
        }
    }
    for (auto* list : { &levels, &sound, &fx })
        for (auto* k : *list) {
            if (!k->get) continue;
            const float v = k->get();
            if (!k->slider.isMouseButtonDown()) k->slider.setValue(v, juce::dontSendNotification);
            juce::String text;
            if (k->param == EPK_P_NOTE) text = noteName(v);
            else if (k->param == EPK_P_LENGTH) text = juce::String(juce::roundToInt(v * 31.0f) + 1);
            else if (k->param == EPK_P_EXTRA1 && eng == EPK_ENG_MACRO)
                text = juce::String(epk_macro_name(juce::roundToInt(v * (float) (EPK_MACRO_MODELS - 1)))).substring(0, 4);
            else if (k->param == EPK_P_PAN) { const int p = juce::roundToInt(v * 100.0f) - 50; text = p == 0 ? "C" : (p < 0 ? "L" : "R") + juce::String(std::abs(p)); }
            k->slider.getProperties().set("text", text);
        }
    repaint();
}
