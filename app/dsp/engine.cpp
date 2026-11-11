// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Javier Sánchez
// EPICYKLOS DSP core (núcleo DSP): clock, sequencer (euclidean or manual steps, with chance and conditions),
// patterns, a voice per track (voices.h), mixer.
// Rules (reglas): C++14 subset, no heap, no exceptions/RTTI/STL, no global constructors (every object is plain data
// set up by init()), float only, no 64-bit multiplies in the audio path.
#include "epk.h"
#include "fastmath.h"
#include "factory.h"
#include "voices.h"

namespace epk {

// Euclidean hits (golpes euclidianos) spread over length steps, not rotated.
inline uint32_t euclid(int length, int hits) {
    uint32_t m = 0;
    if (hits <= 0) return 0;
    if (hits >= length) return length >= 32 ? 0xFFFFFFFFu : (1u << length) - 1u;
    for (int i = 0; i < length; i++)
        if ((i * hits) % length < hits) m |= 1u << i;
    return m;
}
inline uint32_t rotate_mask(uint32_t m, int length, int rotate) {
    if (length < 32) m &= (1u << length) - 1u;
    rotate = ((rotate % length) + length) % length;
    if (!rotate) return m;
    uint32_t r = 0;
    for (int i = 0; i < length; i++)
        if (m >> i & 1u) r |= 1u << ((i + rotate) % length);
    return r;
}

// A track's steps (los pasos de una pista): by hand or not, and each step's chance and condition.
struct Steps {
    uint32_t mask;                         // the manual hits, before rotation (golpes a mano)
    uint8_t manual;
    uint8_t meta[EPK_MAX_STEPS];           // bits 0-1 EPK_PROB_*, bits 2-4 EPK_COND_*; indexed before rotation
};

// ------------------------------------------------------------------ track (pista)
struct Track {
    float p[EPK_P_COUNT];                  // parameters (parámetros), 0..1
    Steps st;
    int muted, soloed;
    int step;
    uint32_t cycle;                        // times the pattern has looped (vueltas)
    uint32_t pos;                          // position within the step, Q32: 2^32 = one step (exacto, sin deriva)
    int fire;                              // play pressed: step 0 at the next sample
    float gain;                            // after mute/solo, smoothed (suavizado)
    float meter;
    uint32_t triggers;
    int key_note;                          // last key played (-1 none)
    Rng chance;
    Voice voice;
};

struct Event { uint8_t type, track, a, b; };
enum { EV_NOTE_ON = 1, EV_PLAY, EV_STOP, EV_PATTERN, EV_STEP_TOGGLE, EV_STEP_META, EV_EUCLID, EV_UNDO, EV_RANDOM };

constexpr int kQueue = 64;                 // a power of two
constexpr uint32_t kBlock = 256;           // the largest block the platform renders (APP_BLOCK)

struct Undo {
    int ready, pattern, bank;
    float p[EPK_TRACKS][EPK_P_PATTERN_COUNT], pan[EPK_TRACKS];
    Steps st[EPK_TRACKS];
};

struct Engine {
    float sr, bpm;
    int playing;
    float bank[EPK_PATTERNS][EPK_TRACKS][EPK_P_PATTERN_COUNT];   // the patterns (patrones), 0..1 like Track::p
    Steps bank_steps[EPK_PATTERNS][EPK_TRACKS];
    int pattern, queued;                   // current, queued (-1 none)
    int bank_id, queued_bank;              // the bank in memory (banco en memoria), the queued pattern's
    uint32_t bar_pos;                      // the bar clock (reloj de compás): Q32 of a sixteenth
    int bar_step, bar_fire;
    Track tr[EPK_TRACKS];
    Undo undo;
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
    case EPK_P_ENGINE: return (int)(t.p[param] * (EPK_ENG_COUNT - 1) + 0.5f);
    default: return 0;
    }
}

inline float note_hz(int note) { return 440.0f * fm_exp2f((float)(note - 69) * (1.0f / 12.0f)); }

// The hits before rotation (golpes sin rotar), and as they play (como suenan)
inline uint32_t base_mask(const Track &t, int len) {
    return t.st.manual ? t.st.mask : euclid(len, param_int(t, EPK_P_HITS));
}
inline uint32_t play_mask(const Track &t) {
    const int len = param_int(t, EPK_P_LENGTH);
    return rotate_mask(base_mask(t, len), len, param_int(t, EPK_P_ROTATE));
}
// A step as it plays -> its index before rotation
inline int unrotated(const Track &t, int step, int len) {
    const int rot = param_int(t, EPK_P_ROTATE) % len;
    return ((step - rot) % len + len) % len;
}

void push(uint8_t type, int track, int a, int b) {
    uint32_t w = g.q_write;
    if (w - g.q_read >= (uint32_t)kQueue) return;                  // full: dropped (lleno)
    Event &e = g.queue[w & (kQueue - 1)];
    e.type = type; e.track = (uint8_t)track; e.a = (uint8_t)a; e.b = (uint8_t)b;
    g.q_write = w + 1;
}

// ------------------------------------------------------------------ patterns: factory and generated
// the engines by track (motores): kick, hats, and the test voice until the other engines exist
static const uint8_t kEngine[EPK_TRACKS] = {EPK_ENG_GRAVE, EPK_ENG_METAL, EPK_ENG_TEST, EPK_ENG_TEST,
                                            EPK_ENG_TEST, EPK_ENG_TEST};

void set_spec(int pt, int i, int note, float ch, float tone, float dec, float e1, float e2, int len, int hits,
              int rot, int div) {
    float *b = g.bank[pt][i];
    b[EPK_P_NOTE] = (float)(clampi(note, 24, 84) - 24) / 60.0f;
    b[EPK_P_CHARACTER] = ch;
    b[EPK_P_TONE] = tone;
    b[EPK_P_DECAY] = dec;
    b[EPK_P_ENGINE] = (float)kEngine[i] / (float)(EPK_ENG_COUNT - 1);
    b[EPK_P_EXTRA1] = e1;
    b[EPK_P_EXTRA2] = e2;
    b[EPK_P_LENGTH] = (float)(len - 1) / 31.0f;
    b[EPK_P_HITS] = (float)clampi(hits, 0, len) / (float)len;
    b[EPK_P_ROTATE] = (float)(rot % len) / 31.0f;
    b[EPK_P_DIVISION] = (float)div / (float)(EPK_DIV_COUNT - 1);
    Steps &s = g.bank_steps[pt][i];
    s.mask = 0; s.manual = 0;
    for (uint8_t &m : s.meta) m = 0;
}

void fill_factory(int pt) {
    for (int i = 0; i < EPK_TRACKS; i++) {
        const TrackSpec &k = kFactory[pt][i];
        const float e1 = kEngine[i] == EPK_ENG_GRAVE ? 0.45f : kEngine[i] == EPK_ENG_METAL ? 0.40f : 0.0f;
        float e2 = kEngine[i] == EPK_ENG_GRAVE ? 0.40f : kEngine[i] == EPK_ENG_METAL ? 0.30f : 0.0f;
        if (i == 0 && pt == 8) e2 = 0.75f;                           // ECLIPSE: more rumble (más rumble)
        if (i == 0 && pt == 13) e2 = 0.85f;                          // NADIR
        set_spec(pt, i, k.note, k.character, k.tone, k.decay, e1, e2, k.length, k.hits, k.rotate, k.division);
    }
}

inline float rr(Rng &r, float lo, float hi) { return lo + (hi - lo) * r.unipolar(); }
inline int ri(Rng &r, int n) { int v = (int)(r.unipolar() * (float)n); return v >= n ? n - 1 : v; }   // 0..n-1
// A note of D minor (re menor): root is a D, degrees up from it
inline int scale_note(Rng &r, int root, int degrees) {
    static const int8_t kScale[7] = {0, 2, 3, 5, 7, 8, 10};
    const int d = ri(r, degrees);
    return root + 12 * (d / 7) + kScale[d % 7];
}

// A whole pattern from a seed (un patrón entero a partir de una semilla): each track keeps its role, inside
// ranges that work: a kick that mostly lands on four, odd cycle lengths for the polymetry, notes in D minor.
void generate(int pt, uint32_t seed) {
    static const uint8_t kLen[12] = {7, 9, 10, 11, 12, 13, 15, 16, 16, 20, 24, 32};
    Rng r;
    r.init(seed * 2654435761u + 0x9E3779B9u);
    for (int i = 0; i < 4; i++) r.bipolar();
    {   // 1 GRAVE
        const float x = r.unipolar();
        const int hits = x < 0.12f ? 0 : x < 0.72f ? 4 : x < 0.86f ? 3 : 5;
        const float drive = rr(r, 0.10f, 0.70f), click = rr(r, 0.20f, 0.80f), dec = rr(r, 0.30f, 0.70f);
        const float sweep = rr(r, 0.30f, 0.60f), rumble = rr(r, 0.0f, 0.80f);
        set_spec(pt, 0, 29 + ri(r, 8), drive, click, dec, sweep, rumble, 16, hits, 0, EPK_DIV_1_16);
    }
    {   // 2 METAL
        static const uint8_t kHatLen[4] = {16, 16, 12, 8};
        const int len = kHatLen[ri(r, 4)], hits = 2 + ri(r, len - 1), rot = ri(r, len);
        const int div = r.unipolar() < 0.15f ? EPK_DIV_1_16T : EPK_DIV_1_16;
        const float metal = rr(r, 0.50f, 1.0f), tone = rr(r, 0.70f, 1.0f), dec = rr(r, 0.03f, 0.25f);
        const float ring = rr(r, 0.10f, 0.70f), vary = rr(r, 0.0f, 0.60f);
        set_spec(pt, 1, 80 + ri(r, 12), metal, tone, dec, ring, vary, len, hits, rot, div);
    }
    {   // 3 stab (acorde)
        const int len = kLen[ri(r, 12)], hits = 1 + ri(r, len * 3 / 10 + 1);
        const float ch = rr(r, 0.35f, 0.55f), tone = rr(r, 0.30f, 0.60f), dec = rr(r, 0.35f, 0.85f);
        set_spec(pt, 2, scale_note(r, 50, 10), ch, tone, dec, 0.0f, 0.0f, len, hits, ri(r, len), EPK_DIV_1_16);
    }
    {   // 4 bass (bajo)
        const int len = kLen[ri(r, 12)], hits = 2 + ri(r, len * 45 / 100 + 1);
        const float ch = rr(r, 0.50f, 0.65f), tone = rr(r, 0.25f, 0.50f), dec = rr(r, 0.25f, 0.40f);
        set_spec(pt, 3, scale_note(r, 38, 5), ch, tone, dec, 0.0f, 0.0f, len, hits, ri(r, len), EPK_DIV_1_16);
    }
    {   // 5 drone
        static const uint8_t kDroneLen[3] = {5, 7, 9};
        const int len = kDroneLen[ri(r, 3)], hits = 1 + ri(r, 3);
        const float tone = rr(r, 0.18f, 0.30f), dec = rr(r, 0.90f, 1.0f);
        const int div = r.unipolar() < 0.5f ? EPK_DIV_1_8 : EPK_DIV_1_4;
        set_spec(pt, 4, scale_note(r, 38, 8), 0.5f, tone, dec, 0.0f, 0.0f, len, hits, 0, div);
    }
    {   // 6 perc (percusión)
        const int len = kLen[ri(r, 12)], hits = 1 + ri(r, len * 4 / 10 + 1);
        const float ch = r.unipolar() < 0.3f ? rr(r, 0.85f, 1.0f) : rr(r, 0.05f, 0.35f);
        const float tone = rr(r, 0.45f, 0.85f), dec = rr(r, 0.06f, 0.35f);
        set_spec(pt, 5, scale_note(r, 62, 12), ch, tone, dec, 0.0f, 0.0f, len, hits, ri(r, len), EPK_DIV_1_16);
    }
    for (int i = 2; i < EPK_TRACKS; i++)                             // now and then a track rests (descansa)
        if (r.unipolar() < 0.10f) g.bank[pt][i][EPK_P_HITS] = 0.0f;
    for (int i = 1; i < EPK_TRACKS; i++)                             // some steps by chance (pasos con azar)
        for (uint8_t &m : g.bank_steps[pt][i].meta)
            if (r.unipolar() < 0.12f) m = r.unipolar() < 0.5f ? EPK_PROB_50 : EPK_PROB_75;
}

// Bank A is the first project; B..K are generated, the same every time (banco A de fábrica; B..K generados)
void load_bank(int bank) {
    for (int pt = 0; pt < EPK_PATTERNS; pt++) {
        if (bank == 0) fill_factory(pt);
        else generate(pt, (uint32_t)(bank * 64 + pt));
    }
    g.bank_id = bank;
}

// A pattern of the bank in memory becomes the live one (pasa a sonar)
void load_live(int pt, int restart) {
    g.pattern = pt;
    for (int t = 0; t < EPK_TRACKS; t++) {
        Track &tr = g.tr[t];
        for (int p = 0; p < EPK_P_PATTERN_COUNT; p++) tr.p[p] = g.bank[pt][t][p];
        tr.st = g.bank_steps[pt][t];
        if (restart && g.playing) { tr.step = -1; tr.pos = 0; tr.fire = 1; tr.cycle = 0xFFFFFFFFu; }
    }
}

// The live parameters go back to their pattern; the new pattern comes in; every track restarts at step 0.
// Mutes, solos, levels and pans stay: they are the project's (del proyecto).
void switch_pattern(int to, int bank) {
    if (bank == g.bank_id) {
        for (int t = 0; t < EPK_TRACKS; t++) {
            for (int p = 0; p < EPK_P_PATTERN_COUNT; p++) g.bank[g.pattern][t][p] = g.tr[t].p[p];
            g.bank_steps[g.pattern][t] = g.tr[t].st;
        }
    } else {
        load_bank(bank);                                             // the old bank's edits are not kept yet (fase 6)
    }
    g.queued = -1;
    load_live(to, 1);
}

// The voice follows the ENGINE parameter (la voz sigue al parámetro MOTOR)
inline void sync_engine(Track &t) {
    const int eng = param_int(t, EPK_P_ENGINE);
    if (eng != t.voice.engine) t.voice.init(eng, t.voice.seed);
}

void drain() {
    while (g.q_read != g.q_write) {
        const Event &e = g.queue[g.q_read & (kQueue - 1)];
        Track *t = e.track < EPK_TRACKS ? &g.tr[e.track] : nullptr;
        if (e.type == EV_NOTE_ON && t) {
            sync_engine(*t);
            t->voice.trigger(note_hz(e.a), (float)e.b / 127.0f);
            t->triggers++;
        } else if (e.type == EV_STEP_TOGGLE && t) {
            const int len = param_int(*t, EPK_P_LENGTH);
            if (e.a < len) {
                if (!t->st.manual) { t->st.mask = euclid(len, param_int(*t, EPK_P_HITS)); t->st.manual = 1; }
                t->st.mask ^= 1u << unrotated(*t, e.a, len);
            }
        } else if (e.type == EV_STEP_META && t) {
            const int len = param_int(*t, EPK_P_LENGTH);
            if (e.a < len) t->st.meta[unrotated(*t, e.a, len)] = e.b;
        } else if (e.type == EV_EUCLID && t) {
            t->st.manual = 0;
        } else if (e.type == EV_UNDO) {
            if (g.undo.ready && g.undo.pattern == g.pattern && g.undo.bank == g.bank_id)
                for (int k = 0; k < EPK_TRACKS; k++) {
                    Track &u = g.tr[k];
                    for (int p = 0; p < EPK_P_PATTERN_COUNT; p++) u.p[p] = g.undo.p[k][p];
                    u.p[EPK_P_PAN] = g.undo.pan[k];
                    u.st = g.undo.st[k];
                }
            g.undo.ready = 0;
        } else if (e.type == EV_RANDOM) {
            generate(g.pattern, (uint32_t)e.a << 8 | e.b | (uint32_t)e.track << 16 | 0x5A000000u);
            load_live(g.pattern, 0);                          // the cycle goes on (el ciclo sigue)
        } else if (e.type == EV_PATTERN) {
            if (g.playing) { g.queued = e.a; g.queued_bank = e.b; }   // at the end of the bar (al final del compás)
            else switch_pattern(e.a, e.b);                    // stopped: now (ahora)
        } else if (e.type == EV_PLAY) {
            g.playing = 1;
            g.bar_step = -1; g.bar_pos = 0; g.bar_fire = 1;
            for (Track &k : g.tr) { k.step = -1; k.pos = 0; k.fire = 1; k.cycle = 0xFFFFFFFFu; }
        } else if (e.type == EV_STOP) {
            g.playing = 0;
            g.bar_step = -1;
            if (g.queued >= 0) switch_pattern(g.queued, g.queued_bank);
            for (Track &k : g.tr) k.step = -1;
        }
        g.q_read++;
    }
}

int any_solo() {
    for (const Track &t : g.tr) if (t.soloed) return 1;
    return 0;
}

// The next step: it triggers if the pattern, its condition and its chance say so (siguiente paso)
void advance(Track &t, int len, uint32_t mask) {
    t.step = (t.step + 1) % len;
    if (t.step == 0) t.cycle++;
    if (!(mask >> t.step & 1u)) return;
    const uint8_t meta = t.st.meta[unrotated(t, t.step, len)];
    const int prob = meta & 3, cond = meta >> 2 & 7;
    if (cond == EPK_COND_1_2 || cond == EPK_COND_2_2) {
        if ((int)(t.cycle & 1u) != cond - EPK_COND_1_2) return;
    } else if (cond >= EPK_COND_1_4) {
        if ((int)(t.cycle & 3u) != cond - EPK_COND_1_4) return;
    }
    if (prob && t.chance.unipolar() >= 1.0f - 0.25f * (float)prob) return;
    const int note = t.key_note >= 0 ? t.key_note : param_int(t, EPK_P_NOTE);
    t.voice.trigger(note_hz(note), t.step == 0 ? 1.0f : 0.8f);       // the first step accented (acento)
    t.triggers++;
}

// The sequencer for one track over a block: the voice is rendered in pieces, each trigger at its own sample.
void render_track(Track &t, float *l, float *r, uint32_t n, int solo) {
    const int len = param_int(t, EPK_P_LENGTH);
    const uint32_t mask = rotate_mask(base_mask(t, len), len, param_int(t, EPK_P_ROTATE));
    const float steps_per_sample = g.bpm * (1.0f / 60.0f) * kStepsPerBeat[param_int(t, EPK_P_DIVISION)] / g.sr;
    const uint32_t inc = (uint32_t)(steps_per_sample * 4294967296.0f);   // Q32 per sample (por muestra)
    sync_engine(t);
    if (t.step >= len) t.step = len - 1;                             // the length was shortened (se acortó)
    uint32_t done = 0;
    while (done < n) {
        if (g.playing && t.fire) {                                   // play: step 0 now (paso 0 ya)
            t.fire = 0;
            t.step = -1;
            advance(t, len, mask);
        }
        uint32_t run = n - done, until = 0xFFFFFFFFu;
        if (g.playing) {                                             // samples until the next step (hasta el paso)
            const uint32_t remain = t.pos ? 0u - t.pos : 0xFFFFFFFFu;
            until = remain / inc + (remain % inc ? 1u : 0u);
            if (until < run) run = until;
        }
        t.voice.render(g.buf + done, run, t.p, g.sr);
        done += run;
        if (g.playing) {
            t.pos += run * inc;                                      // wraps past 2^32 at the step's end
            if (run == until) advance(t, len, mask);
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

inline int valid(int track) { return track >= 0 && track < EPK_TRACKS; }

}  // namespace epk

using namespace epk;

// ------------------------------------------------------------------ the C interface (interfaz C)
extern "C" {

void epk_init(float sample_rate) {
    // the project's mix (mezcla del proyecto): level, pan per track
    static const float kMix[EPK_TRACKS][2] = {{0.85f, 0.50f}, {0.50f, 0.58f}, {0.60f, 0.40f},
                                              {0.62f, 0.62f}, {0.50f, 0.36f}, {0.45f, 0.70f}};
    g.sr = sample_rate;
    g.bpm = 124.0f;
    g.playing = 0;
    g.q_write = g.q_read = 0;
    g.pattern = 0; g.queued = -1;
    g.bar_pos = 0; g.bar_step = -1; g.bar_fire = 0;
    g.undo.ready = 0;
    g.queued_bank = 0;
    load_bank(0);
    for (int i = 0; i < EPK_TRACKS; i++) {
        Track &t = g.tr[i];
        for (int p = 0; p < EPK_P_PATTERN_COUNT; p++) t.p[p] = g.bank[0][i][p];
        t.st = g.bank_steps[0][i];
        t.p[EPK_P_LEVEL] = kMix[i][0];
        t.p[EPK_P_PAN] = kMix[i][1];
        t.muted = t.soloed = 0;
        t.step = -1; t.pos = 0; t.fire = 0; t.cycle = 0;
        t.gain = 1.0f; t.meter = 0.0f; t.triggers = 0; t.key_note = -1;
        t.chance.init(0x2545F491u * (uint32_t)(i + 1));
        t.voice.init(param_int(t, EPK_P_ENGINE), 0x1234567u * (uint32_t)(i + 1));
    }
}

void epk_render(float *left, float *right, uint32_t frames) {
    drain();
    const int solo = any_solo();
    for (uint32_t i = 0; i < frames; i++) left[i] = right[i] = 0.0f;
    // the bar clock (reloj de compás) in sixteenths: a queued pattern comes in on the sample its bar starts
    const uint32_t inc = (uint32_t)(g.bpm * (1.0f / 60.0f) * 4.0f / g.sr * 4294967296.0f);
    uint32_t done = 0;
    while (done < frames) {
        if (g.playing && g.bar_fire) {
            g.bar_fire = 0;
            g.bar_step = (g.bar_step + 1) % EPK_BAR_STEPS;
            if (g.bar_step == 0 && g.queued >= 0) switch_pattern(g.queued, g.queued_bank);
        }
        uint32_t n = frames - done, until = 0xFFFFFFFFu;
        if (n > kBlock) n = kBlock;
        if (g.playing) {
            const uint32_t remain = g.bar_pos ? 0u - g.bar_pos : 0xFFFFFFFFu;
            until = remain / inc + (remain % inc ? 1u : 0u);
            if (until < n) n = until;
        }
        for (Track &t : g.tr) render_track(t, left + done, right + done, n, solo);
        done += n;
        if (g.playing) {
            g.bar_pos += n * inc;
            if (n == until) g.bar_fire = 1;
        }
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
    for (Track &t : g.tr) t.voice.init(t.voice.engine, t.voice.seed);
}

void epk_play(int on) { push(on ? EV_PLAY : EV_STOP, 0, 0, 0); }
int epk_playing(void) { return g.playing; }
void epk_set_tempo(float bpm) { g.bpm = bpm < 40.0f ? 40.0f : bpm > 200.0f ? 200.0f : bpm; }
float epk_tempo(void) { return g.bpm; }

void epk_param_set(int track, int param, float value01) {
    if (!valid(track) || param < 0 || param >= EPK_P_COUNT) return;
    g.tr[track].p[param] = clamp01(value01);
}
float epk_param_get(int track, int param) {
    if (!valid(track) || param < 0 || param >= EPK_P_COUNT) return 0.0f;
    return g.tr[track].p[param];
}
int epk_param_steps(int param) {
    switch (param) {
    case EPK_P_NOTE: return 61;
    case EPK_P_LENGTH: case EPK_P_ROTATE: return 32;
    case EPK_P_HITS: return 33;
    case EPK_P_DIVISION: return EPK_DIV_COUNT;
    case EPK_P_ENGINE: return EPK_ENG_COUNT;
    default: return 0;
    }
}
int epk_engine(int track) { return valid(track) ? param_int(g.tr[track], EPK_P_ENGINE) : 0; }

void epk_mute(int track, int on) { if (valid(track)) g.tr[track].muted = !!on; }
int epk_muted(int track) { return valid(track) ? g.tr[track].muted : 0; }
void epk_solo(int track, int on) { if (valid(track)) g.tr[track].soloed = !!on; }
int epk_soloed(int track) { return valid(track) ? g.tr[track].soloed : 0; }
int epk_audible(int track) {
    if (!valid(track)) return 0;
    const Track &t = g.tr[track];
    return !t.muted && (!any_solo() || t.soloed);
}

void epk_step_toggle(int track, int step) {
    if (valid(track) && step >= 0 && step < EPK_MAX_STEPS) push(EV_STEP_TOGGLE, track, step, 0);
}
void epk_step_set(int track, int step, int prob, int cond) {
    if (!valid(track) || step < 0 || step >= EPK_MAX_STEPS) return;
    push(EV_STEP_META, track, step, clampi(prob, 0, EPK_PROB_COUNT - 1) | clampi(cond, 0, EPK_COND_COUNT - 1) << 2);
}
static int step_meta(int track, int step) {
    if (!valid(track)) return 0;
    const Track &t = g.tr[track];
    const int len = param_int(t, EPK_P_LENGTH);
    return step >= 0 && step < len ? t.st.meta[unrotated(t, step, len)] : 0;
}
int epk_step_prob(int track, int step) { return step_meta(track, step) & 3; }
int epk_step_cond(int track, int step) { return step_meta(track, step) >> 2 & 7; }
int epk_steps_manual(int track) { return valid(track) ? g.tr[track].st.manual : 0; }
void epk_steps_euclid(int track) { if (valid(track)) push(EV_EUCLID, track, 0, 0); }

void epk_undo_save(void) {
    g.undo.ready = 0;
    for (int k = 0; k < EPK_TRACKS; k++) {
        const Track &t = g.tr[k];
        for (int p = 0; p < EPK_P_PATTERN_COUNT; p++) g.undo.p[k][p] = t.p[p];
        g.undo.pan[k] = t.p[EPK_P_PAN];
        g.undo.st[k] = t.st;
    }
    g.undo.pattern = g.pattern;
    g.undo.bank = g.bank_id;
    g.undo.ready = 1;
}
void epk_undo_restore(void) { push(EV_UNDO, 0, 0, 0); }
int epk_undo_ready(void) { return g.undo.ready && g.undo.pattern == g.pattern && g.undo.bank == g.bank_id; }

void epk_note_on(int track, int note, float velocity) {
    if (!valid(track)) return;
    g.tr[track].key_note = note;
    push(EV_NOTE_ON, track, clampi(note, 0, 127), clampi((int)(velocity * 127.0f), 1, 127));
}
void epk_note_off(int track, int note) {
    if (valid(track) && g.tr[track].key_note == note) g.tr[track].key_note = -1;
}

void epk_pattern_select(int bank, int pattern) {
    if (pattern >= 0 && pattern < EPK_PATTERNS && bank >= 0 && bank < EPK_BANKS) push(EV_PATTERN, 0, pattern, bank);
}
int epk_pattern_current(void) { return g.pattern; }
int epk_pattern_queued(void) { return g.queued; }
int epk_bank_current(void) { return g.bank_id; }
int epk_bank_queued(void) { return g.queued >= 0 ? g.queued_bank : -1; }
const char *epk_pattern_name(int bank, int pattern) {
    static char name[8];
    if (pattern < 0 || pattern >= EPK_PATTERNS || bank < 0 || bank >= EPK_BANKS) return "";
    if (bank == 0) return kFactoryNames[pattern];
    name[0] = 'G'; name[1] = 'E'; name[2] = 'N'; name[3] = ' ';
    name[4] = (char)('A' + bank);
    name[5] = (char)('0' + (pattern + 1) / 10);
    name[6] = (char)('0' + (pattern + 1) % 10);
    name[7] = 0;
    return name;
}
void epk_pattern_randomize(uint32_t seed) { push(EV_RANDOM, (int)(seed >> 16 & 0xFFu), (int)(seed >> 8 & 0xFFu), (int)(seed & 0xFFu)); }
int epk_bar_step(void) { return g.playing ? g.bar_step : -1; }

int epk_step(int track) { return valid(track) && g.playing ? g.tr[track].step : -1; }
int epk_length(int track) { return valid(track) ? param_int(g.tr[track], EPK_P_LENGTH) : 0; }
uint32_t epk_pattern(int track) { return valid(track) ? play_mask(g.tr[track]) : 0; }
float epk_meter(int track) {
    if (!valid(track)) return 0.0f;
    float m = g.tr[track].meter;
    g.tr[track].meter = 0.0f;
    return m;
}
float epk_meter_master(void) { float m = g.meter_master; g.meter_master = 0.0f; return m; }
uint32_t epk_triggers(int track) { return valid(track) ? g.tr[track].triggers : 0; }

}  // extern "C"
