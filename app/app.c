/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Javier Sánchez */
/* EPICYKLOS, phase 1 (fase 1): the interface (interfaz). Screen, panel map and lights; the sound comes from the DSP
 * core (app/dsp, C++, through dsp/epk.h). Written on fm1-base's app contract (app.h, plat.h); its template was the
 * reference for the contract's use. Part of the unity build (device/unity.c, host/plat_host.c), after the platform:
 * gfx.c and announce.c are in scope.
 * Panel (panel, docs/diseno/decisiones-y-fase0.md):
 *   FX SCL ENV LFO EDIT GLO  select track 1-6 (pista); HOME+track mute. MIX view: track = mute, HOME+track = solo
 *   HOME   tap: ORBIT <-> MIX view (vista); held: shift (mayúsculas)
 *   PLAY   start / stop          SELECT  page (página); HOME+SELECT tempo
 *   PRESETS track level (nivel)   K1-K4   the page's parameters; HOME+K fine (ajuste fino)
 *   OCT-/OCT+ keyboard octave     keys    play the selected track
 *   ALGORITHM, ARP, SEQ, REC, SAVE: later phases (fases siguientes), they say which */
#include "fastmath.h"
#include "dsp/epk.h"

/* ------------------------------------------------------------------ colours (colores) — "Nebulosa" */
#define C_BG RGB(7, 10, 20)
#define C_PANEL RGB(18, 24, 41)
#define C_RULE RGB(43, 51, 80)
#define C_LABEL RGB(110, 121, 153)
#define C_TEXT RGB(221, 227, 240)
#define C_MUTE RGB(255, 90, 110)
#define C_SOLO RGB(242, 193, 78)
static const uint16_t TRACK_C[EPK_TRACKS] = {RGB(255, 90, 110), RGB(242, 193, 78), RGB(79, 209, 197),
                                            RGB(139, 124, 246), RGB(90, 169, 255), RGB(232, 121, 249)};
static uint16_t dim_colour(uint16_t c)               /* a colour at ~35 % (apagado) */
{
    uint32_t r = (c >> 11) & 31u, g = (c >> 5) & 63u, b = c & 31u;
    return (uint16_t)(((r * 9u / 25u) << 11) | ((g * 9u / 25u) << 5) | (b * 9u / 25u));
}

/* ------------------------------------------------------------------ pages (páginas) */
enum { PAGE_SOUND, PAGE_RHYTHM, PAGE_MIX, NPAGES };
static const char *const PAGE_NAME[NPAGES] = {"SONIDO", "RITMO", "MEZCLA"};
static const int8_t PAGE_PARAM[NPAGES][4] = {
    {EPK_P_NOTE, EPK_P_CHARACTER, EPK_P_TONE, EPK_P_DECAY},
    {EPK_P_LENGTH, EPK_P_HITS, EPK_P_ROTATE, EPK_P_DIVISION},
    {EPK_P_LEVEL, EPK_P_PAN, -1, -1},          /* delay / reverb sends: phase 3 (fase 3) */
};
static const char *const PARAM_NAME[EPK_P_COUNT] = {"NOTA", "CARACTER", "TONO", "CAIDA", "LONGITUD", "GOLPES",
                                                    "ROTAR", "DIVISION", "NIVEL", "PAN"};
static const char *const PARAM_HINT[EPK_P_COUNT] = {
    "La nota de la secuencia", "Seno con caida, sierra, ruido", "Hasta donde se abre el filtro",
    "Cuanto dura cada golpe", "Pasos del ciclo de la pista", "Golpes repartidos en el ciclo",
    "Gira el patron en su orbita", "Velocidad de los pasos", "Volumen de la pista", "Izquierda o derecha"};
static const char *const SLOT_LATER[2] = {"ENVIO A", "ENVIO B"};   /* MIX page, phase 3 */
static const char *const DIV_NAME[EPK_DIV_COUNT] = {"1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4"};

/* ------------------------------------------------------------------ state (estado) */
enum { VIEW_ORBIT, VIEW_MIX, NVIEWS };
static const char *const VIEW_NAME[NVIEWS] = {"ORBITA", "MEZCLA"};
static struct {
    int track, page, view, octave;
    uint32_t btn, keys;                    /* held at the last input (pulsados) */
    int home_used;                         /* HOME was a shift while held: no view change on release */
    int key_note[NKEYS];                   /* the note each held key started (-1 none) */
    char msg[2][24];                       /* a short message in the header (mensaje) */
    uint32_t msg_until;
    int pop_param, pop_slot;               /* the knob turned last: its popup (ventana) */
    uint32_t pop_until;
    int steps_seen[EPK_TRACKS];
    uint32_t meter_at;
    float meters[EPK_TRACKS];
    uint8_t dirty_head, dirty_main, dirty_foot;
} ui;

/* ------------------------------------------------------------------ text (texto) */
static void str_cat(char *d, const char *s)
{
    while (*d)
        d++;
    while (*s)
        *d++ = *s++;
    *d = 0;
}
static void int_str(int32_t v, char *b)
{
    char t[12];
    int n = 0;
    uint32_t u = (uint32_t)(v < 0 ? -v : v);
    do
        t[n++] = (char)('0' + u % 10u);
    while ((u /= 10u) && n < 11);
    if (v < 0)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
}
static void note_str(int note, char *b)
{
    static const char *const N[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    char o[8];
    b[0] = 0;
    str_cat(b, N[note % 12]);
    int_str(note / 12 - 1, o);
    str_cat(b, o);
}

/* a parameter's value as the screen shows it (valor en pantalla) */
static void param_str(int track, int p, char *b)
{
    float v = epk_param_get(track, p);
    int steps = epk_param_steps(p);
    int i = steps ? (int)(v * (float)(steps - 1) + 0.5f) : 0;
    b[0] = 0;
    switch (p) {
    case EPK_P_NOTE: note_str(24 + i, b); break;
    case EPK_P_LENGTH: int_str(i + 1, b); break;
    case EPK_P_HITS: {
        int len = epk_length(track);
        int_str((int)(v * (float)len + 0.5f), b);
        break;
    }
    case EPK_P_ROTATE: int_str(i, b); break;
    case EPK_P_DIVISION: str_cat(b, DIV_NAME[i]); break;
    case EPK_P_PAN: {
        int c = (int)(v * 100.0f + 0.5f) - 50;
        if (c == 0)
            str_cat(b, "C");
        else {
            str_cat(b, c < 0 ? "L" : "R");
            int_str(c < 0 ? -c : c, b + 1);
        }
        break;
    }
    default: int_str((int)(v * 100.0f + 0.5f), b); break;
    }
}

/* ------------------------------------------------------------------ messages and the screen reader (lector) */
void app_say(const char *a, const char *b)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
    ui.dirty_head = 1;
    if (ann_on()) {
        t[0] = 0;
        str_cat(t, ui.msg[0]);
        if (ui.msg[1][0]) {
            str_cat(t, " ");
            str_cat(t, ui.msg[1]);
        }
        ann_say(t, 0, ANN_NOW);
    }
}

static void speak(void)                   /* a reader that starts listening hears where it is (dónde está) */
{
    char t[96], n[12];
    if (ann_on() && ann_where()) {
        t[0] = 0;
        str_cat(t, APP_NAME ". ");
        str_cat(t, VIEW_NAME[ui.view]);
        str_cat(t, ". PISTA ");
        int_str(ui.track + 1, n);
        str_cat(t, n);
        str_cat(t, ". ");
        str_cat(t, PAGE_NAME[ui.page]);
        str_cat(t, ". TEMPO ");
        int_str((int32_t)(epk_tempo() + 0.5f), n);
        str_cat(t, n);
        ann_say(t, 0, ANN_NOW);
    }
    ann_tick(plat_ms());
}

static void say_later(const char *what, const char *phase) { app_say(what, phase); }

/* ------------------------------------------------------------------ audio (audio) */
static float buf_l[APP_BLOCK], buf_r[APP_BLOCK], gain_now;

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t i;
    const float gain = (float)master_q12 / 4096.0f * 8300000.0f;   /* MASTER, read once a block (una vez por bloque) */
    epk_render(buf_l, buf_r, n);
    for (i = 0; i < n; i++) {
        gain_now += (gain - gain_now) * 0.004f;                     /* ~5 ms glide: MASTER does not click */
        lr[2 * i] = (int32_t)(buf_l[i] * gain_now);
        lr[2 * i + 1] = (int32_t)(buf_r[i] * gain_now);
    }
}

void app_panic(void) { epk_panic(); }

void app_init(void)
{
    uint32_t k;
    epk_init(44100.0f);
    ui.track = 0;
    ui.page = PAGE_SOUND;
    ui.view = VIEW_ORBIT;
    for (k = 0; k < NKEYS; k++)
        ui.key_note[k] = -1;
}

int epk_ble_off(void) { return 0; }       /* Bluetooth MIDI on at boot (settings: fase 6) */

/* ------------------------------------------------------------------ input (entrada) */
static void select_track(int t)
{
    char n[8];
    ui.track = t;
    int_str(t + 1, n);
    app_say("PISTA", n);
    ui.dirty_main = ui.dirty_foot = 1;
}

static void toggle_mute(int t)
{
    char n[8];
    epk_mute(t, !epk_muted(t));
    int_str(t + 1, n);
    app_say(epk_muted(t) ? "MUTE" : "ABIERTA", n);
    ui.dirty_main = 1;
}

static void toggle_solo(int t)
{
    char n[8];
    epk_solo(t, !epk_soloed(t));
    int_str(t + 1, n);
    app_say(epk_soloed(t) ? "SOLO" : "SIN SOLO", n);
    ui.dirty_main = 1;
}

static void turn_param(int slot, int32_t detents, int fine)
{
    int p = PAGE_PARAM[ui.page][slot];
    float v;
    int steps;
    if (p < 0) {
        say_later(SLOT_LATER[slot - 2], "FASE 3");
        return;
    }
    steps = epk_param_steps(p);
    v = epk_param_get(ui.track, p);
    if (p == EPK_P_HITS)                   /* hits: one per detent of this track's length (un golpe por paso) */
        v += (float)detents / (float)(epk_length(ui.track) > 0 ? epk_length(ui.track) : 1);
    else if (steps)
        v += (float)detents / (float)(steps - 1);
    else
        v += (float)detents / (fine ? 400.0f : 64.0f);
    epk_param_set(ui.track, p, fm_clampf(v, 0.0f, 1.0f));
    ui.pop_param = p;
    ui.pop_slot = slot;
    ui.pop_until = plat_ms() + 1000u;
    ui.dirty_main = ui.dirty_foot = 1;
}

static void input(void)
{
    static const int ENC_SLOT[NE] = {-1, -1, -1, 0, 1, 2, 3};
    uint32_t btn = plat_buttons(), keys, k;
    const int shift = (int)(btn >> B_HOME & 1u);
    int32_t e;
    touch_poll();
    keys = touch_keys();
    for (k = 0; k < NKEYS; k++) {          /* the keys play the selected track (el teclado toca la pista) */
        uint32_t bit = 1u << k;
        if ((keys & bit) && !(ui.keys & bit)) {
            int note = 53 + (int)k + 12 * ui.octave;
            ui.key_note[k] = note;
            epk_note_on(ui.track, note, (float)touch_vel(k) / 127.0f);
            plat_midi_send(0x09u | 0x90u << 8 | (uint32_t)note << 16 | (uint32_t)touch_vel(k) << 24, TO_USB | TO_BLE);
        } else if (!(keys & bit) && (ui.keys & bit) && ui.key_note[k] >= 0) {
            epk_note_off(ui.track, ui.key_note[k]);
            plat_midi_send(0x08u | 0x80u << 8 | (uint32_t)ui.key_note[k] << 16, TO_USB | TO_BLE);
            ui.key_note[k] = -1;
        }
    }
    for (k = 0; k < NB; k++) {             /* buttons: on the press (al pulsar) */
        uint32_t bit = 1u << k;
        if (!(btn & bit) || (ui.btn & bit))
            continue;
        if (k != B_HOME && shift)
            ui.home_used = 1;
        if (k <= B_GLO) {                  /* FX SCL ENV LFO EDIT GLO: the six tracks */
            int t = (int)k;
            if (ui.view == VIEW_MIX)
                shift ? toggle_solo(t) : toggle_mute(t);
            else
                shift ? toggle_mute(t) : select_track(t);
        } else if (k == B_HOME) {
            ui.home_used = 0;
        } else if (k == B_PLAY) {
            if (shift)
                say_later("FILL", "FASE 4");
            else {
                epk_play(!epk_playing());
                app_say(epk_playing() ? "STOP" : "PLAY", "");
            }
        } else if (k == B_OCTDN || k == B_OCTUP) {
            int o = clamp(ui.octave + (k == B_OCTUP ? 1 : -1), -2, 2);
            char t[6];
            ui.octave = o;
            t[0] = o < 0 ? '-' : '+';
            t[1] = (char)('0' + (o < 0 ? -o : o));
            t[2] = 0;
            app_say("OCTAVA", t);
        } else if (k == B_REC)
            say_later(shift ? "FREEZE" : "THROW", "FASE 3");
        else if (k == B_SEQ)
            say_later("PASOS", "FASE 2");
        else if (k == B_ARP)
            say_later(shift ? "DESHACER" : "GENERAR", "FASE 4");
        else if (k == B_SAVE)
            say_later(shift ? "GLOBAL" : "PROYECTOS", "FASE 6");
    }
    if (!(btn >> B_HOME & 1u) && (ui.btn >> B_HOME & 1u)) {   /* HOME released: a tap changes the view (vista) */
        if (!ui.home_used) {
            ui.view = (ui.view + 1) % NVIEWS;
            if (ui.view == VIEW_MIX)
                ui.page = PAGE_MIX;
            app_say(VIEW_NAME[ui.view], "");
            ui.dirty_main = ui.dirty_foot = 1;
        }
        ui.home_used = 0;
    }
    for (k = 0; k < NE; k++) {
        if ((e = plat_enc((int)k)) == 0)
            continue;
        if (shift)
            ui.home_used = 1;
        if (k == EN_SELECT) {
            if (shift) {                   /* HOME+SELECT: tempo */
                char t[8];
                epk_set_tempo(epk_tempo() + (float)e);
                int_str((int32_t)(epk_tempo() + 0.5f), t);
                app_say("TEMPO", t);
            } else {
                ui.page = ((ui.page + (int)e) % NPAGES + NPAGES) % NPAGES;
                app_say("PAGINA", PAGE_NAME[ui.page]);
                ui.dirty_foot = 1;
            }
        } else if (k == EN_ALGO) {
            say_later("MUTAR", "FASE 4");
        } else if (k == EN_PRESET) {
            if (shift)
                say_later("ENVIO A", "FASE 3");
            else {                         /* PRESETS: the track's level, from any page */
                int saved = ui.page;
                ui.page = PAGE_MIX;
                turn_param(0, e, 0);
                ui.page = saved;
            }
        } else
            turn_param(ENC_SLOT[k], e, shift);
    }
    ui.keys = keys;
    ui.btn = btn;
}

void app_input_only(void) { input(); }

static void set_lights(void)
{
    uint32_t lit = 1u << ui.track, dim = 0, t;
    for (t = 0; t < EPK_TRACKS; t++)
        if (epk_audible((int)t))
            dim |= 1u << t;
    if (epk_playing())
        lit |= 1u << B_PLAY | LED_PLAY_GREEN;
    if (ui.btn >> B_HOME & 1u)
        lit |= 1u << B_HOME;
    plat_lights(lit, ui.keys, dim, 0, 1);
}

/* ------------------------------------------------------------------ drawing (dibujo) */
static void draw_ring(int32_t cx, int32_t cy, int32_t r, uint16_t c)   /* a dotted circle (círculo punteado) */
{
    int32_t n = r * 2 + 8, i;
    for (i = 0; i < n; i++) {
        float a = (float)i / (float)n * 6.2831853f;
        cv_pset(cx + (int32_t)(fm_cosf(a) * (float)r + 0.5f), cy + (int32_t)(fm_sinf(a) * (float)r + 0.5f), c);
    }
}

static void draw_head(void)
{
    char t[16];
    uint32_t now = plat_ms();
    int32_t x;
    cv_begin(240, 24, C_BG);
    if (now < ui.msg_until) {
        x = cv_text(8, 4, &FONT_B, ui.msg[0], C_TEXT);
        cv_text(x + 8, 4, &FONT_B, ui.msg[1], TRACK_C[ui.track]);
    } else {
        x = cv_text(8, 4, &FONT_B, VIEW_NAME[ui.view], C_TEXT);
        int_str(ui.track + 1, t);
        x = cv_text(x + 10, 4, &FONT_B, t, TRACK_C[ui.track]);
        cv_text(x + 4, 7, &FONT_XS, "PRUEBA", C_LABEL);
    }
    int_str((int32_t)(epk_tempo() + 0.5f), t);
    x = 232 - text_w(&FONT_B, t);
    cv_text(x, 4, &FONT_B, t, C_TEXT);
    if (epk_playing()) {                   /* a play triangle (triángulo) */
        int32_t i;
        for (i = 0; i < 7; i++)
            cv_rect(x - 14 + i, 8 + i / 2, 1, 9 - (i / 2) * 2, TRACK_C[ui.track]);
    } else
        cv_rect(x - 14, 8, 7, 8, C_LABEL);
    cv_rect(0, 23, 240, 1, C_RULE);
    cv_blit(0, 0);
}

static void draw_orbit(void)
{
    const int32_t cx = 120, cy = 72;
    int t;
    char s[8];
    for (t = 0; t < EPK_TRACKS; t++) {
        const int32_t r = 69 - t * 10;
        const int len = epk_length(t), cur = epk_step(t), on = epk_audible(t);
        const uint32_t pat = epk_pattern(t);
        const uint16_t col = on ? TRACK_C[t] : dim_colour(dim_colour(TRACK_C[t]));
        int s_i;
        draw_ring(cx, cy, r, t == ui.track ? dim_colour(TRACK_C[t]) : C_RULE);
        for (s_i = 0; s_i < len; s_i++) {
            float a = (float)s_i / (float)len * 6.2831853f - 1.5707963f;
            int32_t x = cx + (int32_t)(fm_cosf(a) * (float)r + 0.5f), y = cy + (int32_t)(fm_sinf(a) * (float)r + 0.5f);
            int32_t sz = t == ui.track ? 5 : 4;
            if (s_i == cur)
                cv_rect(x - 3, y - 3, 7, 7, (pat >> s_i & 1u) && on ? C_TEXT : C_LABEL);
            else if (pat >> s_i & 1u)
                cv_rect(x - sz / 2, y - sz / 2, sz, sz, col);
        }
    }
    int_str(ui.track + 1, s);                  /* the centre: the selected track (pista elegida) */
    cv_text(cx - text_w(&FONT_B, s) / 2, cy - 8, &FONT_B, s, TRACK_C[ui.track]);
}

static void draw_mixer(void)
{
    int t;
    char s[8];
    for (t = 0; t < EPK_TRACKS; t++) {
        const int32_t x = 10 + t * 38, top = 22, h = 92;
        const int32_t lv = (int32_t)(epk_param_get(t, EPK_P_LEVEL) * (float)h + 0.5f);
        const int32_t mt = (int32_t)(fm_minf(ui.meters[t] * 1.6f, 1.0f) * (float)h + 0.5f);
        const int on = epk_audible(t);
        int_str(t + 1, s);
        cv_text(x + 10 - text_w(&FONT_S, s) / 2, 2, &FONT_S, s, t == ui.track ? C_TEXT : TRACK_C[t]);
        cv_rect(x, top, 20, h, C_PANEL);
        cv_rect(x, top + h - lv, 20, lv, on ? TRACK_C[t] : dim_colour(TRACK_C[t]));
        cv_rect(x + 22, top + h - mt, 3, mt, C_TEXT);                   /* the meter (medidor) */
        if (t == ui.track) {
            cv_rect(x - 2, top - 2, 24, 1, C_TEXT);
            cv_rect(x - 2, top + h + 1, 24, 1, C_TEXT);
        }
        cv_text(x, top + h + 6, &FONT_XS, "M", epk_muted(t) ? C_MUTE : C_RULE);
        cv_text(x + 12, top + h + 6, &FONT_XS, "S", epk_soloed(t) ? C_SOLO : C_RULE);
    }
}

static void draw_popup(void)                     /* the knob just turned, big (el knob girado, en grande) */
{
    char v[16];
    const int p = ui.pop_param, steps = epk_param_steps(p);
    const float val = epk_param_get(ui.track, p);
    const uint16_t c = TRACK_C[ui.track];
    int32_t w;
    cv_rect(12, 8, 216, 128, C_PANEL);
    cv_rect(12, 8, 216, 1, c);
    cv_rect(12, 135, 216, 1, c);
    cv_text(120 - text_w(&FONT_S, PARAM_NAME[p]) / 2, 16, &FONT_S, PARAM_NAME[p], c);
    param_str(ui.track, p, v);
    w = text_w(&FONT_L, v);
    cv_text(120 - w / 2, 40, &FONT_L, v, C_TEXT);
    cv_rect(28, 90, 184, 4, C_RULE);
    if (steps)
        cv_rect(28 + (int32_t)(val * 180.0f), 86, 4, 12, c);
    else
        cv_rect(28, 90, (int32_t)(val * 184.0f + 0.5f), 4, c);
    cv_text(120 - text_w(&FONT_XS, PARAM_HINT[p]) / 2, 108, &FONT_XS, PARAM_HINT[p], C_LABEL);
}

static void draw_main(void)
{
    cv_begin(240, 144, C_BG);
    if (plat_ms() < ui.pop_until)
        draw_popup();
    else if (ui.view == VIEW_ORBIT)
        draw_orbit();
    else
        draw_mixer();
    cv_blit(0, 24);
}

static void draw_foot(void)                       /* the page tabs and the four knobs (pestañas y knobs) */
{
    const uint16_t c = TRACK_C[ui.track];
    int i;
    int32_t x = 6;
    char v[16];
    cv_begin(240, 72, C_BG);
    cv_rect(0, 0, 240, 1, C_RULE);
    for (i = 0; i < NPAGES; i++) {
        x = cv_text(x, 3, &FONT_XS, PAGE_NAME[i], i == ui.page ? c : C_LABEL) + 12;
    }
    for (i = 0; i < 4; i++) {
        const int p = PAGE_PARAM[ui.page][i];
        const int32_t cx = i * 60 + 2;
        cv_rect(cx, 18, 56, 52, C_PANEL);
        if (p < 0) {
            cv_text(cx + 4, 22, &FONT_XS, SLOT_LATER[i - 2], C_RULE);
            cv_text(cx + 4, 40, &FONT_S, "F3", C_RULE);
            continue;
        }
        cv_text(cx + 4, 22, &FONT_XS, PARAM_NAME[p], C_LABEL);
        param_str(ui.track, p, v);
        cv_text(cx + 4, 36, &FONT_M, v, C_TEXT);
        cv_rect(cx + 4, 63, 48, 3, C_RULE);
        cv_rect(cx + 4, 63, (int32_t)(epk_param_get(ui.track, p) * 48.0f + 0.5f), 3, c);
    }
    cv_blit(0, 168);
}

void app_ui_init(void)
{
    touch_on(1);
    lcd_fill(0, 0, 240, 240, C_BG);
    ui.dirty_head = ui.dirty_main = ui.dirty_foot = 1;
}

void app_frame(void)
{
    static uint32_t last_head;
    uint32_t now = plat_ms(), t;
    input();
    speak();
    set_lights();
    for (t = 0; t < EPK_TRACKS; t++) {        /* a step moved: the orbit is redrawn (paso nuevo) */
        int s = epk_step((int)t);
        if (s != ui.steps_seen[t]) {
            ui.steps_seen[t] = s;
            if (ui.view == VIEW_ORBIT)
                ui.dirty_main = 1;
        }
    }
    if (ui.view == VIEW_MIX && now - ui.meter_at >= 50u) {   /* the meters at 20 fps (medidores) */
        ui.meter_at = now;
        for (t = 0; t < EPK_TRACKS; t++)
            ui.meters[t] = ui.meters[t] * 0.6f + epk_meter((int)t);
        ui.dirty_main = 1;
    }
    if (ui.pop_until && now >= ui.pop_until) {
        ui.pop_until = 0;
        ui.dirty_main = 1;
    }
    if ((ui.msg_until && now >= ui.msg_until) || (uint32_t)epk_playing() != last_head) {
        if (now >= ui.msg_until)
            ui.msg_until = 0;
        last_head = (uint32_t)epk_playing();
        ui.dirty_head = 1;
    }
    if (ui.dirty_head) {
        ui.dirty_head = 0;
        draw_head();
    }
    if (ui.dirty_main) {
        ui.dirty_main = 0;
        draw_main();
    }
    if (ui.dirty_foot) {
        ui.dirty_foot = 0;
        draw_foot();
    }
}

#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    if (!strcmp(what, "track"))
        *got = ui.track;
    else if (!strcmp(what, "view"))
        *got = ui.view;
    else if (!strcmp(what, "page"))
        *got = ui.page;
    else if (!strcmp(what, "octave"))
        *got = ui.octave;
    else if (!strcmp(what, "playing"))
        *got = epk_playing();
    else if (!strcmp(what, "tempo"))
        *got = (int)(epk_tempo() + 0.5f);
    else if (!strncmp(what, "mute", 4))       /* muteN, soloN, stepN, levelN (x100), trigN, trigN_K: track N */
        *got = epk_muted(atoi(what + 4) - 1);
    else if (!strncmp(what, "solo", 4))
        *got = epk_soloed(atoi(what + 4) - 1);
    else if (!strncmp(what, "step", 4))
        *got = epk_step(atoi(what + 4) - 1);
    else if (!strncmp(what, "level", 5))
        *got = (int)(epk_param_get(atoi(what + 5) - 1, EPK_P_LEVEL) * 100.0f + 0.5f);
    else if (!strncmp(what, "trig", 4)) {     /* trigN: triggers; trigN_K: 1 if at least K (al menos K) */
        const char *u = strchr(what, '_');
        int n = (int)epk_triggers(atoi(what + 4) - 1);
        *got = u ? n >= atoi(u + 1) : n;
    }
    else
        return 0;
    return 1;
}
#endif
