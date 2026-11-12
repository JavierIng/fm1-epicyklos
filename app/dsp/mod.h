// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Javier Sánchez
// EPICYKLOS modulation (modulación), the project's: two LFOs locked to the bar and two envelope followers, each with
// two destinations and a bipolar amount. Worked out once a block (≤ 256 samples): the targets smooth what they get.
// Rules: no heap, float only.
#pragma once
#include "epk.h"
#include "fastmath.h"
#include "voices.h"

namespace epk {

// LFO rates in sixteenths per cycle (semicorcheas por ciclo): 1/16 .. 32 bars
constexpr float kLfoSixteenths[EPK_LFO_RATE_COUNT] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512};

struct ModSource {
    float out;              // LFO: -1..1; follower: 0..1 (salida)
    float last_phase;       // to see a new cycle (S&H, random)
    float held, from, to;   // S&H and smooth random (azar)
    float env;              // the follower's envelope (envolvente)
};

struct Mod {
    float m[EPK_MOD_SOURCES][EPK_M_COUNT];        // the settings, 0..1 (ajustes)
    ModSource s[EPK_MOD_SOURCES];
    float free_phase;                             // stopped: the LFOs run on their own (parado: van solas)
    float off[EPK_DEST_COUNT];                    // this block's offsets (desplazamientos de este bloque)
    Rng rng;

    void init() {
        static const float kDefault[EPK_MOD_SOURCES][EPK_M_COUNT] = {
            // shape/source, rate/release, dest 1, amount 1, dest 2, amount 2  (amount 0.5 = none)
            {1.0f / 5.0f, 7.0f / 9.0f, 0.0f, 0.5f, 0.0f, 0.5f},     // LFO 1: triangle, 8 bars (to set)
            {0.0f, 5.0f / 9.0f, 0.0f, 0.5f, 0.0f, 0.5f},            // LFO 2: sine, 2 bars
            {0.0f, 0.25f, 0.0f, 0.5f, 0.0f, 0.5f},                  // follower 1: the kick (track 1)
            {5.0f / 5.0f, 0.4f, 0.0f, 0.5f, 0.0f, 0.5f},            // follower 2: track 6
        };
        for (int i = 0; i < EPK_MOD_SOURCES; i++) {
            for (int k = 0; k < EPK_M_COUNT; k++) m[i][k] = kDefault[i][k];
            s[i].out = s[i].last_phase = s[i].held = s[i].from = s[i].to = s[i].env = 0.0f;
        }
        // a default that moves (un ajuste de inicio que se mueve): LFO 1 opens and closes the delay's tone
        m[0][EPK_M_DEST1] = (float)EPK_DEST_DLY_TONE / (float)(EPK_DEST_COUNT - 1);
        m[0][EPK_M_AMT1] = 0.5f + 0.5f * 0.25f;
        free_phase = 0.0f;
        rng.init(0x0D0D1E5u);
        for (int d = 0; d < EPK_DEST_COUNT; d++) off[d] = 0.0f;
    }
    static int steps(int src, int param) {
        if (param == EPK_M_SHAPE) return src < 2 ? EPK_LFO_SHAPE_COUNT : EPK_TRACKS;
        if (param == EPK_M_RATE) return src < 2 ? EPK_LFO_RATE_COUNT : 0;
        if (param == EPK_M_DEST1 || param == EPK_M_DEST2) return EPK_DEST_COUNT;
        return 0;
    }
    int idx(int src, int param) const {
        const int n = steps(src, param);
        return n ? (int)(m[src][param] * (float)(n - 1) + 0.5f) : 0;
    }

    // pos16: where the bar clock is, in sixteenths since play (dónde va el reloj); playing: locked to it
    void tick(int playing, float pos16, uint32_t n, float bpm, float sr, const float *track_peak) {
        free_phase += (float)n * bpm * (1.0f / 60.0f) * 4.0f / sr;              // in sixteenths
        if (free_phase > 1048576.0f) free_phase -= 1048576.0f;
        const float t16 = playing ? pos16 : free_phase;
        for (int d = 0; d < EPK_DEST_COUNT; d++) off[d] = 0.0f;
        for (int i = 0; i < EPK_MOD_SOURCES; i++) {
            ModSource &o = s[i];
            if (i < 2) {                                                         // an LFO
                const float len = kLfoSixteenths[idx(i, EPK_M_RATE)];
                const float cyc = t16 / len;
                const float ph = cyc - fm_floorf(cyc);
                const int fresh = ph < o.last_phase;                             // a new cycle (ciclo nuevo)
                o.last_phase = ph;
                switch (idx(i, EPK_M_SHAPE)) {
                case EPK_LFO_SINE: o.out = fm_sin_turns(ph); break;
                case EPK_LFO_TRIANGLE: o.out = ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph; break;
                case EPK_LFO_SAW: o.out = 1.0f - 2.0f * ph; break;                // falls (cae): a dub swell
                case EPK_LFO_SQUARE: o.out = ph < 0.5f ? 1.0f : -1.0f; break;
                case EPK_LFO_SH:
                    if (fresh) o.held = rng.bipolar();
                    o.out = o.held;
                    break;
                default:                                                         // smooth random (azar suave)
                    if (fresh) { o.from = o.to; o.to = rng.bipolar(); }
                    o.out = o.from + (o.to - o.from) * (ph * ph * (3.0f - 2.0f * ph));
                    break;
                }
            } else {                                                             // a follower (seguidor)
                const float in = track_peak[idx(i, EPK_M_SHAPE)] * 3.0f;
                const float rel_s = 0.02f * fm_exp2f(m[i][EPK_M_RATE] * 6.0f);    // 20 ms .. 1.3 s
                const float a = 1.0f - fm_expf(-(float)n / (0.004f * sr));
                const float r = 1.0f - fm_expf(-(float)n / (rel_s * sr));
                o.env += (in - o.env) * (in > o.env ? a : r);
                o.out = fm_minf(o.env, 1.0f);
            }
            const int d1 = idx(i, EPK_M_DEST1), d2 = idx(i, EPK_M_DEST2);
            if (d1) off[d1] += o.out * (m[i][EPK_M_AMT1] - 0.5f) * 2.0f;
            if (d2) off[d2] += o.out * (m[i][EPK_M_AMT2] - 0.5f) * 2.0f;
        }
    }
};

}  // namespace epk
