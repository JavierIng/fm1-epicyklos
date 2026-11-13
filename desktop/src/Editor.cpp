// SPDX-License-Identifier: GPL-3.0-only
#include "Editor.h"

namespace {
const juce::Colour kBack(0xff0b0e14), kPanel(0xff141a24), kLine(0xff263142), kText(0xffe6edf3), kDim(0xff8b98a9),
    kAccent(0xffffc857);
const char* const kEngine[EPK_ENG_COUNT] = { "PRUEBA", "GRAVE", "METAL", "GOLPE", "TAMBOR", "ACORDE", "DRONE", "MACRO" };
const char* const kRole[EPK_TRACKS] = { "BOMBO", "PLATOS", "ACORDE", "BAJO", "DRON", "PERC" };

void styleButton(juce::TextButton& b, juce::Colour on)
{
    b.setColour(juce::TextButton::buttonColourId, kPanel);
    b.setColour(juce::TextButton::buttonOnColourId, on);
    b.setColour(juce::TextButton::textColourOffId, kText);
    b.setColour(juce::TextButton::textColourOnId, kBack);
}
void styleLabel(juce::Label& l, float size, juce::Colour c, juce::Justification j = juce::Justification::centredLeft)
{
    l.setFont(juce::FontOptions(size));
    l.setColour(juce::Label::textColourId, c);
    l.setJustificationType(j);
    l.setInterceptsMouseClicks(false, false);
}
}

juce::Colour EpkEditor::trackColour(int track)
{
    static const juce::uint32 c[EPK_TRACKS] = { 0xffff5d5d, 0xffffc857, 0xff7bdff2, 0xffb388ff, 0xff5eead4, 0xffff9f6e };
    return juce::Colour(c[juce::jlimit(0, EPK_TRACKS - 1, track)]);
}

EpkEditor::Knob* EpkEditor::addKnob(juce::OwnedArray<Knob>& into, const juce::String& name, std::function<float()> get,
                                    std::function<void(float)> set, bool vertical)
{
    auto* k = into.add(new Knob());
    k->get = std::move(get);
    k->set = std::move(set);
    k->slider.setRange(0.0, 1.0, 0.0);
    k->slider.setSliderStyle(vertical ? juce::Slider::LinearVertical : juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    k->slider.setColour(juce::Slider::rotarySliderFillColourId, kAccent);
    k->slider.setColour(juce::Slider::rotarySliderOutlineColourId, kLine);
    k->slider.setColour(juce::Slider::thumbColourId, kText);
    k->slider.setColour(juce::Slider::trackColourId, kAccent);
    k->slider.setColour(juce::Slider::backgroundColourId, kLine);
    k->slider.onValueChange = [k] { if (k->set) k->set((float) k->slider.getValue()); };
    styleLabel(k->label, 11.0f, kDim, juce::Justification::centred);
    k->label.setText(name, juce::dontSendNotification);
    addAndMakeVisible(k->slider);
    addAndMakeVisible(k->label);
    return k;
}

EpkEditor::EpkEditor(EpkProcessor& p) : AudioProcessorEditor(&p), proc(p)
{
    setOpaque(true);
    styleLabel(title, 22.0f, kText);
    title.setText("EPICYKLOS", juce::dontSendNotification);
    addAndMakeVisible(title);
    styleLabel(notice, 14.0f, kAccent, juce::Justification::centred);
    addChildComponent(notice);

    if (!proc.isOwner()) {                 // the core is one engine: a second instance cannot sound (por ahora)
        notice.setText("Solo puede sonar una instancia de EPICYKLOS por proyecto (por ahora).", juce::dontSendNotification);
        notice.setVisible(true);
        setSize(560, 160);
        return;
    }

    styleButton(play, juce::Colour(0xff5eead4));
    play.onClick = [] { epk_play(epk_playing() ? 0 : 1); };
    addAndMakeVisible(play);

    styleLabel(tempoLabel, 16.0f, kText, juce::Justification::centred);
    addAndMakeVisible(tempoLabel);
    tempo.setRange(40.0, 200.0, 1.0);
    tempo.setSliderStyle(juce::Slider::LinearHorizontal);
    tempo.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    tempo.setColour(juce::Slider::trackColourId, kAccent);
    tempo.setColour(juce::Slider::backgroundColourId, kLine);
    tempo.onValueChange = [this] { if (!proc.hostHasTransport.load()) epk_set_tempo((float) tempo.getValue()); };
    addAndMakeVisible(tempo);

    styleButton(random, kAccent);
    random.onClick = [this] {
        seed = seed * 1664525u + 1013904223u + (uint32_t) juce::Time::getMillisecondCounter();
        epk_undo_save();
        epk_pattern_randomize(seed);
    };
    addAndMakeVisible(random);
    styleButton(undo, kAccent);
    undo.onClick = [] { if (epk_undo_ready()) epk_undo_restore(); };
    addAndMakeVisible(undo);
    scope.addItem("TODO", 1);
    scope.addItem("GROOVE (1 2 6)", 2);
    scope.addItem("SINTES (3 4 5)", 3);
    scope.setSelectedId(epk_random_scope_get() + 1, juce::dontSendNotification);
    scope.onChange = [this] { epk_random_scope(scope.getSelectedId() - 1); };
    scope.setColour(juce::ComboBox::backgroundColourId, kPanel);
    scope.setColour(juce::ComboBox::textColourId, kText);
    scope.setColour(juce::ComboBox::outlineColourId, kLine);
    addAndMakeVisible(scope);

    styleLabel(patternTitle, 12.0f, kDim);
    patternTitle.setText("BANCOS Y PATRONES", juce::dontSendNotification);
    addAndMakeVisible(patternTitle);
    for (int b = 0; b < EPK_BANKS; b++) {
        auto* btn = banks.add(new juce::TextButton(juce::String::charToString((juce::juce_wchar) ('A' + b))));
        styleButton(*btn, kAccent);
        btn->onClick = [b] { epk_pattern_select(b, epk_pattern_current()); };
        addAndMakeVisible(btn);
    }
    for (int i = 0; i < EPK_PATTERNS; i++) {
        auto* btn = patterns.add(new juce::TextButton());
        styleButton(*btn, kAccent);
        btn->onClick = [i] { epk_pattern_select(epk_bank_current(), i); };
        addAndMakeVisible(btn);
    }

    for (int t = 0; t < EPK_TRACKS; t++) {
        auto* sel = selects.add(new juce::TextButton());
        styleButton(*sel, trackColour(t));
        sel->onClick = [this, t] { proc.selectedTrack.store(t); };
        addAndMakeVisible(sel);
        auto* m = mutes.add(new juce::TextButton("M"));
        styleButton(*m, juce::Colour(0xffff5d5d));
        m->onClick = [t] { epk_mute(t, epk_muted(t) ? 0 : 1); };
        addAndMakeVisible(m);
        auto* s = solos.add(new juce::TextButton("S"));
        styleButton(*s, kAccent);
        s->onClick = [t] { epk_solo(t, epk_soloed(t) ? 0 : 1); };
        addAndMakeVisible(s);
        auto* k = addKnob(levels, "NIVEL", [t] { return epk_param_get(t, EPK_P_LEVEL); },
                          [t] (float v) { epk_param_set(t, EPK_P_LEVEL, v); }, true);
        k->slider.setColour(juce::Slider::trackColourId, trackColour(t));
    }

    styleLabel(trackTitle, 12.0f, kDim);
    addAndMakeVisible(trackTitle);
    struct P { const char* name; int id; };
    static const P kSound[] = { { "CARACTER", EPK_P_CHARACTER }, { "TONO", EPK_P_TONE }, { "CAIDA", EPK_P_DECAY },
                                { "MOTOR 1", EPK_P_EXTRA1 }, { "MOTOR 2", EPK_P_EXTRA2 }, { "ATAQUE", EPK_P_ATTACK },
                                { "PAN", EPK_P_PAN }, { "A ECO", EPK_P_SEND_A }, { "A REVERB", EPK_P_SEND_B },
                                { "MUTAR", EPK_P_MUTATE }, { "AUSENCIA", EPK_P_ABSENCE } };
    for (const auto& q : kSound) {
        const int id = q.id;
        addKnob(sound, q.name, [this, id] { return epk_param_get(proc.selectedTrack.load(), id); },
                [this, id] (float v) { epk_param_set(proc.selectedTrack.load(), id, v); });
    }

    styleLabel(fxTitle, 12.0f, kDim);
    fxTitle.setText("EFECTOS Y MASTER", juce::dontSendNotification);
    addAndMakeVisible(fxTitle);
    static const P kFx[] = { { "ECO", EPK_FX_DLY_LEVEL }, { "REVERB", EPK_FX_REV_LEVEL }, { "REV CAIDA", EPK_FX_REV_DECAY },
                             { "FILTRO", EPK_FX_FLT }, { "RESO", EPK_FX_FLT_RESO }, { "COMP", EPK_FX_COMP } };
    for (const auto& q : kFx) {
        const int id = q.id;
        addKnob(fx, q.name, [id] { return epk_fx_get(id); }, [id] (float v) { epk_fx_set(id, v); });
    }

    setResizable(true, true);
    setResizeLimits(820, 520, 1800, 1200);
    setSize(980, 620);
    timerCallback();                       // the first drawing already has its texts (ya con sus textos)
    startTimerHz(30);
}

EpkEditor::~EpkEditor() { stopTimer(); }

void EpkEditor::paint(juce::Graphics& g)
{
    g.fillAll(kBack);
    if (!proc.isOwner()) return;
    for (int t = 0; t < EPK_TRACKS; t++) {
        const auto r = stripArea[t].toFloat();
        const bool sel = proc.selectedTrack.load() == t;
        g.setColour(kPanel);
        g.fillRoundedRectangle(r, 8.0f);
        g.setColour(sel ? trackColour(t) : kLine);
        g.drawRoundedRectangle(r.reduced(0.5f), 8.0f, sel ? 2.0f : 1.0f);
        g.setColour(trackColour(t).withAlpha(0.15f + 0.85f * flash[t]));   // lights on each hit (se enciende)
        g.fillEllipse(r.getRight() - 20.0f, r.getY() + 10.0f, 10.0f, 10.0f);
    }
}

void EpkEditor::resized()
{
    auto area = getLocalBounds().reduced(14);
    if (!proc.isOwner()) {
        title.setBounds(area.removeFromTop(34));
        notice.setBounds(area);
        return;
    }
    auto top = area.removeFromTop(40);
    title.setBounds(top.removeFromLeft(170));
    play.setBounds(top.removeFromLeft(90).reduced(2));
    tempoLabel.setBounds(top.removeFromLeft(96));
    tempo.setBounds(top.removeFromLeft(150).reduced(2, 8));
    undo.setBounds(top.removeFromRight(100).reduced(2));
    random.setBounds(top.removeFromRight(110).reduced(2));
    scope.setBounds(top.removeFromRight(150).reduced(2, 6));
    area.removeFromTop(8);

    patternTitle.setBounds(area.removeFromTop(18));
    auto bankRow = area.removeFromTop(30);
    const int bw = bankRow.getWidth() / EPK_BANKS;
    for (auto* b : banks) b->setBounds(bankRow.removeFromLeft(bw).reduced(2));
    for (int row = 0; row < 2; row++) {
        auto r = area.removeFromTop(34);
        const int pw = r.getWidth() / 8;
        for (int i = 0; i < 8; i++) patterns[row * 8 + i]->setBounds(r.removeFromLeft(pw).reduced(2));
    }
    area.removeFromTop(10);

    auto bottom = area.removeFromBottom(juce::jmax(150, area.getHeight() * 2 / 5));
    auto strips = area;
    const int sw = strips.getWidth() / EPK_TRACKS;
    for (int t = 0; t < EPK_TRACKS; t++) {
        auto s = strips.removeFromLeft(sw).reduced(4, 2);
        stripArea[t] = s;
        s = s.reduced(8);
        selects[t]->setBounds(s.removeFromTop(30).withTrimmedRight(18));
        auto ms = s.removeFromBottom(26);
        mutes[t]->setBounds(ms.removeFromLeft(ms.getWidth() / 2).reduced(2));
        solos[t]->setBounds(ms.reduced(2));
        levels[t]->label.setBounds(s.removeFromBottom(14));
        levels[t]->slider.setBounds(s.reduced(s.getWidth() / 3, 2));
    }

    bottom.removeFromTop(8);
    auto soundArea = bottom.removeFromTop(bottom.getHeight() * 3 / 5);
    trackTitle.setBounds(soundArea.removeFromTop(18));
    const int kw = soundArea.getWidth() / sound.size();
    for (auto* k : sound) {
        auto c = soundArea.removeFromLeft(kw);
        k->label.setBounds(c.removeFromBottom(14));
        k->slider.setBounds(c.reduced(4));
    }
    fxTitle.setBounds(bottom.removeFromTop(18));
    const int fw = bottom.getWidth() / juce::jmax(1, sound.size());
    for (auto* k : fx) {
        auto c = bottom.removeFromLeft(fw);
        k->label.setBounds(c.removeFromBottom(14));
        k->slider.setBounds(c.reduced(4));
    }
}

void EpkEditor::timerCallback()
{
    const bool host = proc.hostHasTransport.load();
    play.setToggleState(epk_playing() != 0, juce::dontSendNotification);
    play.setEnabled(!host);
    tempo.setVisible(!host);
    tempoLabel.setText(juce::String(juce::roundToInt(epk_tempo())) + (host ? " BPM (DAW)" : " BPM"), juce::dontSendNotification);
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
    shownBank = bank;

    bool repaintStrips = false;
    for (int t = 0; t < EPK_TRACKS; t++) {
        const int eng = juce::jlimit(0, EPK_ENG_COUNT - 1, epk_engine(t));
        const juce::String text = juce::String(t + 1) + "  " + kRole[t] + " · " + kEngine[eng];
        if (selects[t]->getButtonText() != text) selects[t]->setButtonText(text);
        selects[t]->setToggleState(t == sel, juce::dontSendNotification);
        mutes[t]->setToggleState(epk_muted(t) != 0, juce::dontSendNotification);
        solos[t]->setToggleState(epk_soloed(t) != 0, juce::dontSendNotification);
        const uint32_t trig = epk_triggers(t);
        if (trig != lastTrig[t]) { lastTrig[t] = trig; flash[t] = 1.0f; repaintStrips = true; }
        else if (flash[t] > 0.01f) { flash[t] *= 0.8f; repaintStrips = true; }
    }
    trackTitle.setText("PISTA " + juce::String(sel + 1) + " · " + kRole[sel] + " · " + kEngine[juce::jlimit(0, EPK_ENG_COUNT - 1, epk_engine(sel))],
                       juce::dontSendNotification);
    static int lastSel = -1;
    if (sel != lastSel) { lastSel = sel; repaintStrips = true; }

    for (auto* list : { &levels, &sound, &fx })
        for (auto* k : *list)
            if (!k->slider.isMouseButtonDown() && k->get) k->slider.setValue(k->get(), juce::dontSendNotification);
    if (repaintStrips) repaint();
}
