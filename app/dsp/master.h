// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Javier Sánchez
// EPICYKLOS master chain (cadena del master), after the returns: a filter of several types (DJ, LP, HP, BP, notch,
// morph) with a second, parallel peak (DOBLE, the idea of a dual filter such as Mutable Instruments' Blades; no
// code from it) -> a bus compressor -> tape saturation. (MAREA, the filterbank, is a track insert: marea.h.) The final soft clip stays in engine.cpp.
// Rules: no heap, float only. Coefficients are worked out every 16 samples, with the parameter smoothed, so a knob
// or a modulation moves without zipper noise (sin escalones).
#pragma once
#include "epk.h"
#include "fastmath.h"

namespace epk {

// Zavalishin's state-variable filter (trapezoidal), one channel (un canal)
struct Svf {
    float ic1, ic2;
    void clear() { ic1 = ic2 = 0.0f; }
    // one sample in; lp, bp, hp out
    inline void tick(float x, float a1, float a2, float a3, float k, float &lp, float &bp, float &hp) {
        const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = fm_flush(2.0f * v1 - ic1);
        ic2 = fm_flush(2.0f * v2 - ic2);
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
};
struct SvfCoef {
    float a1, a2, a3, k;
    void set(float fc, float q_k, float sr) {
        const float g = fm_tanf(3.14159265f * fm_clampf(fc, 10.0f, sr * 0.45f) / sr);
        k = q_k;
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
};

// What the FILTER knob means (qué significa el knob): below the middle a low-pass closing, above it a high-pass
// opening, around the middle nothing. Shared with the screen through epk_master_filter().
inline float filter_lp_hz(float v) { return v >= 0.47f ? 20000.0f : 18000.0f * fm_exp2f(-(0.47f - v) / 0.47f * 9.1f); }
inline float filter_hp_hz(float v) { return v <= 0.53f ? 20.0f : 25.0f * fm_exp2f((v - 0.53f) / 0.47f * 8.5f); }

inline float filter_cut_hz(float v) { return 30.0f * fm_exp2f(v * 9.3f); }   // 30 Hz .. 19 kHz

struct Master {
    Svf lp[2], hp[2], peak[2], one[2];
    SvfCoef c_lp, c_hp, c_peak, c_one;
    float v_s;                                    // the filter knob, smoothed (suavizado)
    int type;
    float env, gain_db, gr_max;                   // the compressor (compresor)
    float tone[2], dc_x[2], dc_y[2];              // saturation's top roll-off and DC blocker
    int sub;
    float flt_g, comp_g, sat_g;                   // FLOW: on 1 .. off 0 (activo .. quitado)

    void clear_filters() {
        for (int c = 0; c < 2; c++) { lp[c].clear(); hp[c].clear(); peak[c].clear(); one[c].clear(); }
    }
    void init() {
        clear_filters();
        for (int c = 0; c < 2; c++) tone[c] = dc_x[c] = dc_y[c] = 0.0f;
        v_s = 0.5f; type = -1; env = 0.0f; gain_db = 0.0f; gr_max = 0.0f; sub = 0;
        flt_g = comp_g = sat_g = 1.0f;
    }

    // FLOW: each stage on 1 .. off 0, gliding ~10 ms (sin clics); off and settled, not computed
    static float glide_to(float g, float t, float step) { return g < t ? fm_minf(g + step, t) : fm_maxf(g - step, t); }

    // The filter (filtro), wherever FLOW puts it: on everything, on the tracks or on the returns
    void filter(float *l, float *r, uint32_t n, const float *fx, float sr) {
        const float on = fx[EPK_FX_ON_FLT] > 0.5f ? 1.0f : 0.0f, step = 1.0f / (0.010f * sr);
        if (on == 0.0f && flt_g <= 0.0f) return;
        if (on > 0.0f && flt_g <= 0.0f) clear_filters();                // back on: from empty (vuelve vacío)
        const float reso = fx[EPK_FX_FLT_RESO], dbl = fx[EPK_FX_FLT_DOUBLE];
        const int ty = (int)(fx[EPK_FX_FLT_TYPE] * (float)(EPK_FLT_COUNT - 1) + 0.5f);
        if (ty != type) { type = ty; clear_filters(); }               // another filter: from empty (desde vacío)
        float act = 0.0f;
        for (uint32_t i = 0; i < n; i++) {
            if (sub == 0) {                                              // every 16 samples (cada 16 muestras)
                v_s += (fx[EPK_FX_FLT] - v_s) * 0.15f;
                if (type == EPK_FLT_DJ) {
                    act = fm_minf(fm_fabsf(v_s - 0.5f) * 10.0f, 1.0f);   // 0 in the middle, 1 away from it
                    const float k_on = 2.0f - 1.94f * reso * act, k_off = 1.414f;
                    const float f_lp = filter_lp_hz(v_s), f_hp = filter_hp_hz(v_s);
                    c_lp.set(f_lp, v_s < 0.5f ? k_on : k_off, sr);
                    c_hp.set(f_hp, v_s > 0.5f ? k_on : k_off, sr);
                    const float f_active = v_s < 0.5f ? f_lp : v_s > 0.5f ? f_hp : 1000.0f;
                    c_peak.set(f_active * fm_exp2f(0.6f + 2.0f * dbl), 0.25f, sr);
                } else {
                    const float fc = filter_cut_hz(v_s);
                    c_one.set(fc, 2.0f - 1.94f * reso, sr);
                    c_peak.set(fc * fm_exp2f(0.6f + 2.0f * dbl), 0.25f, sr);
                    act = 1.0f;
                }
            }
            sub = (sub + 1) & 15;
            if (type == EPK_FLT_DJ) act = fm_minf(fm_fabsf(v_s - 0.5f) * 10.0f, 1.0f);
            flt_g = glide_to(flt_g, on, step);
            float x[2] = {l[i], r[i]};
            for (int c = 0; c < 2; c++) {
                float a, b, h, y;
                if (type == EPK_FLT_DJ) {                                // LP then HP (paso bajo y luego alto)
                    float a2, b2, h2;
                    lp[c].tick(x[c], c_lp.a1, c_lp.a2, c_lp.a3, c_lp.k, a, b, h);
                    hp[c].tick(a, c_hp.a1, c_hp.a2, c_hp.a3, c_hp.k, a2, b2, h2);
                    y = h2;
                } else {
                    one[c].tick(x[c], c_one.a1, c_one.a2, c_one.a3, c_one.k, a, b, h);
                    const float bn = b * c_one.k;                        // band-pass at unity (normalizado)
                    switch (type) {
                    case EPK_FLT_LP: y = a; break;
                    case EPK_FLT_HP: y = h; break;
                    case EPK_FLT_BP: y = bn; break;
                    case EPK_FLT_NOTCH: y = a + h; break;
                    default:                                             // MORPH: LP -> BP -> HP
                        y = dbl < 0.5f ? a + (bn - a) * dbl * 2.0f : bn + (h - bn) * (dbl - 0.5f) * 2.0f;
                        break;
                    }
                }
                if (dbl > 0.001f && type != EPK_FLT_MORPH) {             // the second peak (segundo pico)
                    float pl, pb, ph;
                    peak[c].tick(x[c], c_peak.a1, c_peak.a2, c_peak.a3, c_peak.k, pl, pb, ph);
                    y += pb * dbl * act * 0.8f;
                }
                x[c] += (y - x[c]) * flt_g;
            }
            l[i] = x[0]; r[i] = x[1];
        }
    }

    // The bus compressor and the tape (compresor y cinta), in FLOW's order
    void dynamics(float *l, float *r, uint32_t n, const float *fx, float sr) {
        const float comp = fx[EPK_FX_COMP], sat = fx[EPK_FX_SAT], step = 1.0f / (0.010f * sr);
        const float comp_on = fx[EPK_FX_ON_COMP] > 0.5f ? 1.0f : 0.0f, sat_on = fx[EPK_FX_ON_SAT] > 0.5f ? 1.0f : 0.0f;
        const int sat_first = fx[EPK_FX_DRIVE_ORDER] > 0.5f;
        // compressor: threshold -8 .. -28 dB, ratio 1.5 .. 7.5, attack 10 ms, release 150 ms, makeup up to 8 dB
        const float thr = -8.0f - 20.0f * comp, slope = 1.0f - 1.0f / (1.5f + 6.0f * comp), makeup = 8.0f * comp;
        const float att = 1.0f - fm_expf(-1.0f / (0.010f * sr)), rel = 1.0f - fm_expf(-1.0f / (0.150f * sr));
        const float drive = 1.0f + 5.0f * sat, bias = 0.12f * sat, out_gain = 1.0f / fm_sqrtf(drive);
        const float tone_a = 1.0f - fm_expf(-6.2831853f * (16000.0f - 9000.0f * sat) / sr);
        const float bias_off = fm_tanhf(bias);
        for (uint32_t i = 0; i < n; i++) {
            comp_g = glide_to(comp_g, comp_on, step);
            sat_g = glide_to(sat_g, sat_on, step);
            float x[2] = {l[i], r[i]};
            for (int pass = 0; pass < 2; pass++) {
                const int do_sat = (pass == 0) == (sat_first != 0);
                if (!do_sat && comp > 0.001f && comp_g > 0.0f) {         // the bus compressor (compresor de bus)
                    const float lvl = fm_maxf(fm_fabsf(x[0]), fm_fabsf(x[1]));
                    env += (lvl - env) * (lvl > env ? att : rel);
                    const float db = 6.0206f * fm_log2f(fm_maxf(env, 1e-5f));
                    const float over = db - thr;
                    float gr = 0.0f;
                    if (over > 3.0f) gr = over * slope;                  // a 6 dB soft knee (rodilla suave)
                    else if (over > -3.0f) gr = slope * (over + 3.0f) * (over + 3.0f) / 12.0f;
                    gain_db = makeup - gr;
                    gr_max = fm_maxf(gr_max, gr * comp_g);
                    const float gn = 1.0f + (fm_exp2f(gain_db * (1.0f / 6.0206f)) - 1.0f) * comp_g;
                    x[0] *= gn; x[1] *= gn;
                }
                if (do_sat && sat > 0.001f && sat_g > 0.0f) {            // tape (cinta): asymmetric, darker on top
                    for (int c = 0; c < 2; c++) {
                        float y = (fm_tanhf(x[c] * drive + bias) - bias_off) * out_gain;
                        tone[c] += (y - tone[c]) * tone_a;
                        const float d = tone[c] - dc_x[c] + 0.9995f * dc_y[c];   // DC blocker (~4 Hz)
                        dc_x[c] = tone[c]; dc_y[c] = fm_flush(d);
                        x[c] += (d - x[c]) * sat_g;
                    }
                }
            }
            l[i] = x[0]; r[i] = x[1];
        }
    }

    void process(float *l, float *r, uint32_t n, const float *fx, float sr) {
        filter(l, r, n, fx, sr);
        dynamics(l, r, n, fx, sr);
    }
};

// ------------------------------------------------------------------ DIGITAL: reduction (reducción)
// Bits: the signal rounded to 2^b steps; frequency: each sample held for 1..32 samples (sin filtro: aliasing a
// propósito, como BR/SRR).
struct Crush {
    float hold_l, hold_r, phase;
    void init() { hold_l = hold_r = 0.0f; phase = 1.0f; }
    void process(float *l, float *r, uint32_t n, const float *fx) {
        const float bits = fx[EPK_FX_BITS], srr = fx[EPK_FX_SRR];
        if (bits < 0.001f && srr < 0.001f) { phase = 1.0f; return; }
        const float steps = fm_exp2f(16.0f - 12.0f * bits), inv = 1.0f / steps;
        const float step = 1.0f / (1.0f + 31.0f * srr * srr);
        for (uint32_t i = 0; i < n; i++) {
            phase += step;
            if (phase >= 1.0f) {
                phase -= 1.0f;
                hold_l = l[i]; hold_r = r[i];
                if (bits >= 0.001f) {
                    hold_l = fm_floorf(hold_l * steps + 0.5f) * inv;
                    hold_r = fm_floorf(hold_r * steps + 0.5f) * inv;
                }
            }
            l[i] = hold_l; r[i] = hold_r;
        }
    }
};

}  // namespace epk
