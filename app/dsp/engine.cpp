// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Javier Sánchez
// EPICYKLOS DSP core, phase 1 (núcleo DSP, fase 1): clock, euclidean sequencer, a test voice per track, mixer.
// Rules (reglas): C++14 subset, no heap, no exceptions/RTTI/STL, no global constructors (every object is plain data
// set up by init()), float only, no 64-bit multiplies in the audio path. The test voice (voz de prueba) is a
// placeholder until the real engines (fases 2-5).
#include "epk.h"
#include "fastmath.h"

namespace epk {

constexpr float kTwoPi = 6.2831853f;

// ------------------------------------------------------------------ helpers (utilidades)
inline float clamp01(float x) { return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x; }
inline int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

struct Rng {                               // xorshift32 (ruido)
    uint32_t s;
    void init(uint32_t seed) { s = seed ? seed : 0x9E3779B9u; }
    float bipolar() {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float)(int32_t)s * (1.0f / 2147483648.0f);
    }
};

// Euclidean pattern (patrón euclidiano): hits spread over length steps, rotated.
inline uint32_t euclid(int length, int hits, int rotate) {
    uint32_t m = 0;
    if (hits <= 0) return 0;
    if (hits >= length) return length >= 32 ? 0xFFFFFFFFu : (1u << length) - 1u;
    for (int i = 0; i < length; i++)
        if ((i * hits) % length < hits) m |= 1u << i;
    rotate = ((rotate % length) + length) % length;
    if (rotate) {
        uint32_t r = 0;
        for (int i = 0; i < length; i++)
            if (m >> i & 1u) r |= 1u << ((i + rotate) % length);
        m = r;
    }
    return m;
}

// ------------------------------------------------------------------ test voice (voz de prueba)
// One oscillator morphing sine (with a pitch drop: kick-like) -> saw -> noise, a low-pass, an exponential decay.
struct TestVoice {
    float phase, freq, pitch_env, amp, attack;
    float ic1, ic2;                        // TPT state-variable filter (filtro SVF) state
    Rng rng;

    void init(uint32_t seed) {
        phase = 0.0f; freq = 110.0f; pitch_env = 0.0f; amp = 0.0f; attack = 1.0f;
        ic1 = ic2 = 0.0f;
        rng.init(seed);
    }
    void trigger(float hz, float velocity) {
        freq = hz;
        pitch_env = 1.0f;
        amp = velocity;
        attack = 0.0f;
    }
    // character, tone (corte), decay as 0..1; sr the sample rate
    void render(float *out, uint32_t n, float character, float tone, float decay, float sr) {
        const float cutoff = 40.0f * fm_exp2f(tone * 8.6f);                       // 40 Hz .. ~16 kHz
        const float g = fm_tanf(3.14159265f * fm_minf(cutoff, sr * 0.45f) / sr);
        const float k = 1.2f;                                                     // resonance (resonancia), mild
        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float decay_s = 0.01f * fm_exp2f(decay * 7.6f);                     // 10 ms .. ~2 s
        const float amp_mul = fm_expf(-6.9f / (decay_s * sr));                    // -60 dB over decay_s
        const float pitch_mul = fm_expf(-1.0f / (0.035f * sr));                   // the drop: ~35 ms
        const float drop = clamp01(1.0f - character * 3.0f) * 3.0f;               // octaves of drop (caída de tono)
        const float to_saw = clamp01(character * 2.0f);
        const float to_noise = clamp01(character * 2.0f - 1.0f);
        for (uint32_t i = 0; i < n; i++) {
            const float f = freq * fm_exp2f(drop * pitch_env);
            const float inc = f / sr;
            phase += inc;
            if (phase >= 1.0f) phase -= 1.0f;
            float saw = 2.0f * phase - 1.0f;                                      // polyBLEP saw (sierra)
            if (phase < inc) { float t = phase / inc; saw -= t + t - t * t - 1.0f; }
            else if (phase > 1.0f - inc) { float t = (phase - 1.0f) / inc; saw -= t * t + t + t + 1.0f; }
            const float sine = fm_sin_turns(phase);
            float s = sine + (saw - sine) * to_saw;
            s += (rng.bipolar() - s) * to_noise;
            const float v3 = s - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = fm_flush(2.0f * v1 - ic1);
            ic2 = fm_flush(2.0f * v2 - ic2);
            attack = fm_minf(attack + 0.02f, 1.0f);                               // ~1 ms: no click (sin clic)
            out[i] = v2 * amp * attack;
            amp = fm_flush(amp * amp_mul);
            pitch_env *= pitch_mul;
        }
    }
};

// ------------------------------------------------------------------ track (pista)
struct Track {
    float p[EPK_P_COUNT];                  // parameters (parámetros), 0..1
    int muted, soloed;
    uint32_t mask;                         // steps that trigger (pasos activos)
    int length, step;
    uint32_t pos;                          // position within the step, Q32: 2^32 = one step (exacto, sin deriva)
    int fire;                              // play pressed: step 0 at the next sample
    float gain;                            // after mute/solo, smoothed (suavizado)
    float meter;
    uint32_t triggers;
    int key_note;                          // last key played (-1 none)
    TestVoice voice;
};

struct Event { uint8_t type, track, note, vel; };
enum { EV_NOTE_ON = 1, EV_PLAY, EV_STOP };

constexpr int kQueue = 64;                 // a power of two
constexpr uint32_t kBlock = 256;           // the largest block the platform renders (APP_BLOCK)

struct Engine {
    float sr, bpm;
    int playing;
    Track tr[EPK_TRACKS];
    Event queue[kQueue];
    volatile uint32_t q_write, q_read;
    float meter_master;
    float buf[kBlock];
};

static Engine g;                           // zero-initialised (.bss): no constructor runs at boot

static const float kStepsPerBeat[EPK_DIV_COUNT] = {8.0f, 6.0f, 4.0f, 3.0f, 2.0f, 1.0f};

inline int param_int(const Track &t, int param) {
    switch (param) {
    case EPK_P_LENGTH: return 1 + (int)(t.p[param] * 31.0f + 0.5f);
    case EPK_P_HITS: { int len = 1 + (int)(t.p[EPK_P_LENGTH] * 31.0f + 0.5f); return (int)(t.p[param] * len + 0.5f); }
    case EPK_P_ROTATE: return (int)(t.p[param] * 31.0f + 0.5f);
    case EPK_P_DIVISION: return (int)(t.p[param] * (EPK_DIV_COUNT - 1) + 0.5f);
    case EPK_P_NOTE: return 24 + (int)(t.p[param] * 60.0f + 0.5f);
    default: return 0;
    }
}

inline float note_hz(int note) { return 440.0f * fm_exp2f((float)(note - 69) * (1.0f / 12.0f)); }

void push(uint8_t type, int track, int note, float vel) {
    uint32_t w = g.q_write;
    if (w - g.q_read >= (uint32_t)kQueue) return;                  // full: dropped (lleno)
    Event &e = g.queue[w & (kQueue - 1)];
    e.type = type; e.track = (uint8_t)track; e.note = (uint8_t)clampi(note, 0, 127);
    e.vel = (uint8_t)clampi((int)(vel * 127.0f), 1, 127);
    g.q_write = w + 1;
}

void drain() {
    while (g.q_read != g.q_write) {
        const Event &e = g.queue[g.q_read & (kQueue - 1)];
        if (e.type == EV_NOTE_ON && e.track < EPK_TRACKS) {
            Track &t = g.tr[e.track];
            t.voice.trigger(note_hz(e.note), (float)e.vel / 127.0f);
            t.triggers++;
        } else if (e.type == EV_PLAY) {
            g.playing = 1;
            for (Track &t : g.tr) { t.step = -1; t.pos = 0; t.fire = 1; }   // the first sample plays step 0
        } else if (e.type == EV_STOP) {
            g.playing = 0;
            for (Track &t : g.tr) t.step = -1;
        }
        g.q_read++;
    }
}

int any_solo() {
    for (const Track &t : g.tr) if (t.soloed) return 1;
    return 0;
}

// The next step: trigger if the pattern says so (siguiente paso)
void advance(Track &t, int len) {
    t.step = (t.step + 1) % len;
    if (t.mask >> t.step & 1u) {
        const int note = t.key_note >= 0 ? t.key_note : param_int(t, EPK_P_NOTE);
        t.voice.trigger(note_hz(note), t.step == 0 ? 1.0f : 0.8f);   // the first step accented (acento)
        t.triggers++;
    }
}

// The sequencer for one track over a block: the voice is rendered in pieces, each trigger at its own sample.
void render_track(Track &t, float *l, float *r, uint32_t n, int solo) {
    const int len = param_int(t, EPK_P_LENGTH);
    t.length = len;
    t.mask = euclid(len, param_int(t, EPK_P_HITS), param_int(t, EPK_P_ROTATE));
    const float steps_per_sample = g.bpm * (1.0f / 60.0f) * kStepsPerBeat[param_int(t, EPK_P_DIVISION)] / g.sr;
    const uint32_t inc = (uint32_t)(steps_per_sample * 4294967296.0f);   // Q32 per sample (por muestra)
    const float character = t.p[EPK_P_CHARACTER], tone = t.p[EPK_P_TONE], decay = t.p[EPK_P_DECAY];
    uint32_t done = 0;
    while (done < n) {
        if (g.playing && t.fire) {                                   // play: step 0 now (paso 0 ya)
            t.fire = 0;
            t.step = -1;
            advance(t, len);
        }
        uint32_t run = n - done, until = 0xFFFFFFFFu;
        if (g.playing) {                                             // samples until the next step (hasta el paso)
            const uint32_t remain = t.pos ? 0u - t.pos : 0xFFFFFFFFu;
            until = remain / inc + (remain % inc ? 1u : 0u);
            if (until < run) run = until;
        }
        t.voice.render(g.buf + done, run, character, tone, decay, g.sr);
        done += run;
        if (g.playing) {
            t.pos += run * inc;                                      // wraps past 2^32 at the step's end
            if (run == until) advance(t, len);
        }
    }
    const float target = (t.muted || (solo && !t.soloed)) ? 0.0f : 1.0f;
    const float lvl = t.p[EPK_P_LEVEL] * t.p[EPK_P_LEVEL];          // a squared taper (curva cuadrática)
    const float pan = t.p[EPK_P_PAN];
    const float gl = fm_cosf(pan * 1.5707963f), gr = fm_sinf(pan * 1.5707963f);   // equal power (potencia constante)
    float peak = t.meter;
    for (uint32_t i = 0; i < n; i++) {
        t.gain += (target - t.gain) * 0.003f;                        // ~8 ms: mutes do not click
        const float s = g.buf[i] * lvl * t.gain;
        l[i] += s * gl;
        r[i] += s * gr;
        peak = fm_maxf(peak, fm_fabsf(s));
    }
    t.meter = peak;
}

}  // namespace epk

using namespace epk;

// ------------------------------------------------------------------ the C interface (interfaz C)
extern "C" {

void epk_init(float sample_rate) {
    // the default song (canción por defecto): a groove to hear the polymetry (polimetría) on the orbit view
    static const float kDefaults[EPK_TRACKS][EPK_P_COUNT] = {
        //  note   char   tone   decay  len    hits   rot    div    level  pan
        {0.20f, 0.00f, 0.55f, 0.42f, 15/31.f, 0.25f, 0.00f, 0.40f, 0.90f, 0.50f},   // 1: kick-like (bombo)
        {0.80f, 1.00f, 0.95f, 0.10f, 15/31.f, 0.50f, 2/31.f, 0.40f, 0.55f, 0.58f},   // 2: hats (ruido)
        {0.45f, 0.45f, 0.45f, 0.50f, 11/31.f, 0.25f, 3/31.f, 0.40f, 0.60f, 0.40f},   // 3: stab, 12 steps (acorde)
        {0.35f, 0.60f, 0.35f, 0.35f, 12/31.f, 0.40f, 1/31.f, 0.40f, 0.50f, 0.65f},   // 4: 13 steps (bajo)
        {0.30f, 0.50f, 0.25f, 0.95f, 6/31.f,  0.30f, 0.00f, 0.80f, 0.45f, 0.35f},    // 5: 7 steps of 1/8 (drone)
        {0.65f, 0.30f, 0.60f, 0.30f, 9/31.f,  0.30f, 4/31.f, 0.40f, 0.40f, 0.50f},   // 6: 10 steps
    };
    g.sr = sample_rate;
    g.bpm = 124.0f;
    g.playing = 0;
    g.q_write = g.q_read = 0;
    for (int i = 0; i < EPK_TRACKS; i++) {
        Track &t = g.tr[i];
        for (int p = 0; p < EPK_P_COUNT; p++) t.p[p] = kDefaults[i][p];
        t.muted = t.soloed = 0;
        t.step = -1; t.pos = 0; t.fire = 0;
        t.gain = 1.0f; t.meter = 0.0f; t.triggers = 0; t.key_note = -1;
        t.length = param_int(t, EPK_P_LENGTH);
        t.mask = euclid(t.length, param_int(t, EPK_P_HITS), param_int(t, EPK_P_ROTATE));
        t.voice.init(0x1234567u * (uint32_t)(i + 1));
    }
    g.tr[5].muted = 1;                                                // track 6 starts muted (silenciada)
}

void epk_render(float *left, float *right, uint32_t frames) {
    drain();
    const int solo = any_solo();
    for (uint32_t i = 0; i < frames; i++) left[i] = right[i] = 0.0f;
    for (uint32_t done = 0; done < frames; done += kBlock) {
        const uint32_t n = frames - done < kBlock ? frames - done : kBlock;
        for (Track &t : g.tr) render_track(t, left + done, right + done, n, solo);
    }
    float peak = g.meter_master;
    for (uint32_t i = 0; i < frames; i++) {                           // the master (salida): a soft clip
        left[i] = fm_tanhf(left[i] * 0.9f);
        right[i] = fm_tanhf(right[i] * 0.9f);
        peak = fm_maxf(peak, fm_maxf(fm_fabsf(left[i]), fm_fabsf(right[i])));
    }
    g.meter_master = peak;
}

void epk_panic(void) {
    for (Track &t : g.tr) t.voice.init(t.voice.rng.s);
}

void epk_play(int on) { push(on ? EV_PLAY : EV_STOP, 0, 0, 0.0f); }
int epk_playing(void) { return g.playing; }
void epk_set_tempo(float bpm) { g.bpm = bpm < 40.0f ? 40.0f : bpm > 200.0f ? 200.0f : bpm; }
float epk_tempo(void) { return g.bpm; }

void epk_param_set(int track, int param, float value01) {
    if (track < 0 || track >= EPK_TRACKS || param < 0 || param >= EPK_P_COUNT) return;
    g.tr[track].p[param] = clamp01(value01);
}
float epk_param_get(int track, int param) {
    if (track < 0 || track >= EPK_TRACKS || param < 0 || param >= EPK_P_COUNT) return 0.0f;
    return g.tr[track].p[param];
}
int epk_param_steps(int param) {
    switch (param) {
    case EPK_P_NOTE: return 61;
    case EPK_P_LENGTH: case EPK_P_ROTATE: return 32;
    case EPK_P_HITS: return 33;
    case EPK_P_DIVISION: return EPK_DIV_COUNT;
    default: return 0;
    }
}

void epk_mute(int track, int on) { if (track >= 0 && track < EPK_TRACKS) g.tr[track].muted = !!on; }
int epk_muted(int track) { return track >= 0 && track < EPK_TRACKS ? g.tr[track].muted : 0; }
void epk_solo(int track, int on) { if (track >= 0 && track < EPK_TRACKS) g.tr[track].soloed = !!on; }
int epk_soloed(int track) { return track >= 0 && track < EPK_TRACKS ? g.tr[track].soloed : 0; }
int epk_audible(int track) {
    if (track < 0 || track >= EPK_TRACKS) return 0;
    const Track &t = g.tr[track];
    return !t.muted && (!any_solo() || t.soloed);
}

void epk_note_on(int track, int note, float velocity) {
    if (track < 0 || track >= EPK_TRACKS) return;
    g.tr[track].key_note = note;
    push(EV_NOTE_ON, track, note, velocity);
}
void epk_note_off(int track, int note) {
    if (track >= 0 && track < EPK_TRACKS && g.tr[track].key_note == note) g.tr[track].key_note = -1;
}

int epk_step(int track) { return track >= 0 && track < EPK_TRACKS && g.playing ? g.tr[track].step : -1; }
int epk_length(int track) { return track >= 0 && track < EPK_TRACKS ? param_int(g.tr[track], EPK_P_LENGTH) : 0; }
uint32_t epk_pattern(int track) {
    if (track < 0 || track >= EPK_TRACKS) return 0;
    const Track &t = g.tr[track];
    return euclid(param_int(t, EPK_P_LENGTH), param_int(t, EPK_P_HITS), param_int(t, EPK_P_ROTATE));
}
float epk_meter(int track) {
    if (track < 0 || track >= EPK_TRACKS) return 0.0f;
    float m = g.tr[track].meter;
    g.tr[track].meter = 0.0f;
    return m;
}
float epk_meter_master(void) { float m = g.meter_master; g.meter_master = 0.0f; return m; }
uint32_t epk_triggers(int track) { return track >= 0 && track < EPK_TRACKS ? g.tr[track].triggers : 0; }

}  // extern "C"
