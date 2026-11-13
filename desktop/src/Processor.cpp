// SPDX-License-Identifier: GPL-3.0-only
#include "Processor.h"
#include "Editor.h"

std::atomic<int> EpkProcessor::instances { 0 };

EpkProcessor::EpkProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    owner = instances.fetch_add(1) == 0;
    if (owner)
        epk_init(44100.0f);                // the core is valid from the start (válido desde el principio)
}

EpkProcessor::~EpkProcessor() { instances.fetch_sub(1); }

bool EpkProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void EpkProcessor::capture(EpkSnapshot& s) const
{
    s.valid = true;
    s.bank = epk_bank_current();
    s.pattern = epk_pattern_current();
    s.tempo = epk_tempo();
    for (int t = 0; t < EPK_TRACKS; t++) {
        s.mute[t] = epk_muted(t) != 0;
        for (int q = 0; q < EPK_P_COUNT; q++) s.p[t][q] = epk_param_get(t, q);
    }
    for (int i = 0; i < EPK_FX_COUNT; i++) s.fx[i] = epk_fx_get(i);
    for (int m = 0; m < EPK_MOD_SOURCES; m++)
        for (int i = 0; i < EPK_M_COUNT; i++) s.mod[m][i] = epk_mod_get(m, i);
}

void EpkProcessor::prepareToPlay(double sampleRate, int)
{
    half = sampleRate > 50000.0;
    const double rate = half ? sampleRate * 0.5 : sampleRate;
    if (!owner) return;
    if (std::abs(rate - coreRate) > 1.0) {  // a new rate: start the core again and put everything back
        EpkSnapshot keep;
        capture(keep);
        epk_init((float) rate);
        coreRate = rate;
        const juce::SpinLock::ScopedLockType lock(pendingLock);
        if (stage.load() == 0) {
            pending = keep;
            stage.store(1);
        }
    }
    coreN = corePos = 0;
    phase = 0;
    prevL = prevR = curL = curR = 0.0f;
    hostPlaying = false;
}

void EpkProcessor::fillCore(int frames)
{
    coreN = juce::jlimit(1, 256, frames);
    corePos = 0;
    epk_render(coreL, coreR, (uint32_t) coreN);
}

void EpkProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (!owner || buffer.getNumChannels() < 2) return;

    // a saved state comes back in two blocks: the pattern first, then what was changed in it (en dos bloques)
    if (stage.load() != 0) {
        const juce::SpinLock::ScopedTryLockType lock(pendingLock);
        if (lock.isLocked() && pending.valid) {
            if (stage.load() == 1) {
                epk_play(0);
                hostPlaying = false;
                epk_pattern_select(pending.bank, pending.pattern);
                stage.store(2);
            } else {
                epk_set_tempo(pending.tempo);
                for (int t = 0; t < EPK_TRACKS; t++) {
                    epk_mute(t, pending.mute[t] ? 1 : 0);
                    for (int q = 0; q < EPK_P_COUNT; q++) epk_param_set(t, q, pending.p[t][q]);
                }
                for (int i = 0; i < EPK_FX_COUNT; i++) epk_fx_set(i, pending.fx[i]);
                for (int m = 0; m < EPK_MOD_SOURCES; m++)
                    for (int i = 0; i < EPK_M_COUNT; i++) epk_mod_set(m, i, pending.mod[m][i]);
                stage.store(0);
            }
        } else if (lock.isLocked())
            stage.store(0);
    }

    // the DAW's tempo and transport (el tempo y el transporte del DAW)
    bool hasHost = false;
    if (!isStandalone())
        if (auto* head = getPlayHead())
            if (auto pos = head->getPosition()) {
                hasHost = true;
                if (auto bpm = pos->getBpm()) epk_set_tempo((float) *bpm);
                const bool playing = pos->getIsPlaying();
                if (playing != hostPlaying && stage.load() == 0) {
                    hostPlaying = playing;
                    epk_play(playing ? 1 : 0);
                }
            }
    hostHasTransport.store(hasHost);

    // notes: channels 1-6 play tracks 1-6, any other the selected track (notas)
    for (const auto meta : midi) {
        const auto m = meta.getMessage();
        const int ch = m.getChannel() - 1;
        const int track = ch >= 0 && ch < EPK_TRACKS ? ch : selectedTrack.load();
        if (m.isNoteOn()) epk_note_on(track, m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff()) epk_note_off(track, m.getNoteNumber());
    }
    midi.clear();

    float* outL = buffer.getWritePointer(0);
    float* outR = buffer.getWritePointer(1);
    if (!half) {
        int done = 0;
        while (done < n) {
            const int chunk = juce::jmin(256, n - done);
            epk_render(outL + done, outR + done, (uint32_t) chunk);
            done += chunk;
        }
    } else {
        for (int i = 0; i < n; i++) {
            if (phase == 0) {              // a new core sample every other one; between them, the mean (la media)
                if (corePos >= coreN) fillCore((n - i + 1) / 2);
                prevL = curL; prevR = curR;
                curL = coreL[corePos]; curR = coreR[corePos];
                corePos++;
                outL[i] = 0.5f * (prevL + curL);
                outR[i] = 0.5f * (prevR + curR);
            } else {
                outL[i] = curL;
                outR[i] = curR;
            }
            phase ^= 1;
        }
    }
}

void EpkProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    EpkSnapshot s;
    {
        const juce::SpinLock::ScopedLockType lock(pendingLock);
        if (stage.load() != 0 && pending.valid) s = pending;   // not applied yet: keep what was loaded
    }
    if (!s.valid) {
        if (!owner) return;
        capture(s);
    }
    juce::MemoryOutputStream out(dest, false);
    out.writeInt(0x4550444B);              // "EPDK"
    out.writeInt(1);
    out.writeInt(s.bank);
    out.writeInt(s.pattern);
    out.writeFloat(s.tempo);
    out.writeInt(EPK_TRACKS);
    out.writeInt(EPK_P_COUNT);
    out.writeInt(EPK_FX_COUNT);
    out.writeInt(EPK_MOD_SOURCES);
    out.writeInt(EPK_M_COUNT);
    for (int t = 0; t < EPK_TRACKS; t++) {
        out.writeBool(s.mute[t]);
        for (int q = 0; q < EPK_P_COUNT; q++) out.writeFloat(s.p[t][q]);
    }
    for (int i = 0; i < EPK_FX_COUNT; i++) out.writeFloat(s.fx[i]);
    for (int m = 0; m < EPK_MOD_SOURCES; m++)
        for (int i = 0; i < EPK_M_COUNT; i++) out.writeFloat(s.mod[m][i]);
}

void EpkProcessor::setStateInformation(const void* data, int size)
{
    if (!owner) return;
    juce::MemoryInputStream in(data, (size_t) size, false);
    if (in.readInt() != 0x4550444B || in.readInt() != 1) return;
    EpkSnapshot s;
    capture(s);                            // what this version has and the saved one lacks keeps its value
    s.bank = juce::jlimit(0, EPK_BANKS - 1, in.readInt());
    s.pattern = juce::jlimit(0, EPK_PATTERNS - 1, in.readInt());
    s.tempo = in.readFloat();
    const int nt = in.readInt(), np = in.readInt(), nf = in.readInt(), nm = in.readInt(), nmp = in.readInt();
    if (nt < 0 || nt > 64 || np < 0 || np > 256 || nf < 0 || nf > 1024 || nm < 0 || nm > 64 || nmp < 0 || nmp > 64) return;
    for (int t = 0; t < nt; t++) {
        const bool mute = in.readBool();
        if (t < EPK_TRACKS) s.mute[t] = mute;
        for (int q = 0; q < np; q++) {
            const float v = in.readFloat();
            if (t < EPK_TRACKS && q < EPK_P_COUNT) s.p[t][q] = v;
        }
    }
    for (int i = 0; i < nf; i++) {
        const float v = in.readFloat();
        if (i < EPK_FX_COUNT) s.fx[i] = v;
    }
    for (int m = 0; m < nm; m++)
        for (int i = 0; i < nmp; i++) {
            const float v = in.readFloat();
            if (m < EPK_MOD_SOURCES && i < EPK_M_COUNT) s.mod[m][i] = v;
        }
    const juce::SpinLock::ScopedLockType lock(pendingLock);
    pending = s;
    stage.store(1);
}

juce::AudioProcessorEditor* EpkProcessor::createEditor() { return new EpkEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EpkProcessor(); }
