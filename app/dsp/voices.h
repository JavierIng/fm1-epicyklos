// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Javier Sánchez
// EPICYKLOS voices (motores de sonido): TEST (prueba), GRAVE (kick + rumble), METAL (808 / 909 hats and rides),
// GOLPE (clap, snare, rim), TAMBOR (toms, congas, metallic percussion), ACORDE DUB (chords), DRONE, MACRO (six models).
// Every voice is plain data with init / trigger / render; parameters arrive 0..1 (p[EPK_P_*]).
// Rules: no heap, float only, no 64-bit multiplies.
#pragma once
#include "epk.h"
#include "fastmath.h"
#include "chords.h"

namespace epk {

inline float clamp01(float x) { return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x; }
inline int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

struct Rng {                               // xorshift32 (ruido)
    uint32_t s;
    void init(uint32_t seed) { s = seed ? seed : 0x9E3779B9u; }
    float bipolar() {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float)(int32_t)s * (1.0f / 2147483648.0f);
    }
    float unipolar() { return bipolar() * 0.5f + 0.5f; }
};

// ------------------------------------------------------------------ TEST (voz de prueba)
// One oscillator morphing sine (with a pitch drop) -> saw -> noise, a low-pass, an attack (ATTACK: 1 ms .. 1.4 s)
// and an exponential decay that starts when the attack ends.
struct TestVoice {
    float phase, freq, pitch_env, amp, attack;
    float ic1, ic2;
    Rng rng;

    void init(uint32_t seed) {
        phase = 0.0f; freq = 110.0f; pitch_env = 0.0f; amp = 0.0f; attack = 1.0f;
        ic1 = ic2 = 0.0f;
        rng.init(seed);
    }
    void trigger(float hz, float velocity) { freq = hz; pitch_env = 1.0f; amp = velocity; attack = 0.0f; }
    void render(float *out, uint32_t n, const float *p, float sr) {
        const float character = p[EPK_P_CHARACTER];
        const float cutoff = 40.0f * fm_exp2f(p[EPK_P_TONE] * 8.6f);              // 40 Hz .. ~16 kHz
        const float g = fm_tanf(3.14159265f * fm_minf(cutoff, sr * 0.45f) / sr);
        const float k = 1.2f;
        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float decay_s = 0.01f * fm_exp2f(p[EPK_P_DECAY] * 7.6f);            // 10 ms .. ~2 s
        const float amp_mul = fm_expf(-6.9f / (decay_s * sr));
        const float pitch_mul = fm_expf(-1.0f / (0.035f * sr));
        const float attack_inc = 1.0f / (0.001f * fm_exp2f(p[EPK_P_ATTACK] * 10.5f) * sr);
        const float drop = clamp01(1.0f - character * 3.0f) * 3.0f;
        const float to_saw = clamp01(character * 2.0f);
        const float to_noise = clamp01(character * 2.0f - 1.0f);
        for (uint32_t i = 0; i < n; i++) {
            const float inc = freq * fm_exp2f(drop * pitch_env) / sr;
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
            out[i] = v2 * amp * attack;
            if (attack < 1.0f) attack = fm_minf(attack + attack_inc, 1.0f);       // the decay waits for the attack
            else amp = fm_flush(amp * amp_mul);
            pitch_env *= pitch_mul;
        }
    }
};

// ------------------------------------------------------------------ GRAVE (bombo + rumble)
// A sine with a pitch sweep (barrido), a click, drive; and a rumble: the kick through a dark diffusion loop
// (three all-passes and a delay, low-passed, run at a quarter of the sample rate) ducked by the kick itself.
// Parameters: NOTE tune, CHARACTER drive, TONE click, DECAY, EXTRA1 sweep, EXTRA2 rumble.
struct Grave {
    static constexpr int kAp0 = 149, kAp1 = 211, kAp2 = 307, kDelay = 613;      // at sr / 4

    float phase, freq, pitch_env, amp, attack, click_env, noise_lp;
    Rng rng;
    // rumble (cola grave)
    float ap0[kAp0], ap1[kAp1], ap2[kAp2], dl[kDelay];
    int i0, i1, i2, id, sub;                // write indexes; sub: 0..3 within the quarter-rate tick
    float acc, loop_lp, loop_hp, hp_state, wet_prev, wet_now;

    void init(uint32_t seed) {
        phase = 0.0f; freq = 55.0f; pitch_env = 0.0f; amp = 0.0f; attack = 1.0f; click_env = 0.0f; noise_lp = 0.0f;
        rng.init(seed);
        for (float &v : ap0) v = 0.0f;
        for (float &v : ap1) v = 0.0f;
        for (float &v : ap2) v = 0.0f;
        for (float &v : dl) v = 0.0f;
        i0 = i1 = i2 = id = sub = 0;
        acc = loop_lp = loop_hp = hp_state = wet_prev = wet_now = 0.0f;
    }
    void trigger(float hz, float velocity) {
        freq = hz; phase = 0.0f; pitch_env = 1.0f; amp = velocity; attack = 0.0f; click_env = velocity;
    }
    static inline float allpass(float *buf, int &idx, int len, float x) {
        const float d = buf[idx];
        const float y = d - 0.6f * x;
        buf[idx] = fm_flush(x + 0.6f * y);
        if (++idx >= len) idx = 0;
        return y;
    }
    void render(float *out, uint32_t n, const float *p, float sr) {
        const float drive = 1.0f + p[EPK_P_CHARACTER] * 9.0f;
        const float drive_norm = 1.0f / fm_tanhf(drive);
        const float click = p[EPK_P_TONE] * 0.7f;
        const float decay_s = 0.06f * fm_exp2f(p[EPK_P_DECAY] * 4.6f);            // 60 ms .. ~1.5 s
        const float amp_mul = fm_expf(-6.9f / (decay_s * sr));
        const float pitch_mul = fm_expf(-1.0f / (0.028f * sr));
        const float click_mul = fm_expf(-1.0f / (0.003f * sr));
        const float sweep = p[EPK_P_EXTRA1] * 4.0f;                               // octaves (octavas)
        const float rumble = p[EPK_P_EXTRA2];
        const float fb = 0.45f + 0.35f * rumble;
        const float wet_gain = rumble * rumble * 3.0f;
        for (uint32_t i = 0; i < n; i++) {
            phase += freq * fm_exp2f(sweep * pitch_env) / sr;
            if (phase >= 1.0f) phase -= 1.0f;
            const float nz = rng.bipolar();
            noise_lp += (nz - noise_lp) * 0.3f;
            float s = fm_sin_turns(phase) * amp + (nz - noise_lp) * click_env * click;
            s = fm_tanhf(s * drive) * drive_norm;
            attack = fm_minf(attack + 0.05f, 1.0f);
            s *= attack;
            amp = fm_flush(amp * amp_mul);
            pitch_env *= pitch_mul;
            click_env = fm_flush(click_env * click_mul);
            acc += s;                                                             // the rumble, every 4th sample
            if (++sub == 4) {
                sub = 0;
                float x = acc * 0.25f + fb * loop_hp;
                acc = 0.0f;
                x = allpass(ap0, i0, kAp0, x);
                x = allpass(ap1, i1, kAp1, x);
                x = allpass(ap2, i2, kAp2, x);
                const float d = dl[id];
                dl[id] = fm_flush(x);
                if (++id >= kDelay) id = 0;
                loop_lp = fm_flush(loop_lp + (d - loop_lp) * 0.065f);             // ~115 Hz at sr / 4
                hp_state = fm_flush(hp_state + (loop_lp - hp_state) * 0.02f);     // ~35 Hz: no build-up below
                loop_hp = loop_lp - hp_state;
                wet_prev = wet_now;
                wet_now = fm_tanhf(loop_lp * wet_gain);
            }
            const float wet = wet_prev + (wet_now - wet_prev) * ((float)sub * 0.25f);
            const float duck = 1.0f - fm_minf(amp * 1.3f, 1.0f);                  // the kick opens room (el bombo manda)
            out[i] = s + wet * duck * 0.8f;
        }
    }
};

// ------------------------------------------------------------------ METAL (hats, rides)
// Six detuned squares at the 808's ratios against noise (the 909 side), a band-pass and a high-pass, a decay.
// Parameters: NOTE tune, CHARACTER noise..metal, TONE brightness, DECAY, EXTRA1 ring (resonance), EXTRA2 vary
// (each hit's length varies: closed and open by chance).
struct Metal {
    float ph[6], tune, amp, amp_mul_hit, attack;
    float ic1, ic2;
    Rng rng;

    void init(uint32_t seed) {
        for (float &v : ph) v = 0.0f;
        tune = 1.0f; amp = 0.0f; amp_mul_hit = 1.0f; attack = 1.0f;
        ic1 = ic2 = 0.0f;
        rng.init(seed);
    }
    void trigger(float hz, float velocity) {
        tune = hz * (1.0f / 1174.66f);                                            // D6 plays the 808's own tuning
        amp = velocity;
        attack = 0.0f;
        amp_mul_hit = rng.bipolar();                                              // this hit's variation (-1..1)
    }
    void render(float *out, uint32_t n, const float *p, float sr) {
        static const float kHz[6] = {205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f};
        float inc[6];
        for (int k = 0; k < 6; k++) inc[k] = fm_minf(kHz[k] * tune / sr, 0.45f);
        const float metal = p[EPK_P_CHARACTER];
        const float fc = 2000.0f * fm_exp2f(p[EPK_P_TONE] * 2.3f);                // 2 k .. 9.8 k
        const float g = fm_tanf(3.14159265f * fm_minf(fc, sr * 0.4f) / sr);
        const float k_res = 1.6f - 1.3f * p[EPK_P_EXTRA1];
        const float a1 = 1.0f / (1.0f + g * (g + k_res)), a2 = g * a1, a3 = g * a2;
        const float decay_s = 0.015f * fm_exp2f(p[EPK_P_DECAY] * 6.5f + amp_mul_hit * p[EPK_P_EXTRA2] * 2.5f);
        const float amp_mul = fm_expf(-6.9f / (decay_s * sr));
        for (uint32_t i = 0; i < n; i++) {
            float sq = 0.0f;
            for (int k = 0; k < 6; k++) {                                         // polyBLEP squares (cuadradas)
                float ph_k = ph[k] + inc[k];
                if (ph_k >= 1.0f) ph_k -= 1.0f;
                ph[k] = ph_k;
                float v = ph_k < 0.5f ? 1.0f : -1.0f;
                const float d = inc[k];
                if (ph_k < d) { float t = ph_k / d; v += t + t - t * t - 1.0f; }
                else if (ph_k > 1.0f - d) { float t = (ph_k - 1.0f) / d; v += t * t + t + t + 1.0f; }
                float h = ph_k + 0.5f;
                if (h >= 1.0f) h -= 1.0f;
                if (h < d) { float t = h / d; v -= t + t - t * t - 1.0f; }
                else if (h > 1.0f - d) { float t = (h - 1.0f) / d; v -= t * t + t + t + 1.0f; }
                sq += v;
            }
            sq *= (1.0f / 6.0f);
            const float nz = rng.bipolar();
            const float s = nz * 0.6f + (sq - nz * 0.6f) * metal;
            const float v3 = s - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = fm_flush(2.0f * v1 - ic1);
            ic2 = fm_flush(2.0f * v2 - ic2);
            const float hp = s - k_res * v1 - v2;
            attack = fm_minf(attack + 0.1f, 1.0f);
            out[i] = fm_tanhf((v1 * 3.2f + hp * 2.4f) * amp) * attack;
            amp = fm_flush(amp * amp_mul);
        }
    }
};

// ------------------------------------------------------------------ GOLPE (palma, caja, aro)
// Noise through a band-pass with its own envelope, and a tuned body. CHARACTER goes from clap (three quick bursts
// and a tail, no body) through snare (body + noise) to rim (a short, higher body, almost no noise).
// Parameters: NOTE tune (the body, and the noise band with it), CHARACTER type, TONE brightness, DECAY, EXTRA1
// body (cuerpo: the tuned part; under a clap, a low thump), EXTRA2 tail (cola: the noise's length, and how far
// apart the clap's bursts are).
struct Golpe {
    float ph, ph2, freq, vel, pitch_env, body, burst, tail;
    float ic1, ic2;
    int bursts_left, gap_count, fresh;
    Rng rng;

    void init(uint32_t seed) {
        ph = ph2 = 0.0f; freq = 165.0f; vel = 0.0f; pitch_env = 0.0f; body = burst = tail = 0.0f;
        ic1 = ic2 = 0.0f;
        bursts_left = gap_count = fresh = 0;
        rng.init(seed);
    }
    void trigger(float hz, float velocity) { freq = hz; vel = velocity; fresh = 1; }
    void render(float *out, uint32_t n, const float *p, float sr) {
        const float type = p[EPK_P_CHARACTER];
        const float clap = clamp01(1.0f - type * 2.5f), rim = clamp01(type * 2.5f - 1.5f);
        const float snare = 1.0f - clap - rim;
        const float tail_amt = p[EPK_P_EXTRA2];
        if (fresh) {                                                              // a new hit (golpe nuevo)
            fresh = 0;
            ph = ph2 = 0.0f; pitch_env = 1.0f; body = vel;
            if (clap > 0.05f) { bursts_left = 3; gap_count = 0; burst = tail = 0.0f; }
            else { bursts_left = 0; burst = tail = vel; }
        }
        const int gap = (int)((0.006f + 0.022f * tail_amt) * sr);
        const float tune = fm_sqrtf(freq * (1.0f / 165.0f));                      // AFINA moves the noise too
        const float fc = 350.0f * fm_exp2f(p[EPK_P_TONE] * 4.6f) * tune;          // 350 Hz .. 8.5 kHz
        const float g = fm_tanf(3.14159265f * fm_minf(fc, sr * 0.4f) / sr);
        const float k = 0.45f + 0.55f * snare;                                    // the clap's band is narrower
        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float burst_mul = fm_expf(-1.0f / (0.004f * sr));
        const float tail_s = 0.02f * fm_exp2f(p[EPK_P_DECAY] * 6.0f) * (0.3f + 1.4f * tail_amt);   // up to ~2 s
        const float tail_mul = fm_expf(-6.9f / (tail_s * sr));
        const float body_s = 0.04f * fm_exp2f(p[EPK_P_DECAY] * 3.5f) * (1.0f - rim) + 0.012f * fm_exp2f(p[EPK_P_DECAY] * 2.0f) * rim;
        const float body_mul = fm_expf(-6.9f / (body_s * sr));
        const float pitch_mul = fm_expf(-1.0f / (0.012f * sr));
        const float body_lvl = (snare * 1.6f + clap * 0.9f) * p[EPK_P_EXTRA1] + rim * (0.35f + 0.95f * p[EPK_P_EXTRA1]);
        const float noise_lvl = (clap * 1.1f + snare * 0.9f) * (1.3f - 0.6f * p[EPK_P_EXTRA1]) + rim * 0.3f * tail_amt;
        const float hp_lvl = 0.35f * (1.0f - clap) * p[EPK_P_TONE];
        const float f_body = freq * (1.0f + 1.4f * rim) / sr, drop = 0.1f + 0.9f * snare;
        for (uint32_t i = 0; i < n; i++) {
            if (bursts_left > 0) {                                                // the clap's bursts (ráfagas)
                if (gap_count <= 0) {
                    burst = vel; gap_count = gap;
                    if (--bursts_left == 0) tail = vel;
                } else gap_count--;
            }
            const float nz = rng.bipolar();
            const float v3 = nz - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = fm_flush(2.0f * v1 - ic1);
            ic2 = fm_flush(2.0f * v2 - ic2);
            const float hp = nz - k * v1 - v2;
            const float noise = (v1 * 1.6f + hp * hp_lvl) * fm_maxf(burst, tail);
            const float inc = f_body * (1.0f + drop * pitch_env);
            ph += inc;
            if (ph >= 1.0f) ph -= 1.0f;
            ph2 += inc * 1.59f;
            if (ph2 >= 1.0f) ph2 -= 1.0f;
            const float b = fm_sin_turns(ph) + rim * 0.6f * fm_sin_turns(ph2);
            out[i] = fm_tanhf((b * body * body_lvl + noise * noise_lvl) * 1.4f);
            burst = fm_flush(burst * burst_mul);
            tail = fm_flush(tail * tail_mul);
            body = fm_flush(body * body_mul);
            pitch_env *= pitch_mul;
        }
    }
};

// ------------------------------------------------------------------ TAMBOR (toms, congas, percusión metálica)
// Two operators (FM): the modulator's ratio goes from 1 (a tom: round, harmonic) to 2.76 (a bell-like, metallic
// hit); its amount has an envelope of its own, faster than the sound's, so the attack is the bright part. A pitch
// sweep and a click on top.
// Parameters: NOTE tune, CHARACTER timbre (ratio), TONE FM amount, DECAY, EXTRA1 sweep (barrido), EXTRA2 click.
struct Tambor {
    float ph, ph_m, freq, amp, idx_env, pitch_env, click_env, attack, noise_lp;
    Rng rng;

    void init(uint32_t seed) {
        ph = ph_m = 0.0f; freq = 110.0f; amp = idx_env = pitch_env = click_env = 0.0f; attack = 1.0f; noise_lp = 0.0f;
        rng.init(seed);
    }
    void trigger(float hz, float velocity) {
        freq = hz; ph = ph_m = 0.0f; amp = velocity; idx_env = 1.0f; pitch_env = 1.0f; click_env = velocity; attack = 0.0f;
    }
    void render(float *out, uint32_t n, const float *p, float sr) {
        const float ratio = 1.0f + p[EPK_P_CHARACTER] * 1.76f;
        const float index = p[EPK_P_TONE] * (1.5f + 3.5f * p[EPK_P_CHARACTER]);    // in turns of phase x 2 pi below
        const float decay_s = 0.04f * fm_exp2f(p[EPK_P_DECAY] * 5.6f);            // 40 ms .. ~2 s
        const float amp_mul = fm_expf(-6.9f / (decay_s * sr));
        const float idx_mul = fm_expf(-6.9f / (decay_s * 0.35f * sr));
        const float pitch_mul = fm_expf(-1.0f / (0.03f * sr));
        const float click_mul = fm_expf(-1.0f / (0.002f * sr));
        const float sweep = p[EPK_P_EXTRA1] * 2.0f;                               // octaves (octavas)
        const float click = p[EPK_P_EXTRA2] * 0.9f;
        for (uint32_t i = 0; i < n; i++) {
            const float inc = freq * fm_exp2f(sweep * pitch_env) / sr;
            ph += inc;
            if (ph >= 1.0f) ph -= 1.0f;
            ph_m += inc * ratio;
            if (ph_m >= 1.0f) ph_m -= 1.0f;
            const float mod = fm_sin_turns(ph_m) * index * idx_env * 0.159f;      // radians -> turns
            const float nz = rng.bipolar();
            noise_lp += (nz - noise_lp) * 0.25f;
            const float s = fm_sin_turns(ph + mod) * amp + (nz - noise_lp) * click_env * click;
            attack = fm_minf(attack + 0.08f, 1.0f);
            out[i] = fm_tanhf(s * 1.3f) * attack;
            amp = fm_flush(amp * amp_mul);
            idx_env = fm_flush(idx_env * idx_mul);
            pitch_env *= pitch_mul;
            click_env = fm_flush(click_env * click_mul);
        }
    }
};

// ------------------------------------------------------------------ shared: a polyBLEP saw (sierra sin aliasing)
inline float blep_saw(float phase, float inc) {
    float saw = 2.0f * phase - 1.0f;
    if (phase < inc) { const float t = phase / inc; saw -= t + t - t * t - 1.0f; }
    else if (phase > 1.0f - inc) { const float t = (phase - 1.0f) / inc; saw -= t * t + t + t + 1.0f; }
    return saw;
}

// ------------------------------------------------------------------ ACORDE DUB (acorde dub)
// Four voices from a chord table of Plaits Palette (Original, Colour Chords, Disastrous Peace, Tonnetz Walk: MIT,
// see CREDITS.md), each two saws a few cents apart, through one resonant low-pass with its own envelope: the dub
// stab. NOTE the root, CHARACTER the chord, TONE the cutoff, DECAY, EXTRA1 the table, EXTRA2 how far the filter
// envelope opens, ATTACK.
struct Acorde {
    float ph[8], hz[8], amp, fenv, attack, ic1, ic2, root;
    int chord_on, table_on;

    void init(uint32_t seed) {
        for (int i = 0; i < 8; i++) { ph[i] = (float)((seed >> i) & 7u) * 0.125f; hz[i] = 110.0f; }
        amp = fenv = 0.0f; attack = 1.0f; ic1 = ic2 = 0.0f; root = 110.0f; chord_on = table_on = -1;
        a1 = a2 = a3 = 0.0f;
    }
    void trigger(float h, float velocity) { root = h; amp = velocity; fenv = 1.0f; attack = 0.0f; chord_on = -1; }
    void render(float *out, uint32_t n, const float *p, float sr) {
        const int table = clampi((int)(p[EPK_P_EXTRA1] * (float)(kChordTables - 1) + 0.5f), 0, kChordTables - 1);
        const int chord = clampi((int)(p[EPK_P_CHARACTER] * (float)(kChordCount[table] - 1) + 0.5f), 0, kChordCount[table] - 1);
        if (chord != chord_on || table != table_on) {                // the voices' pitches (alturas)
            chord_on = chord; table_on = table;
            int low = 32767;
            for (int v = 0; v < 4; v++) low = kChordCents[table][chord][v] < low ? kChordCents[table][chord][v] : low;
            const int shift = (low / 1200) * 1200;                   // the lowest voice within the root's octave
            for (int v = 0; v < 4; v++) {
                const float c = (float)(kChordCents[table][chord][v] - shift);
                hz[2 * v] = root * fm_exp2f((c - 6.0f) / 1200.0f);
                hz[2 * v + 1] = root * fm_exp2f((c + 7.0f) / 1200.0f);
            }
        }
        const float base = 60.0f * fm_exp2f(p[EPK_P_TONE] * 7.5f);                 // 60 Hz .. ~11 kHz
        const float env_oct = p[EPK_P_EXTRA2] * 5.0f;
        const float decay_s = 0.04f * fm_exp2f(p[EPK_P_DECAY] * 6.6f);              // 40 ms .. ~4 s
        const float amp_mul = fm_expf(-6.9f / (decay_s * sr)), fenv_mul = fm_expf(-6.9f / (decay_s * 0.45f * sr));
        const float attack_inc = 1.0f / (0.001f * fm_exp2f(p[EPK_P_ATTACK] * 10.5f) * sr);
        const float k = 1.25f;                                                      // some resonance (resonancia)
        for (uint32_t i = 0; i < n; i++) {
            if ((i & 15u) == 0) {                                                   // the filter every 16 samples
                const float fc = fm_minf(base * fm_exp2f(env_oct * fenv), sr * 0.45f);
                const float g = fm_tanf(3.14159265f * fc / sr);
                a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
            }
            float s = 0.0f;
            for (int v = 0; v < 8; v++) {
                const float inc = hz[v] / sr;
                ph[v] += inc;
                if (ph[v] >= 1.0f) ph[v] -= 1.0f;
                s += blep_saw(ph[v], inc);
            }
            s *= 0.11f;
            const float v3 = s - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = fm_flush(2.0f * v1 - ic1);
            ic2 = fm_flush(2.0f * v2 - ic2);
            out[i] = fm_tanhf(v2 * amp * attack * 4.2f);
            if (attack < 1.0f) attack = fm_minf(attack + attack_inc, 1.0f);
            else { amp = fm_flush(amp * amp_mul); fenv = fm_flush(fenv * fenv_mul); }
        }
    }
    float a1, a2, a3;                          // the filter's coefficients (coeficientes)
};

// ------------------------------------------------------------------ DRONE (dron)
// It sustains (se sostiene): a hit opens it at ATTACK speed, it holds for SUSTAIN (DECAY: 0.2 .. 30 s) and then
// fades over 2 s; a new hit while it sounds keeps it going and glides to the new note (legato). Three oscillators:
// the root, the root a little apart, and an INTERVAL voice (EXTRA2: unison, minor third, fourth, fifth, octave,
// fifth + octave). CHARACTER morphs sine -> triangle -> saw -> saw with breath (aire); TONE a low-pass. DRIFT
// (EXTRA1, deriva): a slow random walk of pitch, spread and filter, what keeps a drone alive.
struct Drone {
    float ph[3], hz, hz_to, amp, hold, ic1, ic2, drift_p, drift_f, drift_s, d_to_p, d_to_f, d_to_s;
    int gate, sub;
    Rng rng;

    void init(uint32_t seed) {
        ph[0] = 0.0f; ph[1] = 0.33f; ph[2] = 0.66f; hz = hz_to = 110.0f; amp = 0.0f; hold = 0.0f; ic1 = ic2 = 0.0f;
        drift_p = drift_f = drift_s = d_to_p = d_to_f = d_to_s = 0.0f; gate = 0; sub = 0;
        a1 = a2 = a3 = 0.0f; vel = 0.0f;
        rng.init(seed ^ 0xD20E5u);
    }
    void trigger(float h, float velocity) {
        hz_to = h;
        if (amp < 0.001f) hz = h;                                    // silent: start on the note (empieza en la nota)
        gate = 1; hold = -1.0f; vel = velocity;
    }
    void render(float *out, uint32_t n, const float *p, float sr) {
        static const float kInterval[6] = {0.0f, 3.0f, 5.0f, 7.0f, 12.0f, 19.0f};
        const float hold_s = 0.2f * fm_exp2f(p[EPK_P_DECAY] * 7.2f);               // 0.2 .. ~30 s
        if (hold < 0.0f) hold = hold_s * sr;
        const float rise = 1.0f - fm_expf(-1.0f / ((0.005f + 8.0f * p[EPK_P_ATTACK] * p[EPK_P_ATTACK]) * sr));
        const float fall = 1.0f - fm_expf(-1.0f / (0.6f * sr));
        const float glide = 1.0f - fm_expf(-1.0f / (0.25f * sr));
        const float ch = p[EPK_P_CHARACTER], drift = p[EPK_P_EXTRA1];
        const float iv = kInterval[clampi((int)(p[EPK_P_EXTRA2] * 5.0f + 0.5f), 0, 5)];
        const float w_sine = clamp01(1.0f - ch * 3.0f), w_tri = clamp01(1.0f - fm_fabsf(ch * 3.0f - 1.0f));
        const float w_saw = clamp01(ch * 3.0f - 1.0f), air = clamp01(ch * 3.0f - 2.0f);
        for (uint32_t i = 0; i < n; i++) {
            if (sub == 0) {                                          // every 32 samples: drift and filter
                if (rng.unipolar() < 0.004f) {                       // a new place to wander to (nuevo rumbo)
                    d_to_p = rng.bipolar(); d_to_f = rng.bipolar(); d_to_s = rng.unipolar();
                }
                drift_p += (d_to_p - drift_p) * 0.002f;
                drift_f += (d_to_f - drift_f) * 0.002f;
                drift_s += (d_to_s - drift_s) * 0.002f;
                const float fc = fm_minf(50.0f * fm_exp2f(p[EPK_P_TONE] * 8.0f + drift * drift_f * 1.2f), sr * 0.45f);
                const float g = fm_tanf(3.14159265f * fc / sr), k = 1.1f;
                a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
            }
            sub = (sub + 1) & 31;
            hz += (hz_to - hz) * glide;
            const float base = hz * fm_exp2f(drift * drift_p * 0.25f / 12.0f);
            const float spread = 0.002f + drift * 0.012f * drift_s;
            const float f[3] = {base, base * (1.0f + spread), base * fm_exp2f(iv / 12.0f) * (1.0f - spread * 0.5f)};
            float s = 0.0f;
            for (int o = 0; o < 3; o++) {
                const float inc = f[o] / sr;
                ph[o] += inc;
                if (ph[o] >= 1.0f) ph[o] -= 1.0f;
                const float sine = fm_sin_turns(ph[o]);
                const float tri = ph[o] < 0.5f ? 4.0f * ph[o] - 1.0f : 3.0f - 4.0f * ph[o];
                s += sine * w_sine + tri * w_tri + blep_saw(ph[o], inc) * w_saw * 0.8f;
            }
            s = s * 0.3f + rng.bipolar() * air * 0.12f;
            const float v3 = s - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = fm_flush(2.0f * v1 - ic1);
            ic2 = fm_flush(2.0f * v2 - ic2);
            if (gate) {
                amp += (vel - amp) * rise;
                if (--hold <= 0.0f) gate = 0;
            } else amp = fm_flush(amp - amp * fall);
            out[i] = fm_tanhf(v2 * amp * 0.8f);
        }
    }
    float a1, a2, a3, vel;
};

// ------------------------------------------------------------------ MACRO (macro-oscilador)
// Several ways of making a sound behind the same few knobs, the idea of Mutable Instruments Plaits (the code here is
// written for EPICYKLOS, not ported: see CREDITS.md). One model at a time:
//   ANALOG   two oscillators, saw -> square -> narrow pulse (TIMBRE); the second detuned, then a sub octave, a fifth,
//            an octave (ARMON)
//   PLIEGUE  a sine through a wavefolder (TIMBRE: how many folds; ARMON: asymmetry)
//   FM       two operators (ARMON: the ratio; TIMBRE: how much, falling with the note)
//   GRANO    two formants restarted every period, like a voice (TIMBRE: the formant; ARMON: the second one's place)
//   CUERDA   a plucked string, Karplus-Strong (TIMBRE: brightness; ARMON: how hard the pick is); DECAY is its ring
//   ARMONICO eight sine harmonics under a bump (ARMON: where; TIMBRE: how wide)
//   ACIDO    the acid line: saw -> square (TIMBRE) through a resonant low-pass of 18 dB an octave (ARMON: resonance)
//            that each note snaps open; some notes accented; ATTACK here is SALTOS: how many notes jump an octave
//            and slide into the next, in a loop of 16 hits. Not a model of any one machine (no es una emulación)
// After the model, a low-pass that the note's envelope opens (a low-pass gate): TONE at 1 is open, lower and each
// note closes as it fades. NOTE, CHARACTER = TIMBRE, TONE, DECAY, EXTRA1 = MODELO, EXTRA2 = ARMON, ATTACK.
enum { MACRO_ANALOG, MACRO_PLIEGUE, MACRO_FM, MACRO_GRANO, MACRO_CUERDA, MACRO_ARMONICO, MACRO_ACIDO, MACRO_MODELS };
struct Macro {
    static constexpr int kString = 1400;       // a string down to 32 Hz at 44.1 kHz (cuerda)
    float ph, ph2, freq, amp, env, attack, vel, ic1, ic2, a1, a2, a3, fb, hp, ks_lp, ks_pos;
    float fcur, jump, lp1, g1;                 // ACIDO: the gliding pitch, this note's octave, the third pole
    int fresh, di, sub, legato, accent, slide;
    uint32_t hits, salt;
    int16_t dl[kString];
    Rng rng;

    void init(uint32_t seed) {
        ph = 0.0f; ph2 = 0.37f; freq = 110.0f; amp = env = vel = 0.0f; attack = 1.0f; ic1 = ic2 = 0.0f;
        a1 = a2 = a3 = 0.0f; fb = hp = ks_lp = ks_pos = 0.0f; fresh = 0; di = 0; sub = 0;
        fcur = 110.0f; jump = 1.0f; lp1 = g1 = 0.0f; legato = accent = slide = 0; hits = 0; salt = seed;
        for (int16_t &v : dl) v = 0;
        rng.init(seed ^ 0x3AC20u);
    }
    void trigger(float hz, float velocity) {
        legato = amp > 0.02f;                                        // the last note still sounds (aún suena)
        freq = hz; vel = velocity; amp = velocity; env = 1.0f; attack = 0.0f; fresh = 1;
    }
    static inline int model_of(const float *p) {
        return clampi((int)(p[EPK_P_EXTRA1] * (float)(MACRO_MODELS - 1) + 0.5f), 0, MACRO_MODELS - 1);
    }
    void render(float *out, uint32_t n, const float *p, float sr) {
        const int model = model_of(p);
        const float timbre = p[EPK_P_CHARACTER], harm = p[EPK_P_EXTRA2], tone = p[EPK_P_TONE];
        const float decay_s = 0.02f * fm_exp2f(p[EPK_P_DECAY] * 7.6f);            // 20 ms .. ~4 s
        const float amp_mul = fm_expf(-6.9f / (decay_s * sr));
        float attack_inc = 1.0f / (0.001f * fm_exp2f(p[EPK_P_ATTACK] * 10.5f) * sr);
        const float base = 45.0f * fm_exp2f(tone * 8.5f);                         // 45 Hz .. ~16 kHz
        float gate_oct = (1.0f - tone) * 4.5f;                                    // what the envelope opens (LPG)
        float k_res = 1.3f, acid_lvl = 1.0f;
        const float glide = 1.0f - fm_expf(-1.0f / (0.045f * sr));
        const float inc = fm_minf(freq / sr, 0.45f);
        // per model, once a block (por modelo, una vez por bloque)
        float inc2 = inc, mix2 = 0.0f, width = 0.5f, drive = 1.0f, bias = 0.0f, ratio = 1.0f, index = 0.0f;
        float f1 = 1.0f, f2 = 1.0f, damp = 0.5f, ks_fb = 0.99f, period = 100.0f, gain = 1.0f;
        float hw[8];
        for (float &w : hw) w = 0.0f;
        switch (model) {
        case MACRO_ANALOG: {
            static const float kIv[4] = {-12.0f, 7.0f, 12.0f, 19.0f};
            const float semis = harm < 0.5f ? harm : kIv[clampi((int)((harm - 0.5f) * 8.0f), 0, 3)] + 0.04f;
            inc2 = fm_minf(inc * fm_exp2f(semis / 12.0f), 0.45f);
            mix2 = timbre < 0.5f ? timbre * 2.0f : 1.0f;                          // saw -> square
            width = timbre < 0.5f ? 0.5f : 0.5f - (timbre - 0.5f) * 0.84f;        // -> a narrow pulse (pulso)
            gain = 0.8f;
            break;
        }
        case MACRO_PLIEGUE:
            drive = 1.0f + timbre * timbre * 9.0f;
            bias = harm * 0.9f;
            gain = 1.1f;
            break;
        case MACRO_FM: {
            static const float kRatio[9] = {0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 3.5f, 4.0f, 5.0f, 7.0f};
            ratio = kRatio[clampi((int)(harm * 8.0f + 0.5f), 0, 8)];
            index = timbre * timbre * 5.0f * 0.159f;                              // radians -> turns
            gain = 1.25f;
            break;
        }
        case MACRO_GRANO:
            f1 = fm_maxf(150.0f * fm_exp2f(timbre * 4.5f) / freq, 1.0f);          // formants, in periods (formantes)
            f2 = f1 * (1.3f + harm * 2.2f);
            gain = 0.8f;
            break;
        case MACRO_CUERDA: {
            period = fm_clampf(sr / freq, 8.0f, (float)(kString - 2));
            damp = 0.12f + 0.85f * timbre;
            ks_fb = fm_expf(-6.9f / (0.25f * fm_exp2f(p[EPK_P_DECAY] * 5.0f) * freq));   // rings 0.25 .. 8 s
            if (fresh) {                                             // the pick: a noise burst (la púa)
                const int len = (int)period;
                const float soft = 0.08f + 0.9f * harm;
                float z = 0.0f;
                for (int k = 0; k < len; k++) {
                    z += (rng.bipolar() - z) * soft;
                    int w = di - len + k;
                    if (w < 0) w += kString;
                    dl[w] = (int16_t)(z * vel * 20000.0f);
                }
                ks_lp = 0.0f;
            }
            gain = 1.6f;
            break;
        }
        case MACRO_ACIDO: {
            if (fresh) {                                             // this note: octave, slide, accent (salto, ligado, acento)
                uint32_t h = ((hits++ & 15u) + 1u) * 2654435761u ^ salt;
                h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
                const float jumps = p[EPK_P_ATTACK];
                const float u1 = (float)(h & 0xFFu) * (1.0f / 255.0f), u2 = (float)(h >> 8 & 0xFFu) * (1.0f / 255.0f);
                const float u3 = (float)(h >> 16 & 0xFFu) * (1.0f / 255.0f);
                jump = u1 < jumps * 0.45f ? (u2 < 0.7f ? 2.0f : 0.5f) : 1.0f;
                slide = legato && u3 < jumps * 0.6f;
                accent = (h >> 24) < 77u;                            // three in ten
                if (!slide) fcur = freq * jump;
            }
            mix2 = timbre;                                           // saw -> square
            k_res = 1.9f - 1.78f * harm;
            // the envelope opens less as TONE closes: at the bottom the line is gone (abajo del todo, desaparece)
            gate_oct = (0.6f + 2.9f * clamp01(tone * 3.0f)) + (accent ? 0.3f + 0.7f * clamp01(tone * 3.0f) : 0.0f);
            acid_lvl = accent ? 1.0f : 0.7f;
            attack_inc = 1.0f / (0.002f * sr);
            gain = 2.0f;
            break;
        }
        default: {                                                   // ARMONICO
            const float centre = 1.0f + harm * 7.0f, wide = 0.7f + timbre * 4.0f;
            float sum = 0.0f;
            for (int k = 0; k < 8; k++) {
                const float d = fm_fabsf((float)(k + 1) - centre) / wide;
                hw[k] = (float)(k + 1) * inc < 0.45f && d < 1.0f ? (1.0f - d) * (1.0f - d) : 0.0f;
                sum += hw[k];
            }
            if (hw[0] < 0.15f) { sum += 0.15f - hw[0]; hw[0] = 0.15f; }           // always some fundamental
            for (float &w : hw) w /= sum;
            gain = 1.1f;
            break;
        }
        }
        fresh = 0;
        const int acid = model == MACRO_ACIDO;
        float inc_s = inc;
        for (uint32_t i = 0; i < n; i++) {
            if (sub == 0) {                                          // the gate's filter every 16 samples
                const float fc = fm_minf(base * fm_exp2f(gate_oct * (acid ? env * env : env)), sr * 0.45f);
                const float g = fm_tanf(3.14159265f * fc / sr), k = k_res;
                g1 = fm_minf(6.2831853f * fc / sr, 0.95f);
                a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
            }
            sub = (sub + 1) & 15;
            if (acid) {
                fcur += (freq * jump - fcur) * glide;
                inc_s = fm_minf(fcur / sr, 0.45f);
            }
            ph += inc_s;
            if (ph >= 1.0f) ph -= 1.0f;
            float s;
            switch (model) {
            case MACRO_ANALOG: {
                float q = ph + width;
                if (q >= 1.0f) q -= 1.0f;
                ph2 += inc2;
                if (ph2 >= 1.0f) ph2 -= 1.0f;
                float q2 = ph2 + width;
                if (q2 >= 1.0f) q2 -= 1.0f;
                s = blep_saw(ph, inc) - mix2 * blep_saw(q, inc) + blep_saw(ph2, inc2) - mix2 * blep_saw(q2, inc2);
                break;
            }
            case MACRO_ACIDO: {
                float q = ph + 0.5f;
                if (q >= 1.0f) q -= 1.0f;
                s = fm_tanhf((blep_saw(ph, inc_s) - mix2 * blep_saw(q, inc_s)) * 1.2f);
                break;
            }
            case MACRO_PLIEGUE: {
                float x = (fm_sin_turns(ph) * drive + bias) * 0.25f;              // sin(pi/2 x): a sine folder
                x -= fm_floorf(x);
                s = fm_sin_turns(x);
                break;
            }
            case MACRO_FM: {
                ph2 += inc * ratio;
                if (ph2 >= 1.0f) ph2 -= 1.0f;
                float m = ph2 + fb * 0.08f;
                m -= fm_floorf(m);
                fb = fm_sin_turns(m);
                float c = ph + fb * index * (0.25f + 0.75f * env);
                c -= fm_floorf(c);
                s = fm_sin_turns(c);
                break;
            }
            case MACRO_GRANO: {
                const float w = (1.0f - ph) * (1.0f - ph);
                float x1 = ph * f1, x2 = ph * f2;
                x1 -= fm_floorf(x1); x2 -= fm_floorf(x2);
                s = (fm_sin_turns(x1) + 0.6f * fm_sin_turns(x2)) * w;
                break;
            }
            case MACRO_CUERDA: {
                float rd = (float)di - period;
                if (rd < 0.0f) rd += (float)kString;
                const int r0 = (int)rd;
                const float fr = rd - (float)r0;
                const int r1 = r0 + 1 >= kString ? 0 : r0 + 1;
                const float y = ((float)dl[r0] + ((float)dl[r1] - (float)dl[r0]) * fr) * (1.0f / 20000.0f);
                ks_lp = fm_flush(ks_lp + (y - ks_lp) * damp);
                dl[di] = (int16_t)(fm_clampf(ks_lp * ks_fb, -1.6f, 1.6f) * 20000.0f);
                if (++di >= kString) di = 0;
                s = y;
                break;
            }
            default: {
                s = 0.0f;
                for (int k = 0; k < 8; k++) {
                    if (hw[k] == 0.0f) continue;
                    float x = ph * (float)(k + 1);
                    x -= fm_floorf(x);
                    s += fm_sin_turns(x) * hw[k];
                }
                break;
            }
            }
            hp = fm_flush(hp + (s - hp) * 0.0025f);                               // ~18 Hz: no DC (sin continua)
            s -= hp;
            const float v3 = s - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = fm_flush(2.0f * v1 - ic1);
            ic2 = fm_flush(2.0f * v2 - ic2);
            float y = v2;
            if (acid) { lp1 = fm_flush(lp1 + (v2 - lp1) * g1); y = lp1 * acid_lvl; }   // 12 + 6 dB an octave
            const float level = model == MACRO_CUERDA ? vel : amp;                // the string fades by itself
            out[i] = fm_tanhf(y * level * attack * gain);
            if (attack < 1.0f) attack = fm_minf(attack + attack_inc, 1.0f);
            else { amp = fm_flush(amp * amp_mul); env = amp / fm_maxf(vel, 0.001f); }
        }
    }
};

// ------------------------------------------------------------------ a track's voice (la voz de una pista)
// One engine at a time, in the same memory (una a la vez, en la misma memoria).
struct Voice {
    int engine;
    uint32_t seed;
    union {
        TestVoice test;
        Grave grave;
        Metal metal;
        Golpe golpe;
        Tambor tambor;
        Acorde acorde;
        Drone drone;
        Macro macro;
    } u;

    void init(int eng, uint32_t sd) {
        engine = eng; seed = sd;
        switch (eng) {
        case EPK_ENG_GRAVE: u.grave.init(sd); break;
        case EPK_ENG_METAL: u.metal.init(sd); break;
        case EPK_ENG_GOLPE: u.golpe.init(sd); break;
        case EPK_ENG_TAMBOR: u.tambor.init(sd); break;
        case EPK_ENG_ACORDE: u.acorde.init(sd); break;
        case EPK_ENG_DRONE: u.drone.init(sd); break;
        case EPK_ENG_MACRO: u.macro.init(sd); break;
        default: u.test.init(sd); break;
        }
    }
    void trigger(float hz, float velocity) {
        switch (engine) {
        case EPK_ENG_GRAVE: u.grave.trigger(hz, velocity); break;
        case EPK_ENG_METAL: u.metal.trigger(hz, velocity); break;
        case EPK_ENG_GOLPE: u.golpe.trigger(hz, velocity); break;
        case EPK_ENG_TAMBOR: u.tambor.trigger(hz, velocity); break;
        case EPK_ENG_ACORDE: u.acorde.trigger(hz, velocity); break;
        case EPK_ENG_DRONE: u.drone.trigger(hz, velocity); break;
        case EPK_ENG_MACRO: u.macro.trigger(hz, velocity); break;
        default: u.test.trigger(hz, velocity); break;
        }
    }
    void release() { if (engine == EPK_ENG_DRONE) u.drone.gate = 0; }   // the sequencer stopped: let it fade
    void render(float *out, uint32_t n, const float *p, float sr) {
        switch (engine) {
        case EPK_ENG_GRAVE: u.grave.render(out, n, p, sr); break;
        case EPK_ENG_METAL: u.metal.render(out, n, p, sr); break;
        case EPK_ENG_GOLPE: u.golpe.render(out, n, p, sr); break;
        case EPK_ENG_TAMBOR: u.tambor.render(out, n, p, sr); break;
        case EPK_ENG_ACORDE: u.acorde.render(out, n, p, sr); break;
        case EPK_ENG_DRONE: u.drone.render(out, n, p, sr); break;
        case EPK_ENG_MACRO: u.macro.render(out, n, p, sr); break;
        default: u.test.render(out, n, p, sr); break;
        }
    }
};

}  // namespace epk
