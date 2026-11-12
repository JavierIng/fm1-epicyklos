// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Javier Sánchez
// MAREA (marea): a bank of 16 band-pass filters whose levels a travelling wave moves, inserted on one track.
// Inspired by Hypnus Records' Slink Filter; its controls follow what Slink's public user manual says they do (RATE,
// SYNC, freeze, reverse, MULTIPLY, RIPPLE, OFFSET, INVERT, GRAVITY, MIX, an area where the wave acts, bands split
// left/right at fixed frequencies with a width). The algorithm is ours, written for this project: no code from it.
//
// The wave, across the bank (x 0..1 from the lowest band to the highest):
//   w = sin(x*MULTIPLY + phase + OFFSET) + RIPPLE * sin(3.3 x*MULTIPLY - 1.7 phase + 2 OFFSET), normalised to -1..1
//   GRAVITY pulls it down: h = ((w + 1) / 2) ^ (1 + 8 GRAVITY): wide valleys, thin crests ("wormholes" at the top)
//   INVERT is the depth, bipolar: 1 full, 0.5 none, 0 upside down (al revés)
//   outside the area (FROM .. TO, one octave of slope) a band keeps its level (fuera del área, nivel fijo)
// Levels are worked out every 16 samples and glide (se deslizan). Rules: no heap, float only.
#pragma once
#include "epk.h"
#include "fastmath.h"
#include "master.h"

namespace epk {

constexpr int kMareaBands = 16;
constexpr float kMareaLow = 40.0f, kMareaOctaves = 8.6f;        // fixed points: 40 Hz .. ~15.5 kHz (puntos fijos)

inline float marea_band_hz(int b) { return kMareaLow * fm_exp2f((float)b * (kMareaOctaves / (float)(kMareaBands - 1))); }
inline float marea_edge_hz(float v) { return 30.0f * fm_exp2f(v * 9.4f); }          // FROM / TO: 30 Hz .. 20 kHz
// the wave's speed in cycles a second, signed (velocidad con signo); 0 in the middle of the knob (parada)
inline float marea_speed_hz(float v, int sync, float bpm) {
    const float s = (v - 0.5f) * 2.0f, a = fm_fabsf(s);
    if (a < 0.04f) return 0.0f;                                  // the middle: frozen (congelada)
    const float sign = s < 0.0f ? -1.0f : 1.0f;
    if (sync) {                                                  // 32 bars .. 1/4 bar a cycle (ciclos por compás)
        static const float kBars[8] = {32.0f, 16.0f, 8.0f, 4.0f, 2.0f, 1.0f, 0.5f, 0.25f};
        const int i = (int)((a - 0.04f) / 0.96f * 7.99f);
        return sign * bpm / (240.0f * kBars[i]);
    }
    return sign * 0.03f * fm_exp2f((a - 0.04f) / 0.96f * 7.0f);  // 0.03 .. 3.8 Hz: the middle of each half ~0.3 Hz
}

struct Marea {
    Svf band[kMareaBands];
    SvfCoef coef[kMareaBands];
    float gain[kMareaBands];                     // each band's level now (nivel de cada banda)
    float phase;
    float reso_set, makeup;
    int sub;

    void init() {
        for (int b = 0; b < kMareaBands; b++) { band[b].clear(); gain[b] = 1.0f; }
        phase = 0.0f; reso_set = -1.0f; makeup = 0.8f; sub = 0;
    }
    void clear() { for (int b = 0; b < kMareaBands; b++) band[b].clear(); }

    // the level a band would have now (para la pantalla también)
    static float band_target(int b, const float *fx, float phase) {
        const float x = (float)b / (float)(kMareaBands - 1);
        const float mult = 0.5f + 5.5f * fx[EPK_FX_MAREA_MULTIPLY], ripple = fx[EPK_FX_MAREA_RIPPLE];
        const float off = fx[EPK_FX_MAREA_OFFSET];
        float w = fm_sin_turns(x * mult + phase + off) + ripple * fm_sin_turns(3.3f * x * mult - 1.7f * phase + 2.0f * off);
        w /= 1.0f + ripple;
        float h = fm_powf(0.5f + 0.5f * w, 1.0f + 8.0f * fx[EPK_FX_MAREA_GRAVITY]);   // gravity (gravedad)
        const float d = (fx[EPK_FX_MAREA_INVERT] - 0.5f) * 2.0f;                         // depth -1..1
        if (d < 0.0f) h = 1.0f - h;
        const float g = 1.0f - fm_fabsf(d) + fm_fabsf(d) * h;
        // the area (área): inside FROM..TO the wave acts, outside the band stays at its level
        const float f = marea_band_hz(b);
        const float lo = marea_edge_hz(fx[EPK_FX_MAREA_FROM]), hi = marea_edge_hz(fx[EPK_FX_MAREA_TO]);
        const float a = fm_clampf(fm_log2f(f / lo) + 0.5f, 0.0f, 1.0f) * fm_clampf(fm_log2f(hi / f) + 0.5f, 0.0f, 1.0f);
        return 1.0f + a * (g - 1.0f);
    }

    // mono in, stereo out (mono entra, estéreo sale)
    void process(const float *in, float *out_l, float *out_r, uint32_t n, const float *fx, float bpm, float sr) {
        const float mix = fx[EPK_FX_MAREA_MIX], width = fx[EPK_FX_MAREA_WIDTH];
        const float reso = fx[EPK_FX_MAREA_RESO];
        const int sync = fx[EPK_FX_MAREA_SYNC] > 0.5f;
        const float hz = marea_speed_hz(fx[EPK_FX_MAREA_SPEED], sync, bpm);
        if (fm_fabsf(reso - reso_set) > 0.002f) {
            reso_set = reso;
            const float k = 0.9f - 0.84f * reso;
            for (int b = 0; b < kMareaBands; b++) coef[b].set(marea_band_hz(b), k, sr);
            makeup = 0.8f * fm_powf(0.41f / k, 0.75f);                 // narrow bands keep the level (mismo nivel)
        }
        // the bands split between the sides at fixed frequencies; WIDTH 0 = mono (ancho)
        const float near = 1.0f + width, far = 1.0f - width;
        for (uint32_t i = 0; i < n; i++) {
            if (sub == 0) {
                phase -= hz * 16.0f / sr;                                // forwards: the wave climbs (sube)
                phase -= fm_floorf(phase);
                for (int b = 0; b < kMareaBands; b++) gain[b] += (band_target(b, fx, phase) - gain[b]) * 0.3f;
            }
            sub = (sub + 1) & 15;
            float yl = 0.0f, yr = 0.0f;
            for (int b = 0; b < kMareaBands; b++) {
                float bl, bb, bh;
                band[b].tick(in[i], coef[b].a1, coef[b].a2, coef[b].a3, coef[b].k, bl, bb, bh);
                const float y = bb * coef[b].k * gain[b];                // unity at the peak
                if (b & 1) { yl += y * far; yr += y * near; } else { yl += y * near; yr += y * far; }
            }
            out_l[i] = in[i] + (yl * makeup - in[i]) * mix;
            out_r[i] = in[i] + (yr * makeup - in[i]) * mix;
        }
    }
};

}  // namespace epk
