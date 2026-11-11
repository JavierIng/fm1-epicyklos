// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Javier Sánchez
// EPICYKLOS voices (motores de sonido): TEST (prueba), GRAVE (kick + rumble), METAL (808 / 909 hats and rides).
// Every voice is plain data with init / trigger / render; parameters arrive 0..1 (p[EPK_P_*]).
// Rules: no heap, float only, no 64-bit multiplies.
#pragma once
#include "epk.h"
#include "fastmath.h"

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
// One oscillator morphing sine (with a pitch drop) -> saw -> noise, a low-pass, an exponential decay.
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
            attack = fm_minf(attack + 0.02f, 1.0f);
            out[i] = v2 * amp * attack;
            amp = fm_flush(amp * amp_mul);
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

// ------------------------------------------------------------------ a track's voice (la voz de una pista)
// One engine at a time, in the same memory (una a la vez, en la misma memoria).
struct Voice {
    int engine;
    uint32_t seed;
    union {
        TestVoice test;
        Grave grave;
        Metal metal;
    } u;

    void init(int eng, uint32_t sd) {
        engine = eng; seed = sd;
        switch (eng) {
        case EPK_ENG_GRAVE: u.grave.init(sd); break;
        case EPK_ENG_METAL: u.metal.init(sd); break;
        default: u.test.init(sd); break;
        }
    }
    void trigger(float hz, float velocity) {
        switch (engine) {
        case EPK_ENG_GRAVE: u.grave.trigger(hz, velocity); break;
        case EPK_ENG_METAL: u.metal.trigger(hz, velocity); break;
        default: u.test.trigger(hz, velocity); break;
        }
    }
    void render(float *out, uint32_t n, const float *p, float sr) {
        switch (engine) {
        case EPK_ENG_GRAVE: u.grave.render(out, n, p, sr); break;
        case EPK_ENG_METAL: u.metal.render(out, n, p, sr); break;
        default: u.test.render(out, n, p, sr); break;
        }
    }
};

}  // namespace epk
