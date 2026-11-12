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
#include "fx.h"
#include "master.h"
#include "mod.h"
#include "marea.h"

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
    uint8_t meta[EPK_MAX_STEPS];           // bits 0-1 EPK_PROB_*, 2-4 EPK_COND_*, 5-7 micro-timing (signed, eighths
                                           // of a step); indexed before rotation
};

// ------------------------------------------------------------------ track (pista)
struct Track {
    float p[EPK_P_COUNT];                  // parameters (parámetros), 0..1
    float pm[EPK_P_COUNT];                 // the same with the modulation on (con modulación), each block
    float peak_block, lvl_s;               // for the followers; the level, smoothed (suavizado)
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
    // evolution (evolución): offsets over the pattern, moved every EVERY bars (desvíos sobre el patrón)
    float m_tone, m_decay, m_char;
    int m_note, m_hits, m_rot;             // semitones, hits, steps
    int absent;                            // resting this stretch (en ausencia)
    int evo_off;                           // evolution off for this track (apagada; del proyecto)
    // the track's LFO (LFO de pista): its output with the amount, and what it does to the rhythm this block
    float lfo_out, lfo_last_ph, lfo_from, lfo_to, lfo_walk;
    int lfo_hits, lfo_rot;
    Rng lfo_rng;
    uint32_t bars;                         // bars since play (compases desde PLAY)
    Rng evo;
    int thrown;                            // a dub throw to the delay (throw al eco)
    float throw_g;
    // triggers off the grid (golpes fuera de la rejilla): [0] this step's, late; [1] the next step's, early.
    // at: where in the current step, Q32
    struct Pending { uint32_t at; int step, on; } pend[2];
    Voice voice;
};

struct Event { uint8_t type, track, a, b; };
enum { EV_NOTE_ON = 1, EV_PLAY, EV_STOP, EV_PATTERN, EV_STEP_TOGGLE, EV_STEP_META, EV_EUCLID, EV_UNDO, EV_RANDOM,
       EV_KEEP };

constexpr int kQueue = 64;                 // a power of two
constexpr uint32_t kBlock = 256;           // the largest block the platform renders (APP_BLOCK)

struct Undo {
    int ready, pattern, bank, key;
    float p[EPK_TRACKS][EPK_P_PATTERN_COUNT], mix[EPK_TRACKS][3];   // mix: pan, send A, send B
    Steps st[EPK_TRACKS];
};

struct Engine {
    float sr, bpm;
    int playing;
    float bank[EPK_PATTERNS][EPK_TRACKS][EPK_P_PATTERN_COUNT];   // the patterns (patrones), 0..1 like Track::p
    Steps bank_steps[EPK_PATTERNS][EPK_TRACKS];
    int pattern, queued;                   // current, queued (-1 none)
    int bank_id, queued_bank;              // the bank in memory (banco en memoria), the queued pattern's
    uint16_t user_mask;                    // the current bank's slots that come from the store (guardados)
    int evo_hold;                          // every track's evolution frozen (todo congelado)
    int key;                               // the key in use, semitones from D (tonalidad: desde re), -5..6
    int key_lock;                          // the whole-pattern random keeps it (tonalidad fija)
    int8_t pat_key[EPK_PATTERNS];          // each slot's key (la de cada ranura)
    // what the store holds for the bank about to load (lo guardado del banco que va a cargar): written by the main
    // loop before the change, read when it loads. stage_bank -1 while it is being written
    volatile int stage_bank;
    uint16_t stage_mask;
    uint8_t stage[EPK_PATTERNS][EPK_PATTERN_BYTES];
    uint32_t bar_pos;                      // the bar clock (reloj de compás): Q32 of a sixteenth
    int bar_step, bar_fire;
    Track tr[EPK_TRACKS];
    Undo undo;
    Event queue[kQueue];
    volatile uint32_t q_write, q_read;
    float meter_master, meter_fx;
    float fx[EPK_FX_COUNT];                // the send effects' parameters (del proyecto)
    Fx sends;
    Master master;
    Mod mod;
    Marea marea;
    int marea_on;                          // the track it was on last block (para vaciarla al moverla)
    float marea_g;                         // FLOW: MAREA on 1 .. off 0 (sin clics)
    float ins_l[kBlock], ins_r[kBlock];    // the insert's stereo out (salida estéreo del inserto)
    float fx_live[EPK_FX_COUNT];           // the effects with the modulation on
    int32_t sixteenths;                    // since play (desde PLAY), for the LFOs
    float buf[kBlock], bus_a[kBlock], bus_b[kBlock], kick[kBlock], fx_l[kBlock], fx_r[kBlock];
};

static Engine g EPK_POOL;                  // zero-initialised (pool RAM on the device): no constructor runs at boot

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

// With the evolution on (con la evolución): hits never go to 0 while the pattern has some (that is ABSENCE's job)
// _evo: the evolution only (what is saved); _live: with the track LFO too (lo que suena)
inline int hits_with(const Track &t, int len, int extra) {
    const int h = param_int(t, EPK_P_HITS);
    if (!h) return 0;
    return clampi(h + t.m_hits + extra, 1, len);
}
inline int hits_evo(const Track &t, int len) { return hits_with(t, len, 0); }
inline int hits_live(const Track &t, int len) { return hits_with(t, len, t.lfo_hits); }
inline int rot_evo(const Track &t) { return param_int(t, EPK_P_ROTATE) + t.m_rot; }
inline int rot_live(const Track &t) { return rot_evo(t) + t.lfo_rot; }
inline int note_live(const Track &t) { return clampi(param_int(t, EPK_P_NOTE) + t.m_note, 24, 84); }
static const uint8_t kEveryBars[5] = {1, 2, 4, 8, 16};
inline int every_bars(const Track &t) { return kEveryBars[clampi((int)(t.p[EPK_P_EVERY] * 4.0f + 0.5f), 0, 4)]; }

// The hits before rotation (golpes sin rotar), and as they play (como suenan)
inline uint32_t base_mask(const Track &t, int len) {
    return t.st.manual ? t.st.mask : euclid(len, hits_live(t, len));
}
inline uint32_t play_mask(const Track &t) {
    const int len = param_int(t, EPK_P_LENGTH);
    return rotate_mask(base_mask(t, len), len, rot_live(t));
}
// A step as it plays -> its index before rotation
inline int unrotated(const Track &t, int step, int len) {
    const int rot = (rot_live(t) % len + len) % len;
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
// the engines by track (motores): kick, hats, clap / snare on 6, and the test voice until the other engines exist
static const uint8_t kEngine[EPK_TRACKS] = {EPK_ENG_GRAVE, EPK_ENG_METAL, EPK_ENG_ACORDE, EPK_ENG_TEST,
                                            EPK_ENG_DRONE, EPK_ENG_GOLPE};

// The track LFO's defaults (por defecto): sine, 8 bars, to TONE, amount 0. Also what older saved patterns get
static const float kLfoDefault[4] = {0.0f, 7.0f / 9.0f, 0.0f, 0.5f};

void set_spec(int pt, int i, int note, float ch, float tone, float dec, float e1, float e2, int len, int hits,
              int rot, int div) {
    float *b = g.bank[pt][i];
    b[EPK_P_NOTE] = (float)(clampi(note, 24, 84) - 24) / 60.0f;
    b[EPK_P_CHARACTER] = ch;
    b[EPK_P_TONE] = tone;
    b[EPK_P_DECAY] = dec;
    b[EPK_P_ENGINE] = (float)kEngine[i] / (float)(EPK_ENG_COUNT - 1);   // set_engine() changes it afterwards
    b[EPK_P_EXTRA1] = e1;
    b[EPK_P_EXTRA2] = e2;
    b[EPK_P_ATTACK] = 0.0f;
    b[EPK_P_SWING] = 0.0f;
    b[EPK_P_SHIFT] = 0.5f;                                           // on the grid (en la rejilla)
    b[EPK_P_LENGTH] = (float)(len - 1) / 31.0f;
    b[EPK_P_HITS] = (float)clampi(hits, 0, len) / (float)len;
    b[EPK_P_ROTATE] = (float)(rot % len) / 31.0f;
    b[EPK_P_DIVISION] = (float)div / (float)(EPK_DIV_COUNT - 1);
    b[EPK_P_MUTATE] = 0.0f;                                          // still, until asked (quieto)
    b[EPK_P_EVERY] = 0.5f;                                           // 4 bars
    b[EPK_P_RETURN] = 0.3f;
    b[EPK_P_ABSENCE] = 0.0f;
    b[EPK_P_SCOPE] = 0.0f;                                           // everything (todo)
    for (int q = 0; q < 4; q++) b[EPK_P_LFO_SHAPE + q] = kLfoDefault[q];   // still: amount 0 (quieto)
    Steps &s = g.bank_steps[pt][i];
    s.mask = 0; s.manual = 0;
    for (uint8_t &m : s.meta) m = 0;
}

inline void set_engine(int pt, int i, int eng) { g.bank[pt][i][EPK_P_ENGINE] = (float)eng / (float)(EPK_ENG_COUNT - 1); }

void fill_factory(int pt) {
    for (int i = 0; i < EPK_TRACKS; i++) {
        const TrackSpec &k = kFactory[pt][i];
        const float e1 = kEngine[i] == EPK_ENG_GRAVE ? 0.45f : kEngine[i] == EPK_ENG_METAL ? 0.40f : 0.0f;
        float e2 = kEngine[i] == EPK_ENG_GRAVE ? 0.40f : kEngine[i] == EPK_ENG_METAL ? 0.30f : 0.0f;
        if (i == 0 && pt == 8) e2 = 0.75f;                           // ECLIPSE: more rumble (más rumble)
        if (i == 0 && pt == 13) e2 = 0.85f;                          // NADIR
        if (kEngine[i] == EPK_ENG_ACORDE) {                          // dub chords: minor colours, the filter opening
            static const uint8_t kChord[EPK_PATTERNS] = {5, 4, 6, 4, 5, 2, 4, 6, 4, 5, 3, 4, 7, 5, 6, 4};   // Original
            set_spec(pt, i, k.note, (float)kChord[pt] / 10.0f, k.tone * 0.8f, k.decay * 0.85f, 0.0f, 0.35f + 0.3f * k.tone,
                     k.length, k.hits, k.rotate, k.division);
            continue;
        }
        if (kEngine[i] == EPK_ENG_DRONE) {                           // a fifth that drifts (una quinta que deriva)
            set_spec(pt, i, k.note, 0.40f, k.tone + 0.08f, 0.55f + 0.4f * k.decay, 0.45f, 0.6f, k.length, k.hits, k.rotate,
                     k.division);
            g.bank[pt][i][EPK_P_ATTACK] = 0.35f;
            continue;
        }
        if (kEngine[i] == EPK_ENG_GOLPE) {                           // rim at the start, clap, snare at the peak
            if (pt == 4 || pt == 6 || pt == 10 || pt == 11) {        // PULSAR, PERIGEO, FOSFORO, CUASAR: a tom / conga
                set_spec(pt, i, pt == 10 ? 57 : 50, pt == 11 ? 0.55f : 0.15f, 0.35f, 0.40f, 0.35f, 0.30f, k.length, k.hits,
                         k.rotate, k.division);
                set_engine(pt, i, EPK_ENG_TAMBOR);
                continue;
            }
            const float type = pt < 3 ? 0.95f : pt >= 8 && pt < 12 ? 0.50f : 0.05f;
            set_spec(pt, i, 52, type, k.tone, k.decay, 0.5f, 0.4f, k.length, k.hits, k.rotate, k.division);
            continue;
        }
        set_spec(pt, i, k.note, k.character, k.tone, k.decay, e1, e2, k.length, k.hits, k.rotate, k.division);
    }
    // the synths move by themselves (los sintes se mueven solos), the drums hold the groove: the chord every 4 bars,
    // the bass every 4, the drone every 8; kick, hats and percussion still (quietos: MUTAR 0)
    static const float kEvo[EPK_TRACKS][4] = {{0.0f, 0.5f, 0.3f, 0.0f},   {0.0f, 0.25f, 0.35f, 0.0f},
                                              {0.30f, 0.5f, 0.25f, 0.0f}, {0.20f, 0.5f, 0.30f, 0.0f},
                                              {0.25f, 0.75f, 0.20f, 0.0f}, {0.0f, 0.25f, 0.30f, 0.0f}};
    for (int i = 0; i < EPK_TRACKS; i++)
        for (int q = 0; q < 4; q++) g.bank[pt][i][EPK_P_MUTATE + q] = kEvo[i][q];
    // and the track LFOs: the chord breathes into the delay over 4 bars, the bass's tone drifts (deriva)
    float *ch = g.bank[pt][2], *bs = g.bank[pt][3];
    ch[EPK_P_LFO_SHAPE] = 0.0f;
    ch[EPK_P_LFO_RATE] = 6.0f / 9.0f;
    ch[EPK_P_LFO_DEST] = (float)EPK_TLFO_SEND_A / (EPK_TLFO_DEST_COUNT - 1);
    ch[EPK_P_LFO_AMT] = 0.5f + 0.5f * 0.30f;
    bs[EPK_P_LFO_SHAPE] = 1.0f;                                       // DRIFT, the last shape
    bs[EPK_P_LFO_RATE] = 7.0f / 9.0f;
    bs[EPK_P_LFO_DEST] = (float)EPK_TLFO_TONE / (EPK_TLFO_DEST_COUNT - 1);
    bs[EPK_P_LFO_AMT] = 0.5f + 0.5f * 0.25f;
}

inline float rr(Rng &r, float lo, float hi) { return lo + (hi - lo) * r.unipolar(); }
inline int ri(Rng &r, int n) { int v = (int)(r.unipolar() * (float)n); return v >= n ? n - 1 : v; }   // 0..n-1
// A note of D minor (re menor): root is a D, degrees up from it
inline int scale_note(Rng &r, int root, int degrees) {
    static const int8_t kScale[7] = {0, 2, 3, 5, 7, 8, 10};
    const int d = ri(r, degrees);
    return root + 12 * (d / 7) + kScale[d % 7];
}

// The kick by hand (bombo a mano): these steps of 16, and one more at 50 % (ghost: -1 none)
void kick_steps(int pt, uint32_t hits, int ghost) {
    Steps &s = g.bank_steps[pt][0];
    s.manual = 1;
    s.mask = hits;
    if (ghost >= 0) { s.mask |= 1u << ghost; s.meta[ghost] = EPK_PROB_50; }
}

// A whole pattern from a seed (un patrón entero a partir de una semilla): each track keeps its role, inside
// ranges that work: a kick on four less than half the time, odd cycle lengths for the polymetry, notes in a minor
// key: `key` semitones from D (-5..6).
void generate(int pt, uint32_t seed, int key) {
    static const uint8_t kLen[12] = {7, 9, 10, 11, 12, 13, 15, 16, 16, 20, 24, 32};
    Rng r;
    r.init(seed * 2654435761u + 0x9E3779B9u);
    for (int i = 0; i < 4; i++) r.bipolar();
    {   // 1 GRAVE: eight families (familias), the four on the floor 38 %
        const float drive = rr(r, 0.10f, 0.70f), click = rr(r, 0.20f, 0.80f), dec = rr(r, 0.30f, 0.70f);
        const float sweep = rr(r, 0.30f, 0.60f), rumble = rr(r, 0.0f, 0.80f);
        const int note = 29 + ri(r, 8);
        const float x = r.unipolar();
        if (x < 0.10f)                                               // none: ambient (sin bombo)
            set_spec(pt, 0, note, drive, click, dec, sweep, rumble, 16, 0, 0, EPK_DIV_1_16);
        else if (x < 0.48f)                                          // four on the floor (a negras)
            set_spec(pt, 0, note, drive, click, dec, sweep, rumble, 16, 4, 0, EPK_DIV_1_16);
        else if (x < 0.58f) {                                        // four and a ghost (y un fantasma al 50 %)
            set_spec(pt, 0, note, drive, click, dec, sweep, rumble, 16, 4, 0, EPK_DIV_1_16);
            kick_steps(pt, 1u << 0 | 1u << 4 | 1u << 8 | 1u << 12, r.unipolar() < 0.5f ? 14 : 7);
        } else if (x < 0.70f) {                                      // broken, euclidean (roto)
            static const uint8_t kH[3] = {3, 5, 7};
            set_spec(pt, 0, note, drive, click, dec, sweep, rumble, 16, kH[ri(r, 3)], ri(r, 3), EPK_DIV_1_16);
        } else if (x < 0.78f)                                        // half time (medio tiempo)
            set_spec(pt, 0, note, drive, click, dec, sweep, rumble, 16, 2, 0, EPK_DIV_1_16);
        else if (x < 0.86f) {                                        // 2-step
            set_spec(pt, 0, note, drive, click, dec, sweep, rumble, 16, 2, 0, EPK_DIV_1_16);
            kick_steps(pt, 1u << 0 | 1u << 10, r.unipolar() < 0.5f ? 7 : -1);
        } else if (x < 0.93f) {                                      // dotted (con puntillo): 0, 6, 12
            set_spec(pt, 0, note, drive, click, dec, sweep, rumble, 16, 3, 0, EPK_DIV_1_16);
            kick_steps(pt, 1u << 0 | 1u << 6 | 1u << 12, r.unipolar() < 0.5f ? 14 : -1);
        } else {                                                     // its own cycle, hypnotic (ciclo propio)
            static const uint8_t kL[3] = {12, 14, 15};
            const int len = kL[ri(r, 3)];
            set_spec(pt, 0, note, drive, click, dec, sweep, rumble, len, len / 4 + ri(r, 2), 0, EPK_DIV_1_16);
        }
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
        const float ch = r.unipolar(), tone = rr(r, 0.20f, 0.50f), dec = rr(r, 0.30f, 0.75f);
        set_spec(pt, 2, scale_note(r, 50 + key, 10), ch, tone, dec, (float)ri(r, 4) / 3.0f, rr(r, 0.2f, 0.7f), len, hits, ri(r, len),
                 EPK_DIV_1_16);
    }
    {   // 4 bass (bajo)
        const int len = kLen[ri(r, 12)], hits = 2 + ri(r, len * 45 / 100 + 1);
        const float ch = rr(r, 0.50f, 0.65f), tone = rr(r, 0.25f, 0.50f), dec = rr(r, 0.25f, 0.40f);
        set_spec(pt, 3, scale_note(r, 38 + key, 5), ch, tone, dec, 0.0f, 0.0f, len, hits, ri(r, len), EPK_DIV_1_16);
    }
    {   // 5 drone
        static const uint8_t kDroneLen[3] = {5, 7, 9};
        const int len = kDroneLen[ri(r, 3)], hits = 1 + ri(r, 3);
        const float tone = rr(r, 0.18f, 0.30f), dec = rr(r, 0.90f, 1.0f);
        const int div = r.unipolar() < 0.5f ? EPK_DIV_1_8 : EPK_DIV_1_4;
        set_spec(pt, 4, scale_note(r, 38 + key, 8), rr(r, 0.1f, 0.8f), tone + 0.08f, dec * 0.9f, rr(r, 0.2f, 0.8f), (float)ri(r, 6) / 5.0f,
                 len, hits, 0, div);
        g.bank[pt][4][EPK_P_ATTACK] = rr(r, 0.2f, 0.6f);
    }
    {   // 6 GOLPE: clap, snare or rim; often the backbeat (2 y 4), otherwise a cycle of its own
        static const float kType[3] = {0.05f, 0.50f, 0.95f};
        const float type = kType[ri(r, 3)], tone = rr(r, 0.35f, 0.80f), dec = rr(r, 0.15f, 0.50f);
        const float body = rr(r, 0.30f, 0.80f), tail = rr(r, 0.20f, 0.70f);
        const int back = r.unipolar() < 0.45f;
        const int len = back ? 16 : kLen[ri(r, 12)], hits = back ? 2 : 1 + ri(r, len * 3 / 10 + 1);
        if (!back && r.unipolar() < 0.5f) {                          // or a tom / conga / metallic hit (TAMBOR)
            set_spec(pt, 5, 43 + ri(r, 24), r.unipolar(), rr(r, 0.10f, 0.70f), rr(r, 0.20f, 0.60f), rr(r, 0.0f, 0.60f),
                     rr(r, 0.0f, 0.50f), len, hits, ri(r, len), EPK_DIV_1_16);
            set_engine(pt, 5, EPK_ENG_TAMBOR);
        } else
            set_spec(pt, 5, 48 + ri(r, 10), type, tone, dec, body, tail, len, hits, back ? 4 : ri(r, len), EPK_DIV_1_16);
    }
    for (int i = 2; i < EPK_TRACKS; i++)                             // now and then a track rests (descansa)
        if (r.unipolar() < 0.10f) g.bank[pt][i][EPK_P_HITS] = 0.0f;
    for (int i = 1; i < EPK_TRACKS; i++)                             // some steps by chance (pasos con azar)
        for (uint8_t &m : g.bank_steps[pt][i].meta)
            if (r.unipolar() < 0.12f) m = r.unipolar() < 0.5f ? EPK_PROB_50 : EPK_PROB_75;
    for (int i = 1; i < EPK_TRACKS; i++) {                           // the synths evolve, the drums seldom
        float *b = g.bank[pt][i];
        const int synth = i >= 2 && i <= 4;
        if (r.unipolar() < (synth ? 0.75f : 0.0f)) {
            b[EPK_P_MUTATE] = rr(r, 0.10f, 0.45f);
            b[EPK_P_EVERY] = (float)(1 + ri(r, 3)) / 4.0f;              // 2, 4 or 8 bars
            b[EPK_P_RETURN] = rr(r, 0.15f, 0.45f);
        }
        if (synth && r.unipolar() < 0.3f) b[EPK_P_ABSENCE] = rr(r, 0.05f, 0.25f);
    }
}

// ------------------------------------------------------------------ saved patterns (patrones guardados)
// A pattern's record (registro): see epk.h. Packed from parameters and steps, unpacked into a bank slot.
// A parameter with fixed steps in one byte, the others in two (un byte o dos)
constexpr int narrow(int q) {
    return q == EPK_P_NOTE || q == EPK_P_ENGINE || q == EPK_P_LENGTH || q == EPK_P_HITS || q == EPK_P_ROTATE ||
           q == EPK_P_DIVISION || q == EPK_P_EVERY || q == EPK_P_SCOPE || q == EPK_P_LFO_SHAPE || q == EPK_P_LFO_RATE ||
           q == EPK_P_LFO_DEST;
}
constexpr int param_bytes(int q) { return q >= EPK_P_PATTERN_COUNT ? 0 : (narrow(q) ? 1 : 2) + param_bytes(q + 1); }
static_assert(param_bytes(0) == EPK_PATTERN_PARAM_BYTES, "EPK_PATTERN_PARAM_BYTES out of step with the parameters");
static_assert(EPK_BANK_OBJ_BYTES <= 3840, "half a bank must fit one store object (PLAT_STORE_MAX)");

void pack_params(const float *p, uint8_t *&o) {
    for (int q = 0; q < EPK_P_PATTERN_COUNT; q++) {
        if (narrow(q)) { *o++ = (uint8_t)(clamp01(p[q]) * 255.0f + 0.5f); continue; }
        const uint32_t v = (uint32_t)(clamp01(p[q]) * 65535.0f + 0.5f);
        *o++ = (uint8_t)v; *o++ = (uint8_t)(v >> 8);
    }
}
void pack_track(const float *p, const Steps &st, uint8_t *&o) {
    pack_params(p, o);
    for (int k = 0; k < 4; k++) *o++ = (uint8_t)(st.mask >> (8 * k));
    *o++ = st.manual;
    for (int k = 0; k < EPK_MAX_STEPS; k++) *o++ = st.meta[k];
}
void unpack_pattern(const uint8_t *in, int pt) {
    for (int i = 0; i < EPK_TRACKS; i++) {
        float *b = g.bank[pt][i];
        for (int q = 0; q < EPK_P_PATTERN_COUNT; q++) {
            if (narrow(q)) { b[q] = (float)*in++ * (1.0f / 255.0f); continue; }
            b[q] = (float)(in[0] | in[1] << 8) * (1.0f / 65535.0f);
            in += 2;
        }
        Steps &st = g.bank_steps[pt][i];
        st.mask = (uint32_t)in[0] | (uint32_t)in[1] << 8 | (uint32_t)in[2] << 16 | (uint32_t)in[3] << 24;
        in += 4;
        st.manual = *in++ ? 1 : 0;
        for (int k = 0; k < EPK_MAX_STEPS; k++) st.meta[k] = *in++;
    }
}
// The live pattern as it sounds (como suena), its evolution baked in (con la evolución dentro)
// Written straight into its destination: no big buffer on the audio interrupt's stack (sin búferes en la pila)
void bake_track(int i, float *out, Steps &st) {
    {
        const Track &t = g.tr[i];
        for (int q = 0; q < EPK_P_PATTERN_COUNT; q++) out[q] = t.p[q];
        const int len = param_int(t, EPK_P_LENGTH);
        out[EPK_P_NOTE] = (float)(note_live(t) - 24) / 60.0f;
        out[EPK_P_TONE] = clamp01(t.p[EPK_P_TONE] + t.m_tone);
        out[EPK_P_DECAY] = clamp01(t.p[EPK_P_DECAY] + t.m_decay);
        out[EPK_P_CHARACTER] = clamp01(t.p[EPK_P_CHARACTER] + t.m_char);
        out[EPK_P_HITS] = (float)hits_evo(t, len) / (float)len;     // the LFO keeps running in it: not baked
        out[EPK_P_ROTATE] = (float)((rot_evo(t) % len + len) % len) / 31.0f;
        st = t.st;
        if (t.st.manual && t.m_rot) {                                // manual steps: the rotation into the steps
            const int rot = ((t.m_rot % len) + len) % len;
            st.mask = rotate_mask(t.st.mask, len, rot);
            for (int k = 0; k < len; k++) st.meta[(k + rot) % len] = t.st.meta[k];
            out[EPK_P_ROTATE] = (float)param_int(t, EPK_P_ROTATE) / 31.0f;
        }
    }
}

// Bank A is the first project; B..K are generated, the same every time (banco A de fábrica; B..K generados)
// Each bank in one key, so its patterns follow each other (cada banco en una tonalidad): A in D, the rest spread
inline int bank_key(int bank) { return bank == 0 ? 0 : (bank * 5) % 12 - 5; }

void load_bank(int bank) {
    const int staged = g.stage_bank == bank;
    g.user_mask = 0;
    for (int pt = 0; pt < EPK_PATTERNS; pt++) {
        if (staged && (g.stage_mask >> pt & 1u)) {                   // saved (guardado)
            unpack_pattern(g.stage[pt], pt);
            g.user_mask |= (uint16_t)(1u << pt);
        } else if (bank == 0) fill_factory(pt);
        else generate(pt, (uint32_t)(bank * 64 + pt), bank_key(bank));
    }
    g.bank_id = bank;
    g.key = bank_key(bank);
    for (int8_t &k : g.pat_key) k = (int8_t)g.key;
}

// The evolution back to the pattern (la evolución vuelve al patrón)
inline void evo_reset(Track &t) {
    t.m_tone = t.m_decay = t.m_char = 0.0f;
    t.m_note = t.m_hits = t.m_rot = 0;
    t.absent = 0;
    t.bars = 0;
}

inline float clamp_off(float base, float off) { return clamp01(base + off) - base; }
inline int toward0(int v) { return v > 0 ? v - 1 : v < 0 ? v + 1 : 0; }
inline int melodic(const Track &t) {
    const int e = param_int(t, EPK_P_ENGINE);
    return e == EPK_ENG_ACORDE || e == EPK_ENG_DRONE || e == EPK_ENG_TEST;
}

// A bar starts (empieza un compás): every EVERY bars each track pulls back towards its pattern by RETURN, changes by
// MUTATE, and rests or not by ABSENCE.
void evolve(Track &t) {
    if (g.evo_hold) return;                                          // all frozen (todo congelado)
    const int every = every_bars(t);
    const uint32_t bar = t.bars++;
    if (t.evo_off) {                                                 // off: the written pattern (lo escrito)
        const uint32_t keep_bars = t.bars;
        evo_reset(t);
        t.bars = keep_bars;
        return;
    }
    if (bar == 0 || bar % (uint32_t)every) return;                 // the first stretch is the pattern as written
    const float a = t.p[EPK_P_MUTATE], back = t.p[EPK_P_RETURN];
    const int scope = clampi((int)(t.p[EPK_P_SCOPE] * (EPK_SCOPE_COUNT - 1) + 0.5f), 0, EPK_SCOPE_COUNT - 1);
    const int rhythm = scope == EPK_SCOPE_ALL || scope == EPK_SCOPE_RHYTHM;
    const int sound = scope == EPK_SCOPE_ALL || scope == EPK_SCOPE_SOUND;
    const int note = scope == EPK_SCOPE_ALL || scope == EPK_SCOPE_NOTE;
    const float keep = 1.0f - back;
    t.m_tone *= keep; t.m_decay *= keep; t.m_char *= keep;
    if (t.evo.unipolar() < back) t.m_hits = toward0(t.m_hits);
    if (t.evo.unipolar() < back) t.m_rot = toward0(t.m_rot);
    if (t.evo.unipolar() < back) t.m_note = 0;
    if (!sound) t.m_tone = t.m_decay = t.m_char = 0.0f;              // what is out of scope is the pattern
    if (!rhythm) t.m_hits = t.m_rot = 0;
    if (!note) t.m_note = 0;
    if (a > 0.0f) {
        const int len = param_int(t, EPK_P_LENGTH), span = 1 + (int)(a * 3.0f);
        if (sound) {
            t.m_tone = clamp_off(t.p[EPK_P_TONE], t.m_tone + a * 0.30f * t.evo.bipolar());
            t.m_decay = clamp_off(t.p[EPK_P_DECAY], t.m_decay + a * 0.25f * t.evo.bipolar());
            t.m_char = clamp_off(t.p[EPK_P_CHARACTER], t.m_char + a * 0.20f * t.evo.bipolar());
        }
        if (rhythm && t.evo.unipolar() < a * 0.7f)
            t.m_hits = clampi(t.m_hits + (t.evo.bipolar() < 0.0f ? -1 : 1), -span, span);
        if (rhythm && t.evo.unipolar() < a * 0.6f) t.m_rot = (t.m_rot + (t.evo.bipolar() < 0.0f ? -1 : 1)) % len;
        if (note && melodic(t) && t.evo.unipolar() < a * 0.5f) {    // a fourth, a fifth, an octave (cuarta, quinta)
            static const int8_t kMove[8] = {0, 5, 7, -5, -7, 12, -12, 3};
            t.m_note = kMove[clampi((int)(t.evo.unipolar() * 8.0f), 0, 7)];
        }
    }
    t.absent = t.p[EPK_P_ABSENCE] > 0.0f && t.evo.unipolar() < t.p[EPK_P_ABSENCE];
}

// A pattern of the bank in memory becomes the live one (pasa a sonar)
void load_live(int pt, int restart) {
    g.pattern = pt;
    for (int t = 0; t < EPK_TRACKS; t++) {
        Track &tr = g.tr[t];
        for (int p = 0; p < EPK_P_PATTERN_COUNT; p++) tr.p[p] = g.bank[pt][t][p];
        tr.st = g.bank_steps[pt][t];
        evo_reset(tr);
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
    g.key = g.pat_key[to];
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
            if (g.undo.ready && g.undo.pattern == g.pattern && g.undo.bank == g.bank_id) {
                g.key = g.pat_key[g.pattern] = (int8_t)g.undo.key;
                for (int k = 0; k < EPK_TRACKS; k++) {
                    Track &u = g.tr[k];
                    for (int p = 0; p < EPK_P_PATTERN_COUNT; p++) u.p[p] = g.undo.p[k][p];
                    u.p[EPK_P_PAN] = g.undo.mix[k][0];
                    u.p[EPK_P_SEND_A] = g.undo.mix[k][1];
                    u.p[EPK_P_SEND_B] = g.undo.mix[k][2];
                    u.st = g.undo.st[k];
                }
            }
            g.undo.ready = 0;
        } else if (e.type == EV_KEEP) {                       // the live pattern into a slot (a la ranura)
            for (int k = 0; k < EPK_TRACKS; k++) bake_track(k, g.bank[e.a][k], g.bank_steps[e.a][k]);
            g.user_mask |= (uint16_t)(1u << e.a);
            g.pat_key[e.a] = (int8_t)g.key;
            if (e.a == g.pattern) load_live(g.pattern, 0);   // what you heard is now the pattern (ya es el patrón)
        } else if (e.type == EV_RANDOM) {
            const uint32_t seed = (uint32_t)e.a << 8 | e.b | (uint32_t)e.track << 16 | 0x5A000000u;
            if (!g.key_lock) g.key = (int)(seed * 2246822519u >> 28) % 12 - 5;   // a new key (tonalidad nueva)
            generate(g.pattern, seed, g.key);
            g.pat_key[g.pattern] = (int8_t)g.key;
            load_live(g.pattern, 0);                          // the cycle goes on (el ciclo sigue)
        } else if (e.type == EV_PATTERN) {
            if (g.playing) { g.queued = e.a; g.queued_bank = e.b; }   // at the end of the bar (al final del compás)
            else switch_pattern(e.a, e.b);                    // stopped: now (ahora)
        } else if (e.type == EV_PLAY) {
            g.playing = 1;
            g.bar_step = -1; g.bar_pos = 0; g.bar_fire = 1; g.sixteenths = -1;
            for (Track &k : g.tr) { k.step = -1; k.pos = 0; k.fire = 1; k.cycle = 0xFFFFFFFFu; evo_reset(k); }
        } else if (e.type == EV_STOP) {
            g.playing = 0;
            g.bar_step = -1;
            if (g.queued >= 0) switch_pattern(g.queued, g.queued_bank);
            for (Track &k : g.tr) { k.step = -1; k.pend[0].on = k.pend[1].on = 0; k.voice.release(); }
        }
        g.q_read++;
    }
}

int any_solo() {
    for (const Track &t : g.tr) if (t.soloed) return 1;
    return 0;
}

// A step's place off the grid, in steps (fuera de la rejilla): the track's SHIFT, SWING on the odd steps, and
// the step's own micro-timing. Negative = early.
inline float step_offset(const Track &t, int step, int len) {
    float off = (t.p[EPK_P_SHIFT] - 0.5f) * 0.9f;
    if (step & 1) off += t.p[EPK_P_SWING] * 0.4f;
    int micro = t.st.meta[unrotated(t, step, len)] >> 5 & 7;
    if (micro > 3) micro -= 8;
    off += (float)micro * 0.125f;
    return off < -0.9f ? -0.9f : off > 0.9f ? 0.9f : off;
}

// Does this step play on this turn of the cycle? Its condition and its chance decide (condición y azar)
inline int step_passes(Track &t, int step, uint32_t cycle, int len) {
    if (t.absent) return 0;
    const uint8_t meta = t.st.meta[unrotated(t, step, len)];
    const int prob = meta & 3, cond = meta >> 2 & 7;
    if (cond == EPK_COND_1_2 || cond == EPK_COND_2_2) {
        if ((int)(cycle & 1u) != cond - EPK_COND_1_2) return 0;
    } else if (cond >= EPK_COND_1_4) {
        if ((int)(cycle & 3u) != cond - EPK_COND_1_4) return 0;
    }
    return !(prob && t.chance.unipolar() >= 1.0f - 0.25f * (float)prob);
}

inline void trigger_step(Track &t, int step) {
    const int note = t.key_note >= 0 ? t.key_note : note_live(t);
    t.voice.trigger(note_hz(note), step == 0 ? 1.0f : 0.8f);         // the first step accented (acento)
    t.triggers++;
}

// The next step (siguiente paso). A step on the grid triggers now; a late one is left pending inside this step;
// an early one was left pending by the step before it, so here the next step's is prepared. first: the sequencer
// just started, nothing came before, so an early step plays now.
void advance(Track &t, int len, uint32_t mask, int first) {
    t.step = (t.step + 1) % len;
    if (t.step == 0) t.cycle++;
    t.pend[0].on = t.pend[1].on = 0;
    if (mask >> t.step & 1u) {
        const float off = step_offset(t, t.step, len);
        if ((off > -0.01f || first) && step_passes(t, t.step, t.cycle, len)) {
            if (off > 0.01f) {
                t.pend[0].at = (uint32_t)(off * 4294967296.0f); t.pend[0].step = t.step; t.pend[0].on = 1;
            } else trigger_step(t, t.step);
        }
    }
    const int next = (t.step + 1) % len;
    if (mask >> next & 1u) {
        const float off = step_offset(t, next, len);
        if (off <= -0.01f && step_passes(t, next, t.cycle + (next == 0 ? 1u : 0u), len)) {
            t.pend[1].at = (uint32_t)((1.0f + off) * 4294967296.0f); t.pend[1].step = next; t.pend[1].on = 1;
        }
    }
}

// The sequencer for one track over a block: the voice is rendered in pieces, each trigger at its own sample.
void render_track(Track &t, float *l, float *r, uint32_t n, int solo) {
    const int len = param_int(t, EPK_P_LENGTH);
    const uint32_t mask = rotate_mask(base_mask(t, len), len, rot_live(t));
    const float steps_per_sample = g.bpm * (1.0f / 60.0f) * kStepsPerBeat[param_int(t, EPK_P_DIVISION)] / g.sr;
    const uint32_t inc = (uint32_t)(steps_per_sample * 4294967296.0f);   // Q32 per sample (por muestra)
    sync_engine(t);
    if (t.step >= len) t.step = len - 1;                             // the length was shortened (se acortó)
    uint32_t done = 0;
    while (done < n) {
        if (g.playing && t.fire) {                                   // play: step 0 now (paso 0 ya)
            t.fire = 0;
            t.step = -1;
            advance(t, len, mask, 1);
        }
        uint32_t run = n - done, until = 0xFFFFFFFFu;
        if (g.playing) {                                             // samples until the next step (hasta el paso)
            const uint32_t remain = t.pos ? 0u - t.pos : 0xFFFFFFFFu;
            until = remain / inc + (remain % inc ? 1u : 0u);
            if (until < run) run = until;
            for (Track::Pending &pd : t.pend) {                      // and until a trigger off the grid
                if (!pd.on) continue;
                if (t.pos >= pd.at) { pd.on = 0; trigger_step(t, pd.step); continue; }
                const uint32_t gap = pd.at - t.pos, d = gap / inc + (gap % inc ? 1u : 0u);
                if (d < run) run = d;
            }
        }
        t.voice.render(g.buf + done, run, t.pm, g.sr);
        done += run;
        if (g.playing) {
            t.pos += run * inc;                                      // wraps past 2^32 at the step's end
            if (run == until) advance(t, len, mask, 0);
        }
    }
    const float target = (t.muted || (solo && !t.soloed)) ? 0.0f : 1.0f;
    const float lvl = t.pm[EPK_P_LEVEL] * t.pm[EPK_P_LEVEL];        // a squared taper (curva cuadrática)
    if (t.lvl_s < 0.0f) t.lvl_s = lvl;
    const float pan = t.pm[EPK_P_PAN];                               // pm: the track LFO may move them (lo mueve)
    const float send_a = t.pm[EPK_P_SEND_A] * t.pm[EPK_P_SEND_A], send_b = t.pm[EPK_P_SEND_B] * t.pm[EPK_P_SEND_B];
    const float throw_target = t.thrown ? 1.0f : 0.0f;
    const int is_kick = &t == &g.tr[0];
    const float gl = fm_cosf(pan * 1.5707963f), gr = fm_sinf(pan * 1.5707963f);   // equal power (potencia constante)
    // MAREA on this track: the voice goes through it and comes out stereo; PAN works as a balance (balance)
    // FLOW: MAREA off glides out (~10 ms) and then is not computed (no se calcula)
    const float marea_t = g.fx_live[EPK_FX_ON_MAREA] > 0.5f ? 1.0f : 0.0f, marea_step = 1.0f / (0.010f * g.sr);
    const int ins = (int)(g.fx_live[EPK_FX_MAREA_TRACK] * 6.0f + 0.5f) == (int)(&t - g.tr) + 1 &&
                    (marea_t > 0.0f || g.marea_g > 0.0f);
    if (ins) {
        if (marea_t > 0.0f && g.marea_g <= 0.0f) g.marea.clear();    // back on: from empty (vuelve vacía)
        g.marea.process(g.buf, g.ins_l, g.ins_r, n, g.fx_live, g.bpm, g.sr);
    }
    float peak = t.meter;
    for (uint32_t i = 0; i < n; i++) {
        t.gain += (target - t.gain) * 0.003f;                        // ~8 ms: mutes do not click
        t.lvl_s += (lvl - t.lvl_s) * 0.003f;                         // a modulated level glides (sin escalones)
        float s = g.buf[i] * t.lvl_s * t.gain;
        if (ins) {
            const float k = t.lvl_s * t.gain, mg = g.marea_g, dry = g.buf[i] * (1.0f - mg);
            g.marea_g = mg < marea_t ? fm_minf(mg + marea_step, marea_t) : fm_maxf(mg - marea_step, marea_t);
            const float wl = g.ins_l[i] * mg + dry, wr = g.ins_r[i] * mg + dry;
            s = (wl + wr) * 0.5f * k;
            l[i] += wl * k * gl;                                     // the same law as a mono track
            r[i] += wr * k * gr;
        } else {
            l[i] += s * gl;
            r[i] += s * gr;
        }
        t.throw_g += (throw_target - t.throw_g) * 0.003f;            // the throw glides in: no click (sin clic)
        g.bus_a[i] += s * (send_a + (1.0f - send_a) * t.throw_g);    // after the fader (tras el fader)
        if (is_kick) g.kick[i] = s;
        g.bus_b[i] += s * send_b;
        peak = fm_maxf(peak, fm_fabsf(s));
    }
    t.meter = peak;
    float bp = 0.0f;                                                 // this block's peak, for the followers
    for (uint32_t i = 0; i < n; i++) bp = fm_maxf(bp, fm_fabsf((ins ? g.ins_l[i] : g.buf[i]) * t.lvl_s * t.gain));
    t.peak_block = bp;
}

inline int valid(int track) { return track >= 0 && track < EPK_TRACKS; }

}  // namespace epk

using namespace epk;

// ------------------------------------------------------------------ the C interface (interfaz C)
extern "C" {

void epk_init(float sample_rate) {
    // the project's mix (mezcla del proyecto): level, pan per track
    static const float kMix[EPK_TRACKS][4] = {{0.85f, 0.50f, 0.00f, 0.00f}, {0.50f, 0.58f, 0.30f, 0.30f},
                                              {0.60f, 0.40f, 0.65f, 0.55f}, {0.62f, 0.62f, 0.00f, 0.20f},
                                              {0.50f, 0.36f, 0.40f, 0.70f}, {0.60f, 0.57f, 0.40f, 0.50f}};
    // the send effects (efectos): a dotted-eighth tape echo into a long, dark plate
    static const float kFx[EPK_FX_COUNT] = {0.0f, 2.0f / 15.0f, 0.50f, 0.45f, 0.35f, 0.25f, 0.75f,
                                            0.72f, 0.45f, 0.40f, 0.75f, 0.35f,
                                            0.5f, 0.30f, 0.0f, 0.30f, 0.15f,    // master: open, glue, a little tape
                                            0.0f,                               // the DJ filter
                                            // MAREA on track 3, to be heard: all wet, ~3 s a cycle forwards, two
                                            // cycles, some ripple and pull, narrowish bands, full width
                                            3.0f / 6.0f, 1.0f, 0.75f, 0.0f, 0.25f, 0.35f, 0.0f, 1.0f, 0.35f,
                                            0.22f, 0.93f, 0.60f, 1.0f,
                                            1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,   // FLOW: all on (todo activo)
                                            0.0f, 0.0f, 0.0f};                  // filter on all, delay > reverb,
                                                                                // compressor > tape
    for (int i = 0; i < EPK_FX_COUNT; i++) g.fx[i] = kFx[i];
    g.sends.init();
    g.master.init();
    g.mod.init();
    g.marea.init();
    g.marea_on = 0;
    g.marea_g = 1.0f;
    for (int i = 0; i < EPK_FX_COUNT; i++) g.fx_live[i] = g.fx[i];
    g.meter_fx = 0.0f;
    g.sr = sample_rate;
    g.bpm = 124.0f;
    g.playing = 0;
    g.q_write = g.q_read = 0;
    g.pattern = 0; g.queued = -1;
    g.bar_pos = 0; g.bar_step = -1; g.bar_fire = 0;
    g.undo.ready = 0;
    g.queued_bank = 0;
    g.stage_bank = -1;
    g.evo_hold = 0;
    load_bank(0);
    for (int i = 0; i < EPK_TRACKS; i++) {
        Track &t = g.tr[i];
        for (int p = 0; p < EPK_P_PATTERN_COUNT; p++) t.p[p] = g.bank[0][i][p];
        t.st = g.bank_steps[0][i];
        t.p[EPK_P_LEVEL] = kMix[i][0];
        t.p[EPK_P_PAN] = kMix[i][1];
        t.p[EPK_P_SEND_A] = kMix[i][2];
        t.p[EPK_P_SEND_B] = kMix[i][3];
        t.muted = t.soloed = 0;
        t.step = -1; t.pos = 0; t.fire = 0; t.cycle = 0;
        t.pend[0].on = t.pend[1].on = 0;
        t.thrown = 0; t.throw_g = 0.0f;
        t.peak_block = 0.0f; t.lvl_s = -1.0f;
        t.gain = 1.0f; t.meter = 0.0f; t.triggers = 0; t.key_note = -1;
        t.chance.init(0x2545F491u * (uint32_t)(i + 1));
        t.evo.init(0x6C8E9CF5u * (uint32_t)(i + 3));
        t.evo_off = 0;
        t.lfo_out = t.lfo_last_ph = t.lfo_from = t.lfo_to = t.lfo_walk = 0.0f;
        t.lfo_hits = t.lfo_rot = 0;
        t.lfo_rng.init(0x51ED270Bu * (uint32_t)(i + 5));
        evo_reset(t);
        t.voice.init(param_int(t, EPK_P_ENGINE), 0x1234567u * (uint32_t)(i + 1));
    }
}

// The modulation for this block (la modulación de este bloque): the sources move, the effects and the tracks get
// their offsets. The followers hear the previous block (≤ 6 ms late).
// The track LFO over a block (un bloque): the global LFO's shapes, locked to the bar, and DRIFT, a slow random walk
// that bounces off the edges; its speed: how much it wanders in one cycle of RATE
inline void track_lfo(Track &t, float t16, uint32_t n) {
    const float amt = (t.p[EPK_P_LFO_AMT] - 0.5f) * 2.0f;
    const int shape = clampi((int)(t.p[EPK_P_LFO_SHAPE] * (EPK_TLFO_SHAPE_COUNT - 1) + 0.5f), 0, EPK_TLFO_SHAPE_COUNT - 1);
    const float len = kLfoSixteenths[clampi((int)(t.p[EPK_P_LFO_RATE] * (EPK_LFO_RATE_COUNT - 1) + 0.5f), 0,
                                            EPK_LFO_RATE_COUNT - 1)];
    const float cyc = t16 / len, ph = cyc - fm_floorf(cyc);
    const int fresh = ph < t.lfo_last_ph;
    t.lfo_last_ph = ph;
    float v;
    switch (shape) {
    case EPK_LFO_SINE: v = fm_sin_turns(ph); break;
    case EPK_LFO_TRIANGLE: v = ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph; break;
    case EPK_LFO_SAW: v = 1.0f - 2.0f * ph; break;
    case EPK_LFO_SQUARE: v = ph < 0.5f ? 1.0f : -1.0f; break;
    case EPK_LFO_SH:
        if (fresh) t.lfo_from = t.lfo_rng.bipolar();
        v = t.lfo_from;
        break;
    case EPK_LFO_RANDOM:
        if (fresh) { t.lfo_from = t.lfo_to; t.lfo_to = t.lfo_rng.bipolar(); }
        v = t.lfo_from + (t.lfo_to - t.lfo_from) * (ph * ph * (3.0f - 2.0f * ph));
        break;
    default: {                                                       // DRIFT (deriva)
        const float blk16 = (float)n * g.bpm * (1.0f / 60.0f) * 4.0f / g.sr;   // this block in sixteenths
        t.lfo_walk += t.lfo_rng.bipolar() * 1.7f * fm_sqrtf(blk16 / len);
        if (t.lfo_walk > 1.0f) t.lfo_walk = 2.0f - t.lfo_walk;
        if (t.lfo_walk < -1.0f) t.lfo_walk = -2.0f - t.lfo_walk;
        v = t.lfo_walk;
    }
    }
    t.lfo_out = v * amt;
}

void apply_mod(uint32_t n) {
    static const int8_t kFxOf[EPK_DEST_TONE1] = {-1, EPK_FX_FLT, EPK_FX_FLT_RESO, EPK_FX_DLY_TIME, EPK_FX_DLY_FEEDBACK,
                                                 EPK_FX_DLY_TONE, EPK_FX_DLY_LEVEL, EPK_FX_REV_DECAY, EPK_FX_REV_LEVEL,
                                                 EPK_FX_MAREA_SPEED, EPK_FX_MAREA_MULTIPLY, EPK_FX_MAREA_RIPPLE,
                                                 EPK_FX_MAREA_OFFSET, EPK_FX_MAREA_GRAVITY, EPK_FX_MAREA_MIX};
    float peaks[EPK_TRACKS];
    for (int i = 0; i < EPK_TRACKS; i++) peaks[i] = g.tr[i].peak_block;
    const float pos16 = g.playing && g.sixteenths >= 0 ? (float)g.sixteenths + (float)g.bar_pos * (1.0f / 4294967296.0f) : 0.0f;
    g.mod.tick(g.playing && g.sixteenths >= 0, pos16, n, g.bpm, g.sr, peaks);
    for (int i = 0; i < EPK_FX_COUNT; i++) g.fx_live[i] = g.fx[i];
    const int on = (int)(g.fx[EPK_FX_MAREA_TRACK] * 6.0f + 0.5f);
    if (on != g.marea_on) { g.marea.clear(); g.marea_on = on; }    // moved to another track: empty (vacía)
    for (int d = 1; d < EPK_DEST_TONE1; d++) {
        const float o = g.mod.off[d];
        if (o != 0.0f) g.fx_live[kFxOf[d]] = clamp01(g.fx[kFxOf[d]] + o);
    }
    const float t16 = g.playing && g.sixteenths >= 0 ? pos16 : g.mod.free_phase;
    for (int k = 0; k < EPK_TRACKS; k++) {
        Track &t = g.tr[k];
        track_lfo(t, t16, n);
        for (int p = 0; p < EPK_P_COUNT; p++) t.pm[p] = t.p[p];
        t.pm[EPK_P_TONE] = clamp01(t.p[EPK_P_TONE] + t.m_tone + g.mod.off[EPK_DEST_TONE1 + k]);
        t.pm[EPK_P_DECAY] = clamp01(t.p[EPK_P_DECAY] + t.m_decay);
        t.pm[EPK_P_CHARACTER] = clamp01(t.p[EPK_P_CHARACTER] + t.m_char);
        t.pm[EPK_P_NOTE] = (float)(note_live(t) - 24) / 60.0f;
        t.pm[EPK_P_LEVEL] = clamp01(t.p[EPK_P_LEVEL] + g.mod.off[EPK_DEST_LEVEL1 + k]);
        // the track LFO on top (encima): half the range at full amount; the rhythm in hits and steps
        static const int8_t kParamOf[EPK_TLFO_DEST_COUNT] = {EPK_P_TONE, EPK_P_DECAY, EPK_P_CHARACTER, EPK_P_EXTRA1,
                                                             EPK_P_EXTRA2, EPK_P_LEVEL, EPK_P_PAN, EPK_P_SEND_A,
                                                             EPK_P_SEND_B, -1, -1};
        const int dest = clampi((int)(t.p[EPK_P_LFO_DEST] * (EPK_TLFO_DEST_COUNT - 1) + 0.5f), 0, EPK_TLFO_DEST_COUNT - 1);
        t.lfo_hits = t.lfo_rot = 0;
        if (t.lfo_out != 0.0f) {
            const int len = param_int(t, EPK_P_LENGTH);
            if (dest == EPK_TLFO_HITS) t.lfo_hits = (int)fm_floorf(t.lfo_out * (float)len * 0.25f + 0.5f);
            else if (dest == EPK_TLFO_ROTATE) t.lfo_rot = (int)fm_floorf(t.lfo_out * (float)len * 0.5f + 0.5f);
            else t.pm[kParamOf[dest]] = clamp01(t.pm[kParamOf[dest]] + t.lfo_out * 0.5f);
        }
    }
}

void epk_render(float *left, float *right, uint32_t frames) {
    drain();
    const int place = clampi((int)(g.fx[EPK_FX_FLT_PLACE] * (EPK_PLACE_COUNT - 1) + 0.5f), 0, EPK_PLACE_COUNT - 1);
    const int solo = any_solo();
    for (uint32_t i = 0; i < frames; i++) left[i] = right[i] = 0.0f;
    // the bar clock (reloj de compás) in sixteenths: a queued pattern comes in on the sample its bar starts
    const uint32_t inc = (uint32_t)(g.bpm * (1.0f / 60.0f) * 4.0f / g.sr * 4294967296.0f);
    uint32_t done = 0;
    while (done < frames) {
        if (g.playing && g.bar_fire) {
            g.bar_fire = 0;
            g.bar_step = (g.bar_step + 1) % EPK_BAR_STEPS;
            g.sixteenths++;
            if (g.bar_step == 0 && g.queued >= 0) switch_pattern(g.queued, g.queued_bank);
            if (g.bar_step == 0) for (Track &t : g.tr) evolve(t);
        }
        uint32_t n = frames - done, until = 0xFFFFFFFFu;
        if (n > kBlock) n = kBlock;
        if (g.playing) {
            const uint32_t remain = g.bar_pos ? 0u - g.bar_pos : 0xFFFFFFFFu;
            until = remain / inc + (remain % inc ? 1u : 0u);
            if (until < n) n = until;
        }
        for (uint32_t i = 0; i < n; i++) g.bus_a[i] = g.bus_b[i] = g.kick[i] = g.fx_l[i] = g.fx_r[i] = 0.0f;
        apply_mod(n);
        for (Track &t : g.tr) render_track(t, left + done, right + done, n, solo);
        g.sends.process(g.bus_a, g.bus_b, g.kick, g.fx_l, g.fx_r, n, g.fx_live, g.bpm, g.sr);
        if (place == EPK_PLACE_DRY) g.master.filter(left + done, right + done, n, g.fx_live, g.sr);   // FLOW
        else if (place == EPK_PLACE_FX) g.master.filter(g.fx_l, g.fx_r, n, g.fx_live, g.sr);
        float fx_peak = g.meter_fx;
        for (uint32_t i = 0; i < n; i++) {                            // the returns (retornos)
            left[done + i] += g.fx_l[i];
            right[done + i] += g.fx_r[i];
            fx_peak = fm_maxf(fx_peak, fm_maxf(fm_fabsf(g.fx_l[i]), fm_fabsf(g.fx_r[i])));
        }
        g.meter_fx = fx_peak;
        done += n;
        if (g.playing) {
            g.bar_pos += n * inc;
            if (n == until) g.bar_fire = 1;
        }
    }
    if (place == EPK_PLACE_ALL) g.master.filter(left, right, frames, g.fx_live, g.sr);   // on everything (en todo)
    g.master.dynamics(left, right, frames, g.fx_live, g.sr);         // compressor and tape (comp y cinta)
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
    g.sends.clear();                                                 // and the echoes and the tail (ecos y cola)
    g.master.init();
}

void epk_fx_set(int param, float value01) { if (param >= 0 && param < EPK_FX_COUNT) g.fx[param] = clamp01(value01); }
float epk_fx_get(int param) { return param >= 0 && param < EPK_FX_COUNT ? g.fx[param] : 0.0f; }
int epk_fx_steps(int param) {
    return param == EPK_FX_DLY_TYPE ? EPK_DLY_COUNT : param == EPK_FX_DLY_TIME ? 16 : param == EPK_FX_FLT_TYPE ? EPK_FLT_COUNT :
           param == EPK_FX_MAREA_TRACK ? EPK_TRACKS + 1 : param == EPK_FX_MAREA_SYNC ? 2 :
           (param >= EPK_FX_ON_MAREA && param <= EPK_FX_ON_SAT) ? 2 : param == EPK_FX_FLT_PLACE ? EPK_PLACE_COUNT :
           param == EPK_FX_SPACE_ORDER || param == EPK_FX_DRIVE_ORDER ? 2 : 0;
}
float epk_marea_band(int b) { return g.marea.gain[clampi(b, 0, kMareaBands - 1)]; }
float epk_marea_x(float hz) { return clamp01(fm_log2f(fm_maxf(hz, 1.0f) / kMareaLow) / kMareaOctaves); }
float epk_marea_hz(int band) { return marea_band_hz(clampi(band, 0, kMareaBands - 1)); }
float epk_marea_edge_hz(float v01) { return marea_edge_hz(v01); }
float epk_marea_speed(void) {
    return marea_speed_hz(g.fx_live[EPK_FX_MAREA_SPEED], g.fx_live[EPK_FX_MAREA_SYNC] > 0.5f, g.bpm);
}
float epk_fx_meter(void) { float m = g.meter_fx; g.meter_fx = 0.0f; return m; }
void epk_mod_set(int src, int param, float value01) {
    if (src >= 0 && src < EPK_MOD_SOURCES && param >= 0 && param < EPK_M_COUNT) g.mod.m[src][param] = clamp01(value01);
}
float epk_mod_get(int src, int param) {
    return src >= 0 && src < EPK_MOD_SOURCES && param >= 0 && param < EPK_M_COUNT ? g.mod.m[src][param] : 0.0f;
}
int epk_mod_steps(int src, int param) { return Mod::steps(src, param); }
float epk_mod_out(int src) { return src >= 0 && src < EPK_MOD_SOURCES ? g.mod.s[src].out : 0.0f; }
float epk_fx_live(int param) { return param >= 0 && param < EPK_FX_COUNT ? g.fx_live[param] : 0.0f; }
float epk_param_live(int track, int param) { return valid(track) && param >= 0 && param < EPK_P_COUNT ? g.tr[track].pm[param] : 0.0f; }
int epk_chord_tables(void) { return kChordTables; }
int epk_chord_count(int table) { return kChordCount[clampi(table, 0, kChordTables - 1)]; }
const char *epk_chord_table_name(int table) { return kChordTableName[clampi(table, 0, kChordTables - 1)]; }
const char *epk_chord_name(int table, int chord) {
    const int t = clampi(table, 0, kChordTables - 1);
    return kChordName[t][clampi(chord, 0, kChordCount[t] - 1)];
}
float epk_comp_reduction(void) { const float r = g.master.gr_max; g.master.gr_max = 0.0f; return r; }
float epk_master_cutoff(void) { return filter_cut_hz(g.fx[EPK_FX_FLT]); }
int epk_master_filter(float *hz) {
    const float v = g.fx[EPK_FX_FLT];
    if (v < 0.47f) { *hz = filter_lp_hz(v); return 1; }
    if (v > 0.53f) { *hz = filter_hp_hz(v); return 2; }
    *hz = 0.0f;
    return 0;
}
void epk_throw(int track, int on) { if (valid(track)) g.tr[track].thrown = on ? 1 : 0; }
int epk_thrown(int track) { return valid(track) ? g.tr[track].thrown : 0; }
void epk_freeze(int on) { g.sends.freeze = on ? 1 : 0; }
int epk_frozen(void) { return g.sends.freeze; }

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
    case EPK_P_EVERY: return 5;
    case EPK_P_SCOPE: return EPK_SCOPE_COUNT;
    case EPK_P_LFO_SHAPE: return EPK_TLFO_SHAPE_COUNT;
    case EPK_P_LFO_RATE: return EPK_LFO_RATE_COUNT;
    case EPK_P_LFO_DEST: return EPK_TLFO_DEST_COUNT;
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
void epk_step_set(int track, int step, int prob, int cond, int micro) {
    if (!valid(track) || step < 0 || step >= EPK_MAX_STEPS) return;
    push(EV_STEP_META, track, step, clampi(prob, 0, EPK_PROB_COUNT - 1) | clampi(cond, 0, EPK_COND_COUNT - 1) << 2 |
                                    (clampi(micro, -3, 3) & 7) << 5);
}
static int step_meta(int track, int step) {
    if (!valid(track)) return 0;
    const Track &t = g.tr[track];
    const int len = param_int(t, EPK_P_LENGTH);
    return step >= 0 && step < len ? t.st.meta[unrotated(t, step, len)] : 0;
}
int epk_step_prob(int track, int step) { return step_meta(track, step) & 3; }
int epk_step_cond(int track, int step) { return step_meta(track, step) >> 2 & 7; }
int epk_step_micro(int track, int step) { const int m = step_meta(track, step) >> 5 & 7; return m > 3 ? m - 8 : m; }
int epk_steps_manual(int track) { return valid(track) ? g.tr[track].st.manual : 0; }
void epk_steps_euclid(int track) { if (valid(track)) push(EV_EUCLID, track, 0, 0); }

void epk_undo_save(void) {
    g.undo.ready = 0;
    for (int k = 0; k < EPK_TRACKS; k++) {
        const Track &t = g.tr[k];
        for (int p = 0; p < EPK_P_PATTERN_COUNT; p++) g.undo.p[k][p] = t.p[p];
        g.undo.mix[k][0] = t.p[EPK_P_PAN];
        g.undo.mix[k][1] = t.p[EPK_P_SEND_A];
        g.undo.mix[k][2] = t.p[EPK_P_SEND_B];
        g.undo.st[k] = t.st;
    }
    g.undo.pattern = g.pattern;
    g.undo.bank = g.bank_id;
    g.undo.key = g.key;
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
    if (pattern < 0 || pattern >= EPK_PATTERNS || bank < 0 || bank >= EPK_BANKS) return;
    if (g.stage_bank == -2 - bank) g.stage_bank = bank;              // what was staged for it, ready (listo)
    push(EV_PATTERN, 0, pattern, bank);
}
int epk_pattern_current(void) { return g.pattern; }
int epk_pattern_queued(void) { return g.queued; }
int epk_bank_current(void) { return g.bank_id; }
int epk_bank_queued(void) { return g.queued >= 0 ? g.queued_bank : -1; }
const char *epk_pattern_name(int bank, int pattern) {
    static char name[16];
    if (pattern < 0 || pattern >= EPK_PATTERNS || bank < 0 || bank >= EPK_BANKS) return "";
    int n = 0;
    if (bank == 0)
        for (const char *c = kFactoryNames[pattern]; *c && n < 12; c++) name[n++] = *c;
    else {
        name[0] = 'G'; name[1] = 'E'; name[2] = 'N'; name[3] = ' ';
        name[4] = (char)('A' + bank);
        name[5] = (char)('0' + (pattern + 1) / 10);
        name[6] = (char)('0' + (pattern + 1) % 10);
        n = 7;
    }
    if (bank == g.bank_id && (g.user_mask >> pattern & 1u)) name[n++] = '*';   // saved (guardado)
    name[n] = 0;
    return name;
}
int epk_every_bars(int track) { return valid(track) ? every_bars(g.tr[track]) : 1; }
int epk_absent(int track) { return valid(track) && g.playing ? g.tr[track].absent : 0; }
void epk_evolve_enable(int track, int on) { if (valid(track)) g.tr[track].evo_off = on ? 0 : 1; }
int epk_evolve_enabled(int track) { return valid(track) ? !g.tr[track].evo_off : 0; }
float epk_track_lfo(int track) { return valid(track) ? g.tr[track].lfo_out : 0.0f; }
void epk_evolve_hold(int on) { g.evo_hold = on ? 1 : 0; }
void epk_random_key_lock(int on) { g.key_lock = on ? 1 : 0; }
int epk_random_key_locked(void) { return g.key_lock; }
int epk_key(void) { return ((2 + g.key) % 12 + 12) % 12; }
int epk_evolve_held(void) { return g.evo_hold; }
// Older versions had fewer parameters per track (menos parámetros), the newest last: 1 without SCOPE and the track
// LFO, 2 without the track LFO. Each record grows by their defaults (sus valores de inicio)
int epk_bank_upgrade(const uint8_t *in, int len, uint8_t *out, int max) {
    if (len < 5 || max < EPK_BANK_OBJ_BYTES || in[0] != 'E' || in[1] != 'P' || in[2] != 'K' || in[3] != 'B') return 0;
    const int missing = in[4] == 1 ? 5 : in[4] == 2 ? 4 : -1;
    if (missing < 0) return 0;
    const int n_old = EPK_P_PATTERN_COUNT - missing, rec_old = (n_old * 2 + 37) * EPK_TRACKS;
    if (len < EPK_BANK_HEAD + 8 * rec_old) return 0;
    float tail[5];
    tail[0] = 0.0f;                                                  // SCOPE: all (todo)
    for (int q = 0; q < 4; q++) tail[1 + q] = kLfoDefault[q];
    const float *add = tail + (5 - missing);
    for (int i = 0; i < EPK_BANK_HEAD; i++) out[i] = in[i];
    out[4] = EPK_BANK_VERSION;
    const uint8_t *src = in + EPK_BANK_HEAD;
    uint8_t *dst = out + EPK_BANK_HEAD;
    for (int r = 0; r < 8 * EPK_TRACKS; r++) {                      // old: every parameter in two bytes
        float p[EPK_P_PATTERN_COUNT];
        for (int q = 0; q < n_old; q++, src += 2) p[q] = (float)(src[0] | src[1] << 8) * (1.0f / 65535.0f);
        for (int q = 0; q < missing; q++) p[n_old + q] = add[q];
        pack_params(p, dst);
        for (int i = 0; i < 37; i++) *dst++ = *src++;
    }
    return EPK_BANK_OBJ_BYTES;
}

void epk_pattern_pack_live(uint8_t *out) {
    float b[EPK_P_PATTERN_COUNT];
    Steps st;
    for (int i = 0; i < EPK_TRACKS; i++) {
        bake_track(i, b, st);
        pack_track(b, st, out);
    }
}
void epk_pattern_keep(int pattern) { if (pattern >= 0 && pattern < EPK_PATTERNS) push(EV_KEEP, 0, pattern, 0); }
void epk_bank_stage_begin(int bank) {
    g.stage_bank = -1;
    g.stage_mask = 0;
    (void)bank;
}
void epk_bank_stage_half(int bank, int half, const uint8_t *obj, int len) {
    if (len < EPK_BANK_OBJ_BYTES || obj[0] != 'E' || obj[1] != 'P' || obj[2] != 'K' || obj[3] != 'B' ||
        obj[4] != EPK_BANK_VERSION || obj[5] != bank || obj[6] != half || half < 0 || half > 1)
        return;                                                      // not ours, or an older layout (otro formato)
    for (int k = 0; k < 8; k++)
        if (obj[7] >> k & 1u) {
            const uint8_t *src = obj + EPK_BANK_HEAD + k * EPK_PATTERN_BYTES;
            for (int i = 0; i < EPK_PATTERN_BYTES; i++) g.stage[half * 8 + k][i] = src[i];
            g.stage_mask |= (uint16_t)(1u << (half * 8 + k));
        }
    g.stage_bank = -2 - bank;                                        // ready once both halves are in (abajo)
}
void epk_bank_restage(void) {
    if (g.stage_bank <= -2) g.stage_bank = -2 - g.stage_bank;
    load_bank(g.bank_id);
    load_live(g.pattern, 0);
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
