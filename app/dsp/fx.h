// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Javier Sánchez
// EPICYKLOS send effects (efectos de envío): bus A, a delay with three types (TAPE, BBD, PINGPONG); bus B, PLACA,
// a plate reverb in the manner of Valley's Plateau, or NUBE, a granular cloud (nube de granos) on the same buffer. Both run at half the sample rate (a mitad de frecuencia) on 16-bit buffers, to fit the FM-1's
// RAM: the delay 66 KB (1.5 s), bus B 88 KB (PLACA up to twice Dattorro's size plus 500 ms of pre-delay; NUBE 2 s).
// PLACA follows the plate topology Jon Dattorro published ("Effect Design, Part 1", J. Audio Eng. Soc., 1997): four
// input diffusers and a figure-of-eight tank with two modulated all-passes. The delay lengths are his, scaled from
// 29761 Hz to 22050 Hz. Written for this project; no code from other implementations.
// Rules: no heap, float only, no 64-bit multiplies.
#pragma once
#include "epk.h"
#include "fastmath.h"
#include "voices.h"

// big buffers go in the device's pool RAM, not in .bss (los buffers grandes van al pool de RAM)
// The simulator, the emulator and the tests build with PLAT_HOST; the firmware build is the one without it.
#ifdef PLAT_HOST
#define EPK_POOL
#else
#define EPK_POOL __attribute__((section(".pool")))
#endif

namespace epk {

constexpr int kDelayLen = 33075;                       // 1.5 s at 22050 Hz
constexpr int kDelayHalf = kDelayLen / 2;              // PINGPONG: one line per side
static int16_t g_delay_buf[kDelayLen] EPK_POOL;

// ------------------------------------------------------------------ bus A: the delay (eco)
// freeze (congelar): the input is cut and the loop writes back what it reads, untouched: it holds until released
// (linear interpolation still darkens it slowly, over tens of repeats).
// fx[EPK_FX_DLY_*]: TYPE, TIME (1..16 sixteenths; halved until it fits the buffer), FEEDBACK (past 0.9 it feeds
// itself: the dub siren, held by the saturation), TONE (the loop's low-pass), WEAR (desgaste: TAPE wow, flutter and
// drive; BBD a darker, noisier clock; PINGPONG a softer top).
struct Delay {
    int w, type;
    float cur;                                         // the delay now, in half-rate samples (suavizado)
    float lp, hp, wow, flutter;
    Rng rng;

    void clear() {
        for (int i = 0; i < kDelayLen; i++) g_delay_buf[i] = 0;
        lp = hp = 0.0f;
    }
    void init() { w = 0; type = -1; cur = 0.0f; wow = flutter = 0.0f; rng.init(0x51ED270Bu); clear(); }

    static inline float rd(int base, int len, int w, float delay) {      // linear interpolation (interpolación)
        const int whole = (int)delay;
        const float frac = delay - (float)whole;
        int i0 = w - whole, i1;
        if (i0 < 0) i0 += len;
        i1 = i0 - 1;
        if (i1 < 0) i1 += len;
        const float a = (float)g_delay_buf[base + i0], b = (float)g_delay_buf[base + i1];
        return (a + (b - a) * frac) * (1.0f / 32767.0f);
    }
    static inline int16_t pack(float x) { return (int16_t)(fm_clampf(x, -1.0f, 1.0f) * 32767.0f); }

    // one half-rate sample in, a stereo pair out (una muestra a mitad de frecuencia)
    void tick(float in, const float *fx, float bpm, float hsr, int freeze, float &out_l, float &out_r) {
        const int ty = clampi((int)(fx[EPK_FX_DLY_TYPE] * (float)(EPK_DLY_COUNT - 1) + 0.5f), 0, EPK_DLY_COUNT - 1);
        if (ty != type) { type = ty; clear(); w = 0; cur = 0.0f; }      // another machine: an empty tape
        const int len = type == EPK_DLY_PINGPONG ? kDelayHalf : kDelayLen;
        const float wear = fx[EPK_FX_DLY_WEAR];
        const int steps = 1 + (int)(fx[EPK_FX_DLY_TIME] * 15.0f + 0.5f);
        float target = (float)steps * hsr * 15.0f / bpm;                // a sixteenth = 15 / bpm seconds
        const float longest = (float)(len - 8) * 0.98f;
        while (target > longest) target *= 0.5f;
        if (cur <= 0.0f) cur = target;
        // the time glides: the pitch bends like a tape changing speed (el tono se desliza)
        cur += (target - cur) * (type == EPK_DLY_TAPE ? 0.0004f : type == EPK_DLY_BBD ? 0.001f : 0.002f);
        float delay = cur;
        if (type == EPK_DLY_TAPE) {
            wow += 0.7f / hsr;
            if (wow >= 1.0f) wow -= 1.0f;
            flutter += 6.3f / hsr;
            if (flutter >= 1.0f) flutter -= 1.0f;
            delay *= 1.0f + wear * (0.004f * fm_sin_turns(wow) + 0.0008f * fm_sin_turns(flutter));
        }
        if (delay < 2.0f) delay = 2.0f;
        if (delay > (float)(len - 4)) delay = (float)(len - 4);

        float fc = 400.0f * fm_exp2f(fx[EPK_FX_DLY_TONE] * 4.5f);       // 400 Hz .. 9 kHz
        if (type == EPK_DLY_BBD) fc *= fm_clampf(0.25f * hsr / cur, 0.25f, 1.0f) * (1.0f - 0.5f * wear);   // long = dark
        else fc *= 1.0f - 0.45f * wear;
        const float a = 1.0f - fm_expf(-6.2831853f * fm_minf(fc, hsr * 0.45f) / hsr);
        const float fb = fx[EPK_FX_DLY_FEEDBACK] * 1.1f;
        const float drive = type == EPK_DLY_TAPE ? 1.0f + 1.5f * wear : 1.0f;

        if (freeze) {                                                   // the loop holds (el bucle se sostiene)
            if (type == EPK_DLY_PINGPONG) {
                const float yl = rd(0, len, w, delay), yr = rd(kDelayHalf, len, w, delay);
                g_delay_buf[w] = pack(yr);
                g_delay_buf[kDelayHalf + w] = pack(yl);
                out_l = yl; out_r = yr;
            } else {
                const float y = rd(0, len, w, delay);
                g_delay_buf[w] = pack(y);
                out_l = out_r = y;
            }
        } else if (type == EPK_DLY_PINGPONG) {                          // left line, then right line, and back
            const float yl = rd(0, len, w, delay), yr = rd(kDelayHalf, len, w, delay);
            lp = fm_flush(lp + (yr - lp) * a);
            hp = fm_flush(hp + (lp - hp) * 0.025f);
            g_delay_buf[w] = pack(fm_tanhf(in + (lp - hp) * fb));
            g_delay_buf[kDelayHalf + w] = pack(yl);
            out_l = yl; out_r = yr;
        } else {
            const float y = rd(0, len, w, delay);
            lp = fm_flush(lp + (y - lp) * a);
            hp = fm_flush(hp + (lp - hp) * 0.025f);                     // ~90 Hz: the lows do not pile up
            float x = in + (lp - hp) * fb;
            if (type == EPK_DLY_BBD) x += rng.bipolar() * 0.0015f * (0.3f + wear);   // the clock's hiss (soplido)
            g_delay_buf[w] = pack(fm_tanhf(x * drive) / drive);
            out_l = out_r = y;
        }
        if (++w >= len) w = 0;
    }
};

// ------------------------------------------------------------------ bus B: PLACA (reverb de placa)
// The plate the way Valley's Plateau (Dale Johnson) plays it: Dattorro's figure-of-eight tank with PRE-DELAY, SIZE
// (the tank's lengths scaled), input DIFFUSION, a low cut into the tank and a FREEZE that holds the tail. Plateau's
// controls, our own code at half rate on 16 bits; SIZE goes from a quarter of Dattorro's tank to twice it, and a
// SHIMMER pitches the tail up (an octave, a fifth...) and feeds it back in, so it climbs (sube).
// The lines, in order, inside one buffer: 4 input diffusers; left half: modulated all-pass, delay, all-pass, delay;
// right half the same; then the pre-delay. Values are stored at half scale (headroom for the all-passes).
constexpr int kVerbLines = 13;
constexpr int kVerbPre = 11025;                // up to 500 ms of pre-delay (predelay)
// Dattorro's lengths at 22050 Hz (the base, size 1); the tank's lines get room for twice that (tamaño x2)
constexpr int kVerbBase[kVerbLines] = {105, 79, 281, 205, 512, 3300, 1334, 2757, 688, 3125, 1968, 2344, kVerbPre};
constexpr int kVerbLineLen[kVerbLines] = {105, 79, 281, 205, 1024, 6600, 2668, 5514, 1376, 6250, 3936, 4688, kVerbPre};
constexpr int kVerbLen = 105 + 79 + 281 + 205 + 1024 + 6600 + 2668 + 5514 + 1376 + 6250 + 3936 + 4688 + kVerbPre;
constexpr float kVerbMaxSize = 2.0f;
constexpr int kNubeLen = 44100;                // NUBE: 2 s at the half rate, mono, 16-bit (88 KB)
constexpr int kBusBLen = kNubeLen > kVerbLen ? kNubeLen : kVerbLen;
static_assert(kVerbLen <= kNubeLen, "PLACA must fit in NUBE's buffer: no more memory (sin más memoria)");
static int16_t g_verb_buf[kBusBLen] EPK_POOL;  // one buffer for bus B: PLACA or NUBE, never both (nunca las dos)
static int16_t g_shim_buf[2048] EPK_POOL;      // the shimmer's window (ventana del shimmer), the big plate's only
// NUBE's own small plate after the grains (la reverb de la nube, como la de Clouds): the same tank at 0.4 of the size
constexpr int kNVerbLineLen[kVerbLines] = {105, 79, 281, 205, 512, 1320, 534, 1103, 688, 1250, 788, 938, 1};
constexpr int kNVerbLen = 105 + 79 + 281 + 205 + 512 + 1320 + 534 + 1103 + 688 + 1250 + 788 + 938 + 1;
static int16_t g_nverb_buf[kNVerbLen] EPK_POOL;

struct Plate {
    enum { IN1, IN2, IN3, IN4, L_MOD, L_DL1, L_AP, L_DL2, R_MOD, R_DL1, R_AP, R_DL2, PRE };
    int off[kVerbLines], w[kVerbLines], len[kVerbLines];
    float l_fb, r_fb, l_damp, r_damp, lfo, scale;
    // the shimmer's pitch shifter (desplazador de tono): two taps crossfading over a short window
    int16_t *buf, *shim_buf;                  // its lines, and the shimmer's window (null: no shimmer)
    const int *maxlen;
    int buf_len;
    int shim_w;
    float shim_ph, shim_lp;

    void clear() {
        for (int i = 0; i < buf_len; i++) buf[i] = 0;
        l_fb = r_fb = l_damp = r_damp = shim_lp = 0.0f;
        if (shim_buf)
            for (int i = 0; i < 2048; i++) shim_buf[i] = 0;
    }
    // an octave (or the interval) up: the read point slides through the window faster than it is written
    inline float shimmer(float x, float ratio) {
        shim_buf[shim_w] = (int16_t)(fm_clampf(x, -1.99f, 1.99f) * 16384.0f);
        shim_ph += (1.0f - ratio) * (1.0f / 2048.0f);                   // the delay moves (el retardo se mueve)
        shim_ph -= fm_floorf(shim_ph);
        float y = 0.0f;
        for (int k = 0; k < 2; k++) {
            float ph = shim_ph + 0.5f * (float)k;
            if (ph >= 1.0f) ph -= 1.0f;
            const float d = 4.0f + ph * 2000.0f;
            float rp = (float)shim_w - d;
            if (rp < 0.0f) rp += 2048.0f;
            const int i0 = (int)rp, i1 = (i0 + 1) & 2047;
            const float fr = rp - (float)i0;
            const float v = ((float)shim_buf[i0] + ((float)shim_buf[i1] - (float)shim_buf[i0]) * fr) * (1.0f / 16384.0f);
            y += v * (1.0f - fm_fabsf(2.0f * ph - 1.0f));              // a triangle each: they sum to one (suman 1)
        }
        shim_w = (shim_w + 1) & 2047;
        return y;
    }
    void init(int16_t *b, const int *lens, int16_t *shim) {
        buf = b; maxlen = lens; shim_buf = shim;
        int o = 0;
        for (int i = 0; i < kVerbLines; i++) { off[i] = o; w[i] = 0; len[i] = lens[i]; o += lens[i]; }
        buf_len = o;
        lfo = 0.0f; scale = -1.0f;                                       // set_size sets it (lo pone set_size)
        shim_w = 0; shim_ph = 0.0f;
        clear();
        set_size(3.0f / 7.0f);
    }
    // SIZE (tamaño): the tank's lines scaled 0.25 .. 2; the diffusers and the pre-delay keep theirs
    void set_size(float v) {
        const float sc = 0.25f + (kVerbMaxSize - 0.25f) * v;
        if (fm_fabsf(sc - scale) < 0.002f) return;
        scale = sc;
        for (int i = L_MOD; i <= R_DL2; i++) {
            if (i == L_MOD || i == R_MOD) continue;                     // these read a moving tap: keep their room
            int n = (int)((float)kVerbBase[i] * sc);
            if (n > maxlen[i]) n = maxlen[i];
            if (n < 16) n = 16;
            len[i] = n;
            if (w[i] >= n) w[i] = 0;
        }
    }
    inline float rd(int line, int back) const {                         // back: 1 .. the line's length
        int i = w[line] - back;
        if (i < 0) i += len[line];
        return (float)buf[off[line] + i] * (1.0f / 16384.0f);
    }
    inline void wr(int line, float x) {
        buf[off[line] + w[line]] = (int16_t)(fm_clampf(x, -1.99f, 1.99f) * 16384.0f);
        if (++w[line] >= len[line]) w[line] = 0;
    }
    inline float allpass(int line, int n, float x, float g) {
        const float d = rd(line, n), v = x - g * d;
        wr(line, v);
        return d + g * v;
    }
    inline float allpass_mod(int line, float n, float x, float g) {      // a length that moves (longitud que se mueve)
        const int whole = (int)n;
        const float frac = n - (float)whole;
        const float d0 = rd(line, whole), d1 = rd(line, whole + 1), d = d0 + (d1 - d0) * frac;
        const float v = x - g * d;
        wr(line, v);
        return d + g * v;
    }
    inline float delay(int line, float x) {
        const float d = rd(line, len[line]);
        wr(line, x);
        return d;
    }
    inline float tap(int line, int at) const {                          // an output tap, scaled with SIZE
        int k = (int)((float)at * scale);
        if (k < 1) k = 1;
        if (k > len[line]) k = len[line];
        return rd(line, k);
    }

    void tick(float in, const float *fx, float hsr, int freeze, float &out_l, float &out_r) {
        const float decay = freeze ? 0.9995f : 0.30f + 0.69f * fx[EPK_FX_REV_DECAY];
        const float open = freeze ? 1.0f : 0.12f + 0.88f * fx[EPK_FX_REV_TONE];   // the tank's low-pass
        const float depth = (1.0f + 10.0f * fx[EPK_FX_REV_MOD]) * (scale > 1.0f ? scale : 1.0f);
        const float diff = fx[EPK_FX_REV_DIFFUSION];
        lfo += (0.4f + 1.1f * fx[EPK_FX_REV_MOD]) / hsr;
        if (lfo >= 1.0f) lfo -= 1.0f;
        // the pre-delay (predelay): 0 .. ~240 ms; frozen, nothing new comes in (congelada, no entra nada)
        float x = freeze ? 0.0f : in * 0.5f;
        const int pre = (int)(fx[EPK_FX_REV_PREDELAY] * (float)(maxlen[PRE] - 1));
        if (pre > 0) {
            wr(PRE, x);
            x = rd(PRE, pre);
        }
        // the shimmer: the tank's last output, pitched, back in (brillo que sube); a gentle low-pass keeps it soft
        const float shim = fx[EPK_FX_REV_SHIMMER];
        if (shim > 0.001f && !freeze && shim_buf) {                               // frozen: nothing new, not even this
            static const float kSemi[EPK_SHIM_COUNT] = {12.0f, 7.0f, 19.0f, 24.0f, -12.0f};
            const int iv = (int)(fx[EPK_FX_REV_SHIM_INT] * (float)(EPK_SHIM_COUNT - 1) + 0.5f);
            const float up = shimmer((l_fb + r_fb) * 0.5f, fm_exp2f(kSemi[iv] * (1.0f / 12.0f)));
            shim_lp = fm_flush(shim_lp + (up - shim_lp) * 0.5f);
            x += fm_tanhf(shim_lp * 0.6f * shim);
        }
        x = allpass(IN1, 105, x, 0.75f * diff);
        x = allpass(IN2, 79, x, 0.75f * diff);
        x = allpass(IN3, 281, x, 0.625f * diff);
        x = allpass(IN4, 205, x, 0.625f * diff);
        // the tank (el tanque): each half feeds the other
        float l = allpass_mod(L_MOD, 498.0f * scale + depth * fm_sin_turns(lfo), x + decay * r_fb, -0.7f);
        l = delay(L_DL1, l);
        l_damp = fm_flush(l_damp + (l - l_damp) * open);
        l = allpass(L_AP, len[L_AP], l_damp * decay, 0.5f);
        const float l_out = delay(L_DL2, l);
        float r = allpass_mod(R_MOD, 673.0f * scale + depth * fm_sin_turns(lfo + 0.25f), x + decay * l_fb, -0.7f);
        r = delay(R_DL1, r);
        r_damp = fm_flush(r_damp + (r - r_damp) * open);
        r = allpass(R_AP, len[R_AP], r_damp * decay, 0.5f);
        const float r_out = delay(R_DL2, r);
        l_fb = l_out; r_fb = r_out;
        // the output taps (tomas de salida), Dattorro's, scaled
        out_l = 1.2f * (tap(R_DL1, 197) + tap(R_DL1, 2204) - tap(R_AP, 1418) + tap(R_DL2, 1479) - tap(L_DL1, 1475) -
                        tap(L_AP, 139) - tap(L_DL2, 790));
        out_r = 1.2f * (tap(L_DL1, 262) + tap(L_DL1, 2688) - tap(L_AP, 910) + tap(L_DL2, 1981) - tap(R_DL1, 1564) -
                        tap(R_AP, 248) - tap(R_DL2, 90));
    }
};

// ------------------------------------------------------------------ bus B, second type: NUBE (nube de granos)
// The idea of Mutable Instruments' Clouds (Émilie Gillet, MIT): grains read from the last second of what the bus
// hears, with position, size, density, pitch, texture and feedback, and a freeze. Written for this project, much
// smaller: 12 grains, mono capture spread in stereo, no spectral modes. No code taken from Clouds.
constexpr int kNubeGrains = 12;
struct Grain { float pos, inc, ph, dph, gl, gr; int on; };
struct Nube {
    Grain gr[kNubeGrains];
    int w;
    float timer, fb, tone_l, tone_r;
    uint32_t rng;
    float rnd() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (float)(rng >> 8) * (1.0f / 16777216.0f); }
    void clear() {
        for (int i = 0; i < kNubeLen; i++) g_verb_buf[i] = 0;
        for (int i = 0; i < kNubeGrains; i++) gr[i].on = 0;
        timer = fb = tone_l = tone_r = 0.0f;
    }
    void init() { w = 0; rng = 0x6A09E667u; clear(); }
    inline float at(float p) const {                                    // p in [0, kNubeLen) (interpolado)
        const int i = (int)p;
        const int j = i + 1 < kNubeLen ? i + 1 : 0;
        const float a = (float)g_verb_buf[i], b = (float)g_verb_buf[j];
        return (a + (b - a) * (p - (float)i)) * (1.0f / 16384.0f);
    }
    void start(float len, float inc, float pos, float jitter) {
        int k = 0;
        while (k < kNubeGrains && gr[k].on) k++;
        if (k == kNubeGrains) return;                                   // all busy: this one is skipped (se salta)
        const float need = len * (inc > 1.0f ? inc : 1.0f);             // what it reads ahead (lo que avanza)
        float back = need + 4.0f + (pos + jitter * (rnd() - 0.5f)) * ((float)kNubeLen - 2.0f * need - 8.0f);
        back = fm_clampf(back, need + 4.0f, (float)kNubeLen - need - 4.0f);
        float p = (float)w - back;
        if (p < 0.0f) p += (float)kNubeLen;
        const float pan = (rnd() - 0.5f) * 1.2f;                        // spread in stereo (abierto)
        Grain &g = gr[k];
        g.pos = p; g.inc = inc; g.ph = 0.0f; g.dph = 1.0f / len;
        g.gl = 0.75f * (1.0f - pan); g.gr = 0.75f * (1.0f + pan); g.on = 1;
    }
    void tick(float in, const float *fx, float hsr, int freeze, float &out_l, float &out_r) {
        const float size = fx[EPK_FX_NUBE_SIZE];
        float len = hsr * (0.02f + 0.98f * size * size);                // ~20 ms .. 1 s
        const int st = (int)(fx[EPK_FX_NUBE_PITCH] * (float)(EPK_NUBE_PITCH_STEPS - 1) + 0.5f) - 24;
        const float inc = fm_exp2f((float)st * (1.0f / 12.0f));
        if (len * (inc > 1.0f ? inc : 1.0f) > (float)kNubeLen * 0.45f) len = (float)kNubeLen * 0.45f / (inc > 1.0f ? inc : 1.0f);
        const float d = (fx[EPK_FX_NUBE_DENSITY] - 0.5f) * 2.0f;        // -1 regular .. 0 none .. +1 random
        const float dd = fm_fabsf(d);
        if (!freeze) {                                                  // write what comes in (graba), with feedback
            const float x = in + fb * (0.9f * fx[EPK_FX_REV_DECAY]);
            g_verb_buf[w] = (int16_t)(fm_clampf(x, -1.99f, 1.99f) * 16384.0f);
            if (++w >= kNubeLen) w = 0;
        }
        if (dd > 0.04f) {                                               // new grains (granos nuevos)
            const float rate = 0.5f + 40.0f * dd * dd;                  // grains a second (por segundo)
            timer -= 1.0f;
            if (timer <= 0.0f) {
                start(len, inc, fx[EPK_FX_NUBE_POS], d > 0.0f ? 0.25f * dd : 0.0f);
                const float mean = hsr / rate;
                timer += d > 0.0f ? mean * (0.1f + 1.8f * rnd()) : mean;  // random or regular (al azar o regular)
            }
        }
        // texture: the window, hard edges (left) .. triangle .. smooth bell (right)
        const float tex = fx[EPK_FX_REV_TONE];
        const float edge = 2.0f + 30.0f * fm_maxf(0.0f, 1.0f - tex * 2.0f);
        const float bell = fm_maxf(0.0f, tex * 2.0f - 1.0f);
        float sl = 0.0f, sr = 0.0f;
        int n = 0;
        for (int k = 0; k < kNubeGrains; k++) {
            Grain &g = gr[k];
            if (!g.on) continue;
            const float tri = 1.0f - fm_fabsf(2.0f * g.ph - 1.0f);
            const float hard = fm_minf(1.0f, tri * edge * 0.5f);
            const float smooth = tri * tri * (3.0f - 2.0f * tri);
            const float win = hard + (smooth - hard) * bell;
            const float s = at(g.pos) * win;
            sl += s * g.gl; sr += s * g.gr;
            n++;
            g.pos += g.inc;
            if (g.pos >= (float)kNubeLen) g.pos -= (float)kNubeLen;
            g.ph += g.dph;
            if (g.ph >= 1.0f) g.on = 0;
        }
        const float norm = 1.0f / fm_sqrtf(1.0f + (float)n * 0.5f);     // many grains: not louder (no sube tanto)
        const float open = 0.25f + 0.75f * tex;                         // smooth grains a little darker (más oscuros)
        tone_l = fm_flush(tone_l + (sl * norm - tone_l) * open);
        tone_r = fm_flush(tone_r + (sr * norm - tone_r) * open);
        out_l = fm_tanhf(tone_l);
        out_r = fm_tanhf(tone_r);
        fb = (out_l + out_r) * 0.5f;
    }
};

// ------------------------------------------------------------------ both buses (los dos envíos)
// Full-rate samples in and out; inside, every second sample runs the effects, and their output is interpolated back.
struct Fx {
    Delay delay;
    Plate plate, nverb;                           // the big plate; the cloud's small one (la de la nube)
    float nfx[EPK_FX_COUNT];                      // the cloud's plate's settings, this block (ajustes por bloque)
    Nube nube;
    int half, freeze, rev_type;
    float acc_a, acc_b;
    float prev_l, prev_r, cur_l, cur_r;
    float duck_env, duck_gain;
    float dly_g, rev_g;                           // FLOW: on 1 .. off 0, gliding ~10 ms (sin clics)

    void init() {
        dly_g = rev_g = 1.0f;
        delay.init(); plate.init(g_verb_buf, kVerbLineLen, g_shim_buf); nverb.init(g_nverb_buf, kNVerbLineLen, nullptr);
        nverb.set_size(0.0857f); nube.init();                          // 0.4 of the plate (0.25 + 1.75 x) b_hp = 0.0f; half = 0; freeze = 0; rev_type = EPK_REV_PLACA;
        acc_a = acc_b = 0.0f; prev_l = prev_r = cur_l = cur_r = 0.0f; duck_env = 0.0f; duck_gain = 1.0f;
    }
    void clear() { delay.clear(); if (rev_type == EPK_REV_NUBE) { nube.clear(); nverb.clear(); } else plate.clear(); acc_a = acc_b = prev_l = prev_r = cur_l = cur_r = 0.0f; }

    float b_hp;                                   // bus B's low cut (corte de graves)
    inline void bus_b(float in, const float *fx, float hsr, float &l, float &r) {
        const float hz = 20.0f * fm_exp2f(4.32f * fx[EPK_FX_REV_LOWCUT]); // ~20 .. ~400 Hz, 90 in the middle
        b_hp = fm_flush(b_hp + (in - b_hp) * (6.2831853f * hz / hsr));
        in -= b_hp;
        if (rev_type == EPK_REV_NUBE) {
            nube.tick(in, fx, hsr, freeze, l, r);
            const float amt = fx[EPK_FX_NUBE_VERB];                     // the cloud into its own reverb (Clouds)
            if (amt > 0.001f) {
                float vl, vr;
                nverb.tick((l + r) * 0.5f, nfx, hsr, freeze, vl, vr);
                l += vl * amt * amt * 1.6f; r += vr * amt * amt * 1.6f;
            }
        } else
            plate.tick(in, fx, hsr, freeze, l, r);
        const float w = fx[EPK_FX_REV_WIDTH] * 2.0f;                   // width (ancho): side scaled 0 .. 2
        const float m = (l + r) * 0.5f, sd = (l - r) * 0.5f * w;
        l = m + sd; r = m - sd;
    }
    // a, b: the send buses; kick: track 1's signal, for the duck (se suman al estéreo l, r)
    void process(const float *a, const float *b, const float *kick, float *l, float *r, uint32_t n, const float *fx,
                 float bpm, float sr) {
        const float hsr = sr * 0.5f;
        const float duck = fx[EPK_FX_DUCK];
        const float duck_rel = fm_expf(-1.0f / (0.15f * sr));            // ~150 ms back up (vuelve en ~150 ms)
        const float dly_level = fx[EPK_FX_DLY_LEVEL] * fx[EPK_FX_DLY_LEVEL] * 1.5f;
        const float rev_level = fx[EPK_FX_REV_LEVEL] * fx[EPK_FX_REV_LEVEL] * 1.5f;
        const float to_rev = fx[EPK_FX_DLY_TO_REV];                     // the cross (cruce), either way
        const int rev_first = fx[EPK_FX_SPACE_ORDER] > 0.5f;
        const float dly_t = fx[EPK_FX_ON_DLY] > 0.5f ? 1.0f : 0.0f, rev_t = fx[EPK_FX_ON_REV] > 0.5f ? 1.0f : 0.0f;
        const float glide = 1.0f / (0.010f * hsr);
        if (dly_t > 0.0f && dly_g <= 0.0f) delay.clear();                // back on: from empty (vuelve vacío)
        const int type = fx[EPK_FX_REV_TYPE] > 0.5f ? EPK_REV_NUBE : EPK_REV_PLACA;
        plate.set_size(fx[EPK_FX_REV_SIZE]);
        if (type != rev_type) {                                          // the other type: its buffer from empty
            rev_type = type;
            if (type == EPK_REV_NUBE) { nube.clear(); nverb.clear(); } else plate.clear();
        }
        for (int i = 0; i < EPK_FX_COUNT; i++) nfx[i] = fx[i];               // the cloud's plate: its own time,
        nfx[EPK_FX_REV_DECAY] = fx[EPK_FX_NUBE_VERB_TIME];                  // a fixed colour and movement
        nfx[EPK_FX_REV_TONE] = 0.55f; nfx[EPK_FX_REV_MOD] = 0.35f;
        nfx[EPK_FX_REV_DIFFUSION] = 1.0f; nfx[EPK_FX_REV_PREDELAY] = 0.0f; nfx[EPK_FX_REV_SHIMMER] = 0.0f;
        if (rev_t > 0.0f && rev_g <= 0.0f) { if (rev_type == EPK_REV_NUBE) { nube.clear(); nverb.clear(); } else plate.clear(); }
        for (uint32_t i = 0; i < n; i++) {
            acc_a += a[i];
            acc_b += b[i];
            if (half) {
                float dl = 0.0f, dr = 0.0f, vl = 0.0f, vr = 0.0f;
                dly_g = dly_g < dly_t ? fm_minf(dly_g + glide, dly_t) : fm_maxf(dly_g - glide, dly_t);
                rev_g = rev_g < rev_t ? fm_minf(rev_g + glide, rev_t) : fm_maxf(rev_g - glide, rev_t);
                const int run_d = dly_g > 0.0f, run_r = rev_g > 0.0f;     // off: not computed (no se calcula)
                if (!rev_first) {
                    if (run_d) delay.tick(acc_a * 0.5f * dly_g, fx, bpm, hsr, freeze, dl, dr);
                    if (run_r) bus_b((acc_b * 0.5f + (dl + dr) * 0.5f * to_rev * dly_g) * rev_g, fx, hsr, vl, vr);
                } else {                                                 // REVERB > ECO: the plate into the delay
                    if (run_r) bus_b(acc_b * 0.5f * rev_g, fx, hsr, vl, vr);
                    if (run_d)
                        delay.tick((acc_a * 0.5f + (vl + vr) * 0.5f * to_rev * rev_g) * dly_g, fx, bpm, hsr, freeze, dl, dr);
                }
                acc_a = acc_b = 0.0f;
                prev_l = cur_l; prev_r = cur_r;
                cur_l = dl * dly_level * dly_g + vl * rev_level * rev_g;
                cur_r = dr * dly_level * dly_g + vr * rev_level * rev_g;
                l[i] += (prev_l + cur_l) * 0.5f * duck_gain;            // halfway to the new value (a medio camino)
                r[i] += (prev_r + cur_r) * 0.5f * duck_gain;
            } else {
                l[i] += cur_l * duck_gain;
                r[i] += cur_r * duck_gain;
            }
            // the duck (el duck): follows the kick, fast down, slow up
            duck_env = fm_flush(fm_maxf(fm_fabsf(kick[i]), duck_env * duck_rel));
            duck_gain += (1.0f - duck * fm_minf(duck_env * 4.0f, 1.0f) - duck_gain) * 0.01f;
            half ^= 1;
        }
    }
};

}  // namespace epk
