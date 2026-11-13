// SPDX-License-Identifier: GPL-3.0-only
#include "Processor.h"
#include "Editor.h"

std::atomic<int> EpkProcessor::instances { 0 };

// The main stereo output; a stereo pair for each track with its pitch (left, 1 V/octave) and its gate (right), to
// send to a DC-coupled converter; and a mono input for each track, to bring an outside sound through it. The
// inputs start off inside a DAW (it turns on the ones it routes to) and on in the app.
juce::AudioProcessor::BusesProperties EpkProcessor::makeBuses()
{
    const bool app = juce::PluginHostType::getPluginLoadedAs() == wrapperType_Standalone;
    auto b = BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true);
    for (int t = 0; t < EPK_TRACKS; t++) b = b.withOutput("CV " + juce::String(t + 1), juce::AudioChannelSet::stereo(), true);
    for (int t = 0; t < EPK_TRACKS; t++) b = b.withInput("IN " + juce::String(t + 1), juce::AudioChannelSet::mono(), app);
    return b;
}

EpkProcessor::EpkProcessor() : AudioProcessor(makeBuses())
{
    for (auto& m : midiOut) m.store(false);
    owner = instances.fetch_add(1) == 0;
    if (owner)
        epk_init(44100.0f);                // the core is valid from the start (válido desde el principio)
}

EpkProcessor::~EpkProcessor() { instances.fetch_sub(1); }

bool EpkProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    for (int i = 1; i < layouts.outputBuses.size(); i++)
        if (!layouts.outputBuses[i].isDisabled() && layouts.outputBuses[i] != juce::AudioChannelSet::stereo()) return false;
    for (const auto& in : layouts.inputBuses)
        if (!in.isDisabled() && in != juce::AudioChannelSet::mono()) return false;
    return true;
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

void EpkProcessor::prepareToPlay(double sampleRate, int blockSize)
{
    outRate = sampleRate;
    for (auto& e : ext) e.assign((size_t) juce::jmax(8192, blockSize), 0.0f);
    nFired = nCarry = 0;
    for (auto& o : out) o = TrackOut();
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

void EpkProcessor::noteAt(int frame, const EpkNoteEvent& e, int n)
{
    if (frame >= n) {                      // it sounds on the next block's first sample (primera muestra del siguiente)
        if (nCarry < 64) carry[nCarry++] = { 0, e };
    } else if (nFired < 512)
        fired[nFired++] = { frame, e };
}

void EpkProcessor::fillCore(int frames, int outIndex, int n)
{
    coreN = juce::jlimit(1, 256, frames);
    corePos = 0;
    for (int t = 0; t < EPK_TRACKS; t++) {
        if (haveExt[t])
            for (int j = 0; j < coreN; j++) extCore[t][j] = ext[t][(size_t) juce::jmin(n - 1, outIndex + 2 * j)];
        epk_ext_in(t, haveExt[t] ? extCore[t] : nullptr);
    }
    epk_render(coreL, coreR, (uint32_t) coreN);
    EpkNoteEvent ev[64];
    const int got = epk_events_take(ev, 64);
    for (int k = 0; k < got; k++) noteAt(outIndex + 2 * ev[k].frame, ev[k], n);
}

void EpkProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    // what comes in for each track, before the buffer is reused for the outputs (lo que entra, antes de pisarlo)
    for (int t = 0; t < EPK_TRACKS; t++) {
        haveExt[t] = false;
        if (!owner || t >= getBusCount(true) || (size_t) n > ext[t].size()) continue;
        const auto* bus = getBus(true, t);
        if (bus == nullptr || !bus->isEnabled()) continue;
        const auto in = getBusBuffer(buffer, true, t);
        if (in.getNumChannels() < 1) continue;
        juce::FloatVectorOperations::copy(ext[t].data(), in.getReadPointer(0), n);
        haveExt[t] = true;
    }
    buffer.clear();
    if (!owner || buffer.getNumChannels() < 2 || n <= 0) { midi.clear(); return; }

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

    // notes in: channels 1-6 play tracks 1-6, any other the selected track (notas que entran)
    for (const auto meta : midi) {
        const auto m = meta.getMessage();
        const int ch = m.getChannel() - 1;
        const int track = ch >= 0 && ch < EPK_TRACKS ? ch : selectedTrack.load();
        if (m.isNoteOn()) epk_note_on(track, m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff()) epk_note_off(track, m.getNoteNumber());
    }
    midi.clear();

    nFired = 0;
    for (int k = 0; k < nCarry; k++) fired[nFired++] = carry[k];
    nCarry = 0;

    auto main = getBusBuffer(buffer, false, 0);
    float* outL = main.getWritePointer(0);
    float* outR = main.getWritePointer(1);
    if (!half) {
        int done = 0;
        while (done < n) {
            const int chunk = juce::jmin(256, n - done);
            for (int t = 0; t < EPK_TRACKS; t++) epk_ext_in(t, haveExt[t] ? ext[t].data() + done : nullptr);
            epk_render(outL + done, outR + done, (uint32_t) chunk);
            EpkNoteEvent ev[64];
            const int got = epk_events_take(ev, 64);
            for (int k = 0; k < got; k++) noteAt(done + ev[k].frame, ev[k], n);
            done += chunk;
        }
    } else {
        for (int i = 0; i < n; i++) {
            if (phase == 0) {              // a new core sample every other one; between them, the mean (la media)
                if (corePos >= coreN) fillCore((n - i + 1) / 2, i, n);
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
    for (int t = 0; t < EPK_TRACKS; t++) epk_ext_in(t, nullptr);

    // each track's notes out: MIDI on its channel, and pitch and gate as voltages on its pair (notas que salen)
    const int gateLen = juce::jmax(64, (int) (0.5 * outRate * 60.0 / (juce::jmax(40.0f, epk_tempo()) * 4.0)));
    const float perSemitone = 1.0f / (12.0f * juce::jmax(1.0f, cvVolts.load()));
    const float gateLevel = juce::jmin(1.0f, 5.0f / juce::jmax(1.0f, cvVolts.load()));   // 5 V
    for (int t = 0; t < EPK_TRACKS; t++) {
        auto& o = out[t];
        const bool toMidi = midiOut[t].load();
        float* pitch = nullptr;
        float* gate = nullptr;
        if (t + 1 < getBusCount(false) && getBus(false, t + 1)->isEnabled()) {
            auto cv = getBusBuffer(buffer, false, t + 1);
            if (cv.getNumChannels() >= 2) { pitch = cv.getWritePointer(0); gate = cv.getWritePointer(1); }
        }
        int k = 0;
        for (int i = 0; i < n; i++) {
            for (; k < nFired && fired[k].frame <= i; k++) {
                if (fired[k].e.track != t || fired[k].frame != i) continue;
                o.note = fired[k].e.note;
                o.gate = gateLen;
                if (toMidi) {
                    if (o.midiNote >= 0) midi.addEvent(juce::MidiMessage::noteOff(t + 1, o.midiNote), i);
                    midi.addEvent(juce::MidiMessage::noteOn(t + 1, o.note, (juce::uint8) fired[k].e.velocity), i);
                    o.midiNote = o.note;
                    o.midiLeft = gateLen;
                }
            }
            if (pitch != nullptr) {
                pitch[i] = (float) (o.note - 60) * perSemitone;      // C4 = 0 V
                gate[i] = o.gate > 0 ? gateLevel : 0.0f;
            }
            if (o.gate > 0) o.gate--;
            if (o.midiNote >= 0 && (--o.midiLeft <= 0 || !toMidi)) {
                midi.addEvent(juce::MidiMessage::noteOff(t + 1, o.midiNote), i);
                o.midiNote = -1;
            }
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
    out.writeInt(2);
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
    out.writeInt(EPK_TRACKS);              // version 2: the outputs (las salidas)
    for (int t = 0; t < EPK_TRACKS; t++) {
        out.writeBool(owner && epk_voice_is_off(t) != 0);
        out.writeBool(midiOut[t].load());
    }
    out.writeFloat(cvVolts.load());
}

void EpkProcessor::setStateInformation(const void* data, int size)
{
    if (!owner) return;
    juce::MemoryInputStream in(data, (size_t) size, false);
    if (in.readInt() != 0x4550444B) return;
    const int version = in.readInt();
    if (version != 1 && version != 2) return;
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
    if (version >= 2) {
        const int no = juce::jlimit(0, 64, in.readInt());
        for (int t = 0; t < no; t++) {
            const bool off = in.readBool(), toMidi = in.readBool();
            if (t < EPK_TRACKS) { epk_voice_off(t, off ? 1 : 0); midiOut[t].store(toMidi); }
        }
        cvVolts.store(juce::jlimit(1.0f, 20.0f, in.readFloat()));
    }
    const juce::SpinLock::ScopedLockType lock(pendingLock);
    pending = s;
    stage.store(1);
}

juce::AudioProcessorEditor* EpkProcessor::createEditor() { return new EpkEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EpkProcessor(); }
