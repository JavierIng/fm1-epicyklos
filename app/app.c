/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Javier Sánchez */
/* EPICYKLOS: the interface (interfaz). Screen, panel map and lights; the sound comes from the DSP core (app/dsp,
 * C++, through dsp/epk.h). Written on fm1-base's app contract (app.h, plat.h). Part of the unity build
 * (device/unity.c, host/plat_host.c), after the platform: gfx.c and announce.c are in scope.
 * Panel (panel, docs/diseno/decisiones-y-fase0.md):
 *   FX SCL ENV LFO EDIT GLO  select track 1-6 (pista); HOME+track mute. MIX view: track = mute, HOME+track = solo
 *   HOME   tap: ORBIT -> MIX -> EFFECTS view (vista); held: shift (mayúsculas). In EFFECTS, SELECT and K1-K4 are
 *          the delay's and the reverb's (eco y reverb); HOME+PRESETS the track's send to the delay
 *   PLAY   start / stop          SELECT  page (página); HOME+SELECT tempo
 *   PRESETS track level (nivel)   K1-K4   the page's parameters; HOME+K fine (ajuste fino)
 *   OCT-/OCT+ keyboard octave     keys    play the selected track
 *   SAVE   the PATTERNS view (patrones): black keys = banks A..K (bancos), white keys = the bank's 16 patterns;
 *          playing, the change waits for the bar's end. SAVE held + keys: the same from any view
 *   SEQ    the STEPS view (pasos): the 16 white keys are steps (OCT-/OCT+ the other 16); hold a step and turn
 *          K1 for its chance (probabilidad), K2 for its condition (condición), K3 early or late (micro)
 *   ARP    random (aleatorio): the page shown, for the selected track; in the PATTERNS view, a whole new pattern.
 *          HOME+ARP undo (deshacer). ARP held + ALGORITHM: how much (cantidad). ARP held + a knob: lock it (bloquear)
 *   REC    held: THROW, the selected track to the delay at full; REC held + a track button throws that one too.
 *          HOME+REC: FREEZE the delay (congelar) on / off
 *   ALGORITHM: a later phase (fase siguiente), it says which */
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
#define C_FX RGB(79, 209, 197)                       /* the effects' colour (color de los efectos) */
static const uint16_t TRACK_C[EPK_TRACKS] = {RGB(255, 90, 110), RGB(242, 193, 78), RGB(79, 209, 197),
                                            RGB(139, 124, 246), RGB(90, 169, 255), RGB(232, 121, 249)};
static uint16_t dim_colour(uint16_t c)               /* a colour at ~35 % (apagado) */
{
    uint32_t r = (c >> 11) & 31u, g = (c >> 5) & 63u, b = c & 31u;
    return (uint16_t)(((r * 9u / 25u) << 11) | ((g * 9u / 25u) << 5) | (b * 9u / 25u));
}

/* ------------------------------------------------------------------ pages (páginas) */
enum { PAGE_SOUND, PAGE_ENGINE, PAGE_RHYTHM, PAGE_GROOVE, PAGE_MIX, NPAGES };
enum { SLOT_NONE = -1, SLOT_LATER = -2 };            /* nothing there / a later phase (fase siguiente) */
static const char *const PAGE_NAME[NPAGES] = {"SONIDO", "MOTOR", "RITMO", "GROOVE", "MEZCLA"};
static const int8_t PAGE_PARAM[NPAGES][4] = {
    {EPK_P_NOTE, EPK_P_CHARACTER, EPK_P_TONE, EPK_P_DECAY},
    {EPK_P_ENGINE, EPK_P_EXTRA1, EPK_P_EXTRA2, EPK_P_ATTACK},
    {EPK_P_LENGTH, EPK_P_HITS, EPK_P_ROTATE, EPK_P_DIVISION},
    {EPK_P_SWING, EPK_P_SHIFT, SLOT_NONE, SLOT_NONE},
    {EPK_P_LEVEL, EPK_P_PAN, EPK_P_SEND_A, EPK_P_SEND_B},   /* sends: A the delay, B the reverb */
};
static const char *const ENG_NAME[EPK_ENG_COUNT] = {"PRUEBA", "GRAVE", "METAL", "GOLPE", "TAMBOR"};
/* what NOTE, CHARACTER, TONE, DECAY, EXTRA1, EXTRA2, ATTACK are in each engine (0 = unused) */
static const char *const SND_NAME[EPK_ENG_COUNT][7] = {
    {"NOTA", "CARACTER", "TONO", "CAIDA", 0, 0, "ATAQUE"},
    {"NOTA", "DRIVE", "CLICK", "CAIDA", "BARRIDO", "RUMBLE", 0},
    {"AFINA", "METAL", "BRILLO", "CAIDA", "ANILLO", "VARIA", 0},
    {"AFINA", "TIPO", "BRILLO", "CAIDA", "CUERPO", "COLA", 0},
    {"AFINA", "TIMBRE", "FM", "CAIDA", "BARRIDO", "CLICK", 0},
};
static const char *const SND_HINT[EPK_ENG_COUNT][7] = {
    {"La nota de la secuencia", "Seno con caida, sierra, ruido", "Hasta donde se abre el filtro",
     "Cuanto dura cada golpe", 0, 0, "Entra de golpe o poco a poco"},
    {"La nota del bombo", "Saturacion: limpio a industrial", "El chasquido del ataque", "Cuanto dura el bombo",
     "Cuanto cae el tono al golpear", "La cola grave entre bombos", 0},
    {"Afinacion del metal", "De ruido (909) a metal (808)", "Mas oscuro o mas brillante", "Cerrado o abierto",
     "Resonancia: mas campana", "Cada golpe dura distinto", 0},
    {"Afinacion del cuerpo", "Palma, caja o aro", "Mas oscuro o mas brillante", "Cuanto dura el golpe",
     "Cuerpo afinado; en palma, un golpe grave", "Cola de ruido; abre la palma", 0},
    {"Afinacion del tambor", "De tom redondo a metalico", "Brillo del ataque", "Cuanto dura el golpe",
     "Cuanto cae el tono al golpear", "El chasquido del ataque", 0},
};
static const char *const DIV_NAME[EPK_DIV_COUNT] = {"1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4"};
static const char *const PROB_NAME[EPK_PROB_COUNT] = {"100%", "75%", "50%", "25%"};
static const char *const COND_NAME[EPK_COND_COUNT] = {"SIEMPRE", "1:2", "2:2", "1:4", "2:4", "3:4", "4:4"};

static int snd_index(int p)                          /* a sound parameter's column in SND_NAME, -1 if not one */
{
    return p <= EPK_P_DECAY ? p : p == EPK_P_EXTRA1 ? 4 : p == EPK_P_EXTRA2 ? 5 : p == EPK_P_ATTACK ? 6 : -1;
}
static const char *param_name(int track, int p)
{
    switch (p) {
    case EPK_P_ENGINE: return "MOTOR";
    case EPK_P_LENGTH: return "LONGITUD";
    case EPK_P_HITS: return "GOLPES";
    case EPK_P_ROTATE: return "ROTAR";
    case EPK_P_DIVISION: return "DIVISION";
    case EPK_P_SWING: return "SWING";
    case EPK_P_SHIFT: return "DESPLAZA";
    case EPK_P_LEVEL: return "NIVEL";
    case EPK_P_PAN: return "PAN";
    case EPK_P_SEND_A: return "A ECO";
    case EPK_P_SEND_B: return "B REVERB";
    default: return SND_NAME[epk_engine(track)][snd_index(p)];
    }
}
static const char *param_hint(int track, int p)
{
    switch (p) {
    case EPK_P_ENGINE: return "El motor de sonido de la pista";
    case EPK_P_LENGTH: return "Pasos del ciclo de la pista";
    case EPK_P_HITS: return "Golpes repartidos en el ciclo";
    case EPK_P_ROTATE: return "Gira el patron en su orbita";
    case EPK_P_DIVISION: return "Velocidad de los pasos";
    case EPK_P_SWING: return "Los pasos impares se atrasan";
    case EPK_P_SHIFT: return "La pista entera antes o despues";
    case EPK_P_LEVEL: return "Volumen de la pista";
    case EPK_P_PAN: return "Izquierda o derecha";
    case EPK_P_SEND_A: return "Cuanto va al eco";
    case EPK_P_SEND_B: return "Cuanto va a la reverb";
    default: return SND_HINT[epk_engine(track)][snd_index(p)];
    }
}
/* the parameter of a page's knob for a track: SLOT_NONE where the engine has nothing there */
static int slot_param(int track, int page, int slot)
{
    int p = PAGE_PARAM[page][slot];
    if (p >= 0 && snd_index(p) >= 0 && !SND_NAME[epk_engine(track)][snd_index(p)])
        return SLOT_NONE;
    return p;
}

/* ------------------------------------------------------------------ the effects' pages (páginas de EFECTOS) */
enum { NFXPAGES = 3 };
static const char *const FX_PAGE_NAME[NFXPAGES] = {"ECO", "ECO 2", "REVERB"};
static const int8_t FX_PAGE_PARAM[NFXPAGES][4] = {
    {EPK_FX_DLY_TYPE, EPK_FX_DLY_TIME, EPK_FX_DLY_FEEDBACK, EPK_FX_DLY_TONE},
    {EPK_FX_DLY_WEAR, EPK_FX_DLY_TO_REV, EPK_FX_DLY_LEVEL, EPK_FX_DUCK},
    {EPK_FX_REV_DECAY, EPK_FX_REV_TONE, EPK_FX_REV_MOD, EPK_FX_REV_LEVEL},
};
static const char *const FX_NAME[EPK_FX_COUNT] = {"TIPO", "TIEMPO", "REPITE", "TONO", "DESGASTE", "A REVERB", "NIVEL",
                                                  "CAIDA", "TONO", "MOVER", "NIVEL", "DUCK"};
static const char *const FX_HINT[EPK_FX_COUNT] = {
    "Cinta, analogico o de lado a lado", "En semicorcheas: 3, 5, 7...", "Arriba del todo se realimenta solo",
    "Cada repeticion mas oscura", "Cinta gastada / reloj sucio", "El eco entra en la reverb", "Retorno del eco",
    "Cuanto dura la cola", "Oscura o brillante", "Movimiento dentro de la placa", "Retorno de la reverb",
    "El bombo aparta eco y reverb"};
static const char *const DLY_NAME[EPK_DLY_COUNT] = {"TAPE", "BBD", "PINGPONG"};

/* ------------------------------------------------------------------ state (estado) */
enum { VIEW_ORBIT, VIEW_MIX, VIEW_FX, NVIEWS, VIEW_PATTERNS = NVIEWS, VIEW_STEPS };   /* HOME goes round the first three */
static const char *const VIEW_NAME[NVIEWS + 2] = {"ORBITA", "MEZCLA", "EFECTOS", "PATRONES", "PASOS"};
/* the keys, F3..G5: a white key's place among the whites (0..15), a black key's among the blacks (0..10) */
static const int8_t WHITE_OF[NKEYS] = {0, -1, 1, -1, 2, -1, 3, 4, -1, 5, -1, 6, 7, -1, 8, -1, 9, -1, 10, 11, -1,
                                       12, -1, 13, 14, -1, 15};
static const int8_t BLACK_OF[NKEYS] = {-1, 0, -1, 1, -1, 2, -1, -1, 3, -1, 4, -1, -1, 5, -1, 6, -1, 7, -1, -1, 8,
                                       -1, 9, -1, -1, 10, -1};
static int key_at(const int8_t *table, int place)
{
    int k;
    for (k = 0; k < NKEYS; k++)
        if (table[k] == place)
            return k;
    return 0;
}
static struct {
    int track, page, view, octave;
    uint32_t btn, keys;                    /* held at the last input (pulsados) */
    int home_used;                         /* HOME was a shift while held: no view change on release */
    int save_used, arp_used;               /* SAVE / ARP were held for a combination: no action on release */
    int key_note[NKEYS];                   /* the note each held key started (-1 none) */
    char msg[2][24];                       /* a short message in the header (mensaje) */
    uint32_t msg_until;
    int pop_param;                         /* the knob turned last: its popup (ventana) */
    uint32_t pop_until;
    int bank;                              /* the bank shown in the PATTERNS view (banco a la vista) */
    int step_page, hold_step, hold_edited; /* STEPS view: which 16, the step held (-1), a knob turned meanwhile */
    int fx_page, pop_fx;                   /* EFFECTS view: its page; the popup is an effect's knob (-1 a track's) */
    float fx_meter;
    int amount;                            /* random: how much, 1..10 (cantidad) */
    uint32_t locked;                       /* random: parameters it leaves alone, a bit each (bloqueados) */
    uint32_t rng;
    int steps_seen[EPK_TRACKS];
    uint32_t mask_seen;
    int pattern_seen, queued_seen, bank_seen;
    uint8_t blink;
    uint32_t meter_at;
    float meters[EPK_TRACKS];
    uint8_t dirty_head, dirty_main, dirty_foot;
} ui;

static float rnd(void)                     /* 0..1 */
{
    ui.rng ^= ui.rng << 13;
    ui.rng ^= ui.rng >> 17;
    ui.rng ^= ui.rng << 5;
    return (float)(ui.rng >> 8) * (1.0f / 16777216.0f);
}

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
static void slot_str(int bank, int pattern, char *b)   /* A04 */
{
    b[0] = (char)('A' + bank);
    b[1] = (char)('0' + (pattern + 1) / 10);
    b[2] = (char)('0' + (pattern + 1) % 10);
    b[3] = 0;
}

static void micro_str(int micro, char *b)             /* a step's micro-timing in eighths: -2/8 early, +3/8 late */
{
    b[0] = 0;
    if (!micro) {
        str_cat(b, "0");
        return;
    }
    b[0] = micro < 0 ? '-' : '+';
    b[1] = (char)('0' + (micro < 0 ? -micro : micro));
    b[2] = 0;
    str_cat(b, "/8");
}

static void int_str(int32_t v, char *b);
static void fx_str(int p, char *b)                    /* an effect's value as the screen shows it */
{
    const float v = epk_fx_get(p);
    b[0] = 0;
    if (p == EPK_FX_DLY_TYPE)
        str_cat(b, DLY_NAME[(int)(v * (float)(EPK_DLY_COUNT - 1) + 0.5f)]);
    else if (p == EPK_FX_DLY_TIME) {
        int_str(1 + (int)(v * 15.0f + 0.5f), b);
        str_cat(b, "/16");
    } else
        int_str((int)(v * 100.0f + 0.5f), b);
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
    case EPK_P_ENGINE: str_cat(b, ENG_NAME[i]); break;
    case EPK_P_LENGTH: int_str(i + 1, b); break;
    case EPK_P_HITS: {
        int len = epk_length(track);
        int_str((int)(v * (float)len + 0.5f), b);
        break;
    }
    case EPK_P_ROTATE: int_str(i, b); break;
    case EPK_P_DIVISION: str_cat(b, DIV_NAME[i]); break;
    case EPK_P_SHIFT: {                    /* % of a step, - early (antes), + late (después) */
        int c = (int)((v - 0.5f) * 90.0f + (v < 0.5f ? -0.5f : 0.5f));
        if (c > 0)
            str_cat(b, "+");
        int_str(c, b + (c > 0));
        break;
    }
    case EPK_P_CHARACTER:
        if (epk_engine(track) == EPK_ENG_GOLPE) {
            str_cat(b, v < 0.3f ? "PALMA" : v < 0.7f ? "CAJA" : "ARO");
            break;
        }
        int_str((int)(v * 100.0f + 0.5f), b);
        break;
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
    char t[112], n[12];
    if (ann_on() && ann_where()) {
        t[0] = 0;
        str_cat(t, APP_NAME ". ");
        str_cat(t, VIEW_NAME[ui.view]);
        str_cat(t, ". PISTA ");
        int_str(ui.track + 1, n);
        str_cat(t, n);
        str_cat(t, " ");
        str_cat(t, ENG_NAME[epk_engine(ui.track)]);
        str_cat(t, ". ");
        str_cat(t, PAGE_NAME[ui.page]);
        str_cat(t, ". PATRON ");
        str_cat(t, epk_pattern_name(epk_bank_current(), epk_pattern_current()));
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
    ui.hold_step = -1;
    ui.pop_fx = -1;
    ui.amount = 10;
    ui.rng = 0x1F123BB5u;
    for (k = 0; k < NKEYS; k++)
        ui.key_note[k] = -1;
}

int epk_ble_off(void) { return 0; }       /* Bluetooth MIDI on at boot (settings: fase 6) */

/* ------------------------------------------------------------------ actions (acciones) */
static void select_track(int t)
{
    char n[8];
    ui.track = t;
    ui.hold_step = -1;
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

static void set_view(int v)
{
    ui.view = v;
    ui.hold_step = -1;
    if (v == VIEW_MIX)
        ui.page = PAGE_MIX;
    if (v == VIEW_STEPS)
        ui.page = PAGE_RHYTHM;
    if (v == VIEW_PATTERNS)
        ui.bank = epk_bank_current();
    app_say(VIEW_NAME[v], v == VIEW_PATTERNS ? "NEGRAS BANCO" : v == VIEW_STEPS ? "TECLAS BLANCAS" : v == VIEW_FX ? "ECO+REV" : "");
    ui.dirty_main = ui.dirty_foot = 1;
}

static void turn_param(int page, int slot, int32_t detents, int fine)
{
    int p = slot_param(ui.track, page, slot);
    float v;
    int steps;
    if (p < 0)
        return;
    steps = epk_param_steps(p);
    v = epk_param_get(ui.track, p);
    if (p == EPK_P_HITS) {                 /* hits: one per detent; a pattern made by hand goes back to euclid */
        v += (float)detents / (float)(epk_length(ui.track) > 0 ? epk_length(ui.track) : 1);
        if (epk_steps_manual(ui.track)) {
            epk_steps_euclid(ui.track);
            app_say("EUCLIDIANO", "");
        }
    } else if (steps)
        v += (float)detents / (float)(steps - 1);
    else
        v += (float)detents / (fine ? 400.0f : 40.0f);   /* 40 clicks end to end (de punta a punta) */
    epk_param_set(ui.track, p, fm_clampf(v, 0.0f, 1.0f));
    ui.pop_param = p;
    ui.pop_fx = -1;
    ui.pop_until = ui.view == VIEW_STEPS ? 0u : plat_ms() + 1000u;   /* the steps stay in sight (a la vista) */
    ui.dirty_main = ui.dirty_foot = 1;
}

/* EFFECTS view: one of the delay's or the reverb's knobs (un knob del eco o de la reverb) */
static void turn_fx(int slot, int32_t detents, int fine)
{
    const int p = slot >= 0 ? FX_PAGE_PARAM[ui.fx_page][slot] : SLOT_NONE;
    const int steps = p >= 0 ? epk_fx_steps(p) : 0;
    if (p < 0)
        return;
    epk_fx_set(p, epk_fx_get(p) + (float)detents / (steps ? (float)(steps - 1) : fine ? 400.0f : 40.0f));
    ui.pop_fx = p;
    ui.pop_until = plat_ms() + 1000u;
    ui.dirty_main = ui.dirty_foot = 1;
}

/* A note near this one, in D minor (re menor) */
static int scale_near(int note, int spread)
{
    static const int8_t IN_SCALE[12] = {1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0};   /* C D E F G A Bb */
    int n = note + (int)((rnd() * 2.0f - 1.0f) * (float)spread + (rnd() < 0.5f ? -0.5f : 0.5f));
    n = clamp(n, 24, 84);
    while (!IN_SCALE[n % 12])
        n--;
    return n;
}

/* Random for the page shown, selected track (aleatorio de la página): inside musical limits, never the level,
 * the length, the division or the engine; ui.amount of the way from what is there to the new value. */
static void random_page(void)
{
    const float a = (float)ui.amount / 10.0f;
    const int eng = epk_engine(ui.track), len = epk_length(ui.track);
    int slot, changed = 0;
    ui.rng ^= plat_ms() * 2654435761u;
    epk_undo_save();
    for (slot = 0; slot < 4; slot++) {
        const int p = slot_param(ui.track, ui.page, slot);
        float v, target;
        if (p < 0 || (ui.locked >> p & 1u) || p == EPK_P_ENGINE || p == EPK_P_LENGTH || p == EPK_P_DIVISION ||
            p == EPK_P_LEVEL)
            continue;
        v = epk_param_get(ui.track, p);
        if (p == EPK_P_NOTE) {
            int note = 24 + (int)(v * 60.0f + 0.5f), spread = 1 + (int)(a * 7.0f);
            if (eng == EPK_ENG_GRAVE)
                note = clamp(note + (int)((rnd() * 2.0f - 1.0f) * a * 4.0f), 28, 38);
            else if (eng == EPK_ENG_METAL)
                note = clamp(note + (int)((rnd() * 2.0f - 1.0f) * a * 8.0f), 72, 84);
            else
                note = scale_near(note, spread);
            epk_param_set(ui.track, p, (float)(note - 24) / 60.0f);
        } else {
            if (p == EPK_P_HITS) {
                target = (float)(1 + (int)(rnd() * (float)len * 0.6f)) / (float)len;
                epk_steps_euclid(ui.track);
            } else if (p == EPK_P_ROTATE)
                target = (float)(int)(rnd() * (float)len) / 31.0f;
            else if (p == EPK_P_SWING)
                target = rnd() * 0.6f;
            else if (p == EPK_P_SHIFT)
                target = 0.5f + (rnd() - 0.5f) * 0.3f;     /* near the grid (cerca de la rejilla) */
            else if (p == EPK_P_ATTACK)
                target = rnd() * rnd() * 0.6f;
            else
                target = 0.05f + rnd() * 0.9f;
            epk_param_set(ui.track, p, v + (target - v) * a);
        }
        changed++;
    }
    app_say(changed ? "ALEATORIO" : "BLOQUEADO", PAGE_NAME[ui.page]);
    ui.dirty_main = ui.dirty_foot = 1;
}

static void random_pattern(void)           /* a whole new pattern in this slot (aleatorio general) */
{
    ui.rng ^= plat_ms() * 2654435761u;
    rnd();
    epk_undo_save();
    epk_pattern_randomize(ui.rng);
    app_say("ALEATORIO", "TODO");
    ui.dirty_main = ui.dirty_foot = 1;
}

static void undo(void)
{
    if (epk_undo_ready()) {
        epk_undo_restore();
        app_say("DESHECHO", "");
    } else
        app_say("NADA QUE", "DESHACER");
    ui.dirty_main = ui.dirty_foot = 1;
}

/* STEPS view: a knob turned while a step is held (un knob con un paso mantenido) */
static void turn_step(int slot, int32_t detents)
{
    int prob = epk_step_prob(ui.track, ui.hold_step), cond = epk_step_cond(ui.track, ui.hold_step);
    int micro = epk_step_micro(ui.track, ui.hold_step);
    char m[8];
    if (slot == 0)
        prob = clamp(prob + (int)detents, 0, EPK_PROB_COUNT - 1);
    else if (slot == 1)
        cond = clamp(cond + (int)detents, 0, EPK_COND_COUNT - 1);
    else if (slot == 2)
        micro = clamp(micro + (int)detents, -3, 3);
    else
        return;
    if (!(epk_pattern(ui.track) >> ui.hold_step & 1u))
        epk_step_toggle(ui.track, ui.hold_step);       /* editing a silent step turns it on (lo enciende) */
    epk_step_set(ui.track, ui.hold_step, prob, cond, micro);
    ui.hold_edited = 1;
    micro_str(micro, m);
    app_say(slot == 0 ? "PROB" : slot == 1 ? "COND" : "MICRO", slot == 0 ? PROB_NAME[prob] : slot == 1 ? COND_NAME[cond] : m);
    ui.dirty_main = ui.dirty_foot = 1;
}

/* ------------------------------------------------------------------ input (entrada) */
static void input(void)
{
    static const int ENC_SLOT[NE] = {-1, -1, -1, 0, 1, 2, 3};
    uint32_t btn = plat_buttons(), keys, k;
    const int shift = (int)(btn >> B_HOME & 1u);
    const int save = (int)(btn >> B_SAVE & 1u), arp = (int)(btn >> B_ARP & 1u);
    int32_t e;
    touch_poll();
    keys = touch_keys();
    const int choosing = ui.view == VIEW_PATTERNS || save;   /* the keys choose banks and patterns */
    const int stepping = !choosing && ui.view == VIEW_STEPS; /* the white keys are steps */
    for (k = 0; k < NKEYS; k++) {
        uint32_t bit = 1u << k;
        const int down = (keys & bit) && !(ui.keys & bit), up = !(keys & bit) && (ui.keys & bit);
        if ((choosing || stepping) && up && ui.key_note[k] >= 0) {   /* a note held from before */
            epk_note_off(ui.track, ui.key_note[k]);
            ui.key_note[k] = -1;
        }
        if (choosing) {
            if (down && BLACK_OF[k] >= 0) {
                char n[2] = {(char)('A' + BLACK_OF[k]), 0};
                ui.bank = BLACK_OF[k];
                app_say("BANCO", n);
                ui.dirty_main = ui.dirty_foot = 1;
            } else if (down) {
                epk_pattern_select(ui.bank, WHITE_OF[k]);
                app_say(epk_playing() ? "EN COLA" : "PATRON", epk_pattern_name(ui.bank, WHITE_OF[k]));
            }
            if (down && save)
                ui.save_used = 1;          /* SAVE was held for this: no view change on its release */
            continue;
        }
        if (stepping) {
            const int step = WHITE_OF[k] >= 0 ? ui.step_page * 16 + WHITE_OF[k] : -1;
            if (step < 0 || step >= epk_length(ui.track))
                continue;
            if (down) {
                ui.hold_step = step;
                ui.hold_edited = 0;
                ui.dirty_main = ui.dirty_foot = 1;
            } else if (up && ui.hold_step == step) {
                if (!ui.hold_edited)
                    epk_step_toggle(ui.track, step);
                ui.hold_step = -1;
                ui.dirty_main = ui.dirty_foot = 1;
            }
            continue;
        }
        if (down) {                        /* the keys play the selected track (el teclado toca la pista) */
            int note = 53 + (int)k + 12 * ui.octave;
            ui.key_note[k] = note;
            epk_note_on(ui.track, note, (float)touch_vel(k) / 127.0f);
            plat_midi_send(0x09u | 0x90u << 8 | (uint32_t)note << 16 | (uint32_t)touch_vel(k) << 24, TO_USB | TO_BLE);
        } else if (up && ui.key_note[k] >= 0) {
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
            if ((btn >> B_REC & 1u) && !shift) {   /* REC held: throw this one too (throw también esta) */
                char n[8];
                epk_throw(t, 1);
                int_str(t + 1, n);
                app_say("THROW", n);
            } else if (ui.view == VIEW_MIX)
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
        } else if ((k == B_OCTDN || k == B_OCTUP) && ui.view == VIEW_STEPS) {
            ui.step_page = k == B_OCTUP && epk_length(ui.track) > 16 ? 1 : 0;
            app_say("PASOS", ui.step_page ? "17-32" : "1-16");
            ui.dirty_main = 1;
        } else if (k == B_OCTDN || k == B_OCTUP) {
            int o = clamp(ui.octave + (k == B_OCTUP ? 1 : -1), -2, 2);
            char t[6];
            ui.octave = o;
            t[0] = o < 0 ? '-' : '+';
            t[1] = (char)('0' + (o < 0 ? -o : o));
            t[2] = 0;
            app_say("OCTAVA", t);
        } else if (k == B_REC && shift) {  /* HOME+REC: freeze (congelar) */
            epk_freeze(!epk_frozen());
            app_say(epk_frozen() ? "FREEZE" : "SIN FREEZE", "ECO");
            ui.dirty_main = 1;
        } else if (k == B_REC) {           /* REC: throw the selected track to the delay */
            char n[8];
            epk_throw(ui.track, 1);
            int_str(ui.track + 1, n);
            app_say("THROW", n);
            ui.dirty_main = 1;
        }
        else if (k == B_SEQ)
            set_view(ui.view == VIEW_STEPS ? VIEW_ORBIT : VIEW_STEPS);
        else if (k == B_ARP) {
            ui.arp_used = shift;
            if (shift)
                undo();
        } else if (k == B_SAVE) {
            ui.save_used = shift;
            if (shift)
                say_later("GLOBAL", "FASE 6");
            else if (ui.view != VIEW_PATTERNS)
                ui.bank = epk_bank_current();
        }
    }
    if (!(btn >> B_REC & 1u) && (ui.btn >> B_REC & 1u)) {   /* REC released: every throw ends */
        int t;
        for (t = 0; t < EPK_TRACKS; t++)
            epk_throw(t, 0);
        ui.dirty_main = 1;
    }
    if (!save && (ui.btn >> B_SAVE & 1u)) {   /* SAVE released: the PATTERNS view (vista de patrones) */
        if (!ui.save_used)
            set_view(ui.view == VIEW_PATTERNS ? VIEW_ORBIT : VIEW_PATTERNS);
        ui.save_used = 0;
    }
    if (!arp && (ui.btn >> B_ARP & 1u)) {     /* ARP released: random (aleatorio) */
        if (!ui.arp_used)
{
            if (ui.view == VIEW_FX)
                app_say("ALEATORIO", "NO EN EFECTOS");
            else
                ui.view == VIEW_PATTERNS ? random_pattern() : random_page();
        }
        ui.arp_used = 0;
    }
    if (!shift && (ui.btn >> B_HOME & 1u)) {  /* HOME released: a tap changes the view (vista) */
        if (!ui.home_used)
            set_view(ui.view >= NVIEWS ? VIEW_ORBIT : (ui.view + 1) % NVIEWS);
        ui.home_used = 0;
    }
    for (k = 0; k < NE; k++) {
        if ((e = plat_enc((int)k)) == 0)
            continue;
        if (shift)
            ui.home_used = 1;
        if (arp) {                         /* ARP held: how much, and the locks (cantidad y bloqueos) */
            ui.arp_used = 1;
            if (k == EN_ALGO) {
                char t[8];
                ui.amount = clamp(ui.amount + (int)e, 1, 10);
                int_str(ui.amount * 10, t);
                str_cat(t, "%");
                app_say("CANTIDAD", t);
            } else if (ENC_SLOT[k] >= 0 && ui.view != VIEW_FX) {
                const int p = slot_param(ui.track, ui.page, ENC_SLOT[k]);
                if (p >= 0) {
                    ui.locked ^= 1u << p;
                    app_say(ui.locked >> p & 1u ? "BLOQUEADO" : "LIBRE", param_name(ui.track, p));
                    ui.dirty_foot = 1;
                }
            }
            continue;
        }
        if (k == EN_SELECT) {
            if (shift) {                   /* HOME+SELECT: tempo */
                char t[8];
                epk_set_tempo(epk_tempo() + (float)e);
                int_str((int32_t)(epk_tempo() + 0.5f), t);
                app_say("TEMPO", t);
            } else if (ui.view == VIEW_FX) {
                ui.fx_page = ((ui.fx_page + (int)e) % NFXPAGES + NFXPAGES) % NFXPAGES;
                app_say("PAGINA", FX_PAGE_NAME[ui.fx_page]);
                ui.dirty_foot = 1;
            } else {
                ui.page = ((ui.page + (int)e) % NPAGES + NPAGES) % NPAGES;
                app_say("PAGINA", PAGE_NAME[ui.page]);
                ui.dirty_foot = 1;
            }
        } else if (k == EN_ALGO) {
            say_later("MUTAR", "FASE 4");
        } else if (k == EN_PRESET) {
            if (shift)
                turn_param(PAGE_MIX, 2, e, 0);   /* HOME+PRESETS: the track's send to the delay */
            else
                turn_param(PAGE_MIX, 0, e, 0);   /* PRESETS: the track's level, from any page */
        } else if (ui.view == VIEW_STEPS && ui.hold_step >= 0)
            turn_step(ENC_SLOT[k], e);
        else if (ui.view == VIEW_FX)
            turn_fx(ENC_SLOT[k], e, shift);
        else
            turn_param(ui.page, ENC_SLOT[k], e, shift);
    }
    if ((btn ^ ui.btn) >> B_HOME & 1u)
        ui.dirty_foot = 1;                 /* HOME's help comes and goes (la chuleta de HOME) */
    ui.keys = keys;
    ui.btn = btn;
}

void app_input_only(void) { input(); }

static void set_lights(void)
{
    uint32_t lit = 1u << ui.track, dim = 0, whites = 0, blacks = 0, t;
    for (t = 0; t < EPK_TRACKS; t++)
        if (epk_audible((int)t))
            dim |= 1u << t;
    for (t = 0; t < NKEYS; t++) {
        if (WHITE_OF[t] >= 0)
            whites |= 1u << t;
        else
            blacks |= 1u << t;
    }
    if (epk_playing())
        lit |= 1u << B_PLAY | LED_PLAY_GREEN;
    if (epk_frozen() || (ui.btn >> B_REC & 1u))
        lit |= 1u << B_REC;                /* REC lit: a throw or a freeze (throw o freeze) */
    if (ui.btn >> B_HOME & 1u)
        lit |= 1u << B_HOME;
    if (ui.view == VIEW_PATTERNS) {        /* blacks: the bank; whites: the current pattern, the queued one blinks */
        uint32_t keys_lit = 1u << key_at(BLACK_OF, ui.bank);
        const int q = epk_pattern_queued();
        if (ui.bank == epk_bank_current())
            keys_lit |= 1u << key_at(WHITE_OF, epk_pattern_current());
        if (q >= 0 && ui.blink && ui.bank == epk_bank_queued())
            keys_lit |= 1u << key_at(WHITE_OF, q);
        plat_lights(lit | 1u << B_SAVE, keys_lit, dim, whites | blacks, 1);
        return;
    }
    if (ui.view == VIEW_STEPS) {           /* whites: the steps of this 16; the playing step winks (parpadea) */
        const uint32_t pat = epk_pattern(ui.track) >> (ui.step_page * 16) & 0xFFFFu;
        const int len = epk_length(ui.track) - ui.step_page * 16, cur = epk_step(ui.track) - ui.step_page * 16;
        uint32_t keys_lit = 0, keys_dim = 0;
        int s;
        for (s = 0; s < 16 && s < len; s++) {
            keys_dim |= 1u << key_at(WHITE_OF, s);
            if ((pat >> s & 1u) != (uint32_t)(s == cur))
                keys_lit |= 1u << key_at(WHITE_OF, s);
        }
        plat_lights(lit | 1u << B_SEQ, keys_lit, dim, keys_dim, 1);
        return;
    }
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
        if (ui.view != VIEW_PATTERNS) {
            int_str(ui.track + 1, t);
            x = cv_text(x + 10, 4, &FONT_B, t, TRACK_C[ui.track]);
            x = cv_text(x + 4, 7, &FONT_XS, ENG_NAME[epk_engine(ui.track)], C_LABEL);
            if (ui.view == VIEW_STEPS && epk_steps_manual(ui.track))
                cv_text(x + 5, 7, &FONT_XS, "A MANO", C_SOLO);
        }
    }
    int_str((int32_t)(epk_tempo() + 0.5f), t);
    x = 232 - text_w(&FONT_B, t);
    cv_text(x, 4, &FONT_B, t, C_TEXT);
    {                                      /* the pattern (patrón): A04, and >B08 when one is queued */
        char p[8];
        const int q = epk_pattern_queued();
        int32_t px;
        slot_str(epk_bank_current(), epk_pattern_current(), p);
        px = x - 26 - text_w(&FONT_S, p) - (q >= 0 ? 34 : 0);
        px = cv_text(px, 6, &FONT_S, p, C_LABEL);
        if (q >= 0) {
            p[0] = '>';
            slot_str(epk_bank_queued(), q, p + 1);
            cv_text(px + 2, 6, &FONT_S, p, ui.blink ? C_SOLO : C_LABEL);
        }
    }
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
            if (s_i == cur && (pat >> s_i & 1u))
                cv_rect(x - 3, y - 3, 7, 7, on ? C_TEXT : C_LABEL);         /* a hit playing (golpe sonando) */
            else if (s_i == cur)
                cv_rect(x - 1, y - 1, 3, 3, C_LABEL);                       /* where the track is (dónde va) */
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
    const int fx = ui.pop_fx >= 0, p = fx ? ui.pop_fx : ui.pop_param, steps = fx ? epk_fx_steps(p) : epk_param_steps(p);
    const float val = fx ? epk_fx_get(p) : epk_param_get(ui.track, p);
    const uint16_t c = fx ? C_FX : TRACK_C[ui.track];
    const char *name = fx ? FX_NAME[p] : param_name(ui.track, p), *hint = fx ? FX_HINT[p] : param_hint(ui.track, p);
    cv_rect(12, 8, 216, 128, C_PANEL);
    cv_rect(12, 8, 216, 1, c);
    cv_rect(12, 135, 216, 1, c);
    if (!name || !hint)
        return;
    cv_text(120 - text_w(&FONT_S, name) / 2, 16, &FONT_S, name, c);
    if (fx)
        fx_str(p, v);
    else
        param_str(ui.track, p, v);
    cv_text(120 - text_w(&FONT_L, v) / 2, 40, &FONT_L, v, C_TEXT);
    cv_rect(28, 90, 184, 4, C_RULE);
    if (steps)
        cv_rect(28 + (int32_t)(val * 180.0f), 86, 4, 12, c);
    else
        cv_rect(28, 90, (int32_t)(val * 184.0f + 0.5f), 4, c);
    cv_text(120 - text_w(&FONT_XS, hint) / 2, 108, &FONT_XS, hint, C_LABEL);
}

static void draw_patterns(void)             /* the banks A..K, and 4 x 4: the bank's 16 patterns */
{
    const int same = ui.bank == epk_bank_current(), cur = same ? epk_pattern_current() : -1;
    const int q = ui.bank == epk_bank_queued() ? epk_pattern_queued() : -1;
    int i;
    char n[4];
    for (i = 0; i < EPK_BANKS; i++) {
        const int32_t x = 3 + i * 21;
        n[0] = (char)('A' + i);
        n[1] = 0;
        cv_rect(x, 2, 19, 15, i == ui.bank ? TRACK_C[2] : C_PANEL);
        cv_text(x + 9 - text_w(&FONT_XS, n) / 2, 4, &FONT_XS, n,
                i == ui.bank ? C_BG : i == epk_bank_current() ? C_TEXT : C_LABEL);
    }
    for (i = 0; i < EPK_PATTERNS; i++) {
        const int32_t x = 2 + (i % 4) * 59, y = 20 + (i / 4) * 31;
        const uint16_t edge = i == cur ? TRACK_C[2] : (i == q && ui.blink) ? C_SOLO : C_PANEL;
        cv_rect(x, y, 57, 29, edge);
        cv_rect(x + 2, y + 2, 53, 25, i == cur ? RGB(24, 44, 52) : C_PANEL);
        n[0] = (char)('0' + (i + 1) / 10);
        n[1] = (char)('0' + (i + 1) % 10);
        n[2] = 0;
        cv_text(x + 5, y + 3, &FONT_XS, n, i == cur ? TRACK_C[2] : C_LABEL);
        cv_text(x + 5, y + 14, &FONT_XS, epk_pattern_name(ui.bank, i), i == cur ? C_TEXT : C_LABEL);
    }
}

static void draw_steps(void)                /* two rows of 16 steps (dos filas de 16 pasos) */
{
    const int len = epk_length(ui.track), cur = epk_step(ui.track);
    const uint32_t pat = epk_pattern(ui.track);
    const uint16_t c = TRACK_C[ui.track];
    int s;
    for (s = 0; s < 32; s++) {
        const int row = s / 16;
        const int32_t x = 9 + (s % 16) * 14, y = 10 + row * 68, h = 54;
        const int hit = (int)(pat >> s & 1u);
        if (s >= len) {
            cv_rect(x + 5, y + h / 2, 2, 2, C_RULE);                       /* beyond the length (fuera del ciclo) */
            continue;
        }
        cv_rect(x, y, 12, h, s == ui.hold_step ? C_TEXT : row == ui.step_page ? C_RULE : C_PANEL);
        cv_rect(x + 1, y + 1, 10, h - 2, C_PANEL);
        if (hit) {                                                        /* the bar's height is its chance */
            const int32_t bar = (h - 4) * (4 - epk_step_prob(ui.track, s)) / 4;
            cv_rect(x + 2, y + h - 2 - bar, 8, bar, c);
            if (epk_step_cond(ui.track, s))
                cv_rect(x + 4, y + 3, 4, 4, C_SOLO);                       /* it has a condition (condición) */
            if (epk_step_micro(ui.track, s))                              /* early or late: a tick to that side */
                cv_rect(x + 5 + epk_step_micro(ui.track, s), y + h - 7, 2, 4, C_TEXT);
        }
        if (s == cur)
            cv_rect(x, y + h + 2, 12, 3, C_TEXT);
        if (s % 4 == 0)
            cv_rect(x, y - 4, 12, 1, C_LABEL);
    }
    cv_rect(2, 10 + ui.step_page * 68, 3, 54, c);                          /* the row the keys edit (fila activa) */
}

static void draw_fx(void)                         /* the delay, the reverb, and what each track sends them */
{
    char v[16];
    int t;
    const int32_t m = (int32_t)(fm_minf(ui.fx_meter * 1.5f, 1.0f) * 76.0f + 0.5f);
    cv_rect(4, 4, 112, 80, C_PANEL);                                  /* the delay (eco) */
    cv_rect(4, 4, 112, 1, ui.fx_page < 2 ? C_FX : C_RULE);
    cv_text(10, 8, &FONT_XS, "A  ECO", ui.fx_page < 2 ? C_FX : C_LABEL);
    fx_str(EPK_FX_DLY_TYPE, v);
    cv_text(10, 24, text_w(&FONT_M, v) > 100 ? &FONT_S : &FONT_M, v, C_TEXT);
    fx_str(EPK_FX_DLY_TIME, v);
    cv_text(10, 52, &FONT_S, v, C_TEXT);
    if (epk_frozen())
        cv_text(112 - text_w(&FONT_XS, "FREEZE"), 54, &FONT_XS, "FREEZE", C_SOLO);
    else if (ui.btn >> B_REC & 1u)
        cv_text(112 - text_w(&FONT_XS, "THROW"), 54, &FONT_XS, "THROW", C_MUTE);
    cv_rect(10, 74, 100, 3, C_RULE);
    cv_rect(10, 74, (int32_t)(epk_fx_get(EPK_FX_DLY_FEEDBACK) * 100.0f + 0.5f), 3, C_FX);
    cv_rect(120, 4, 108, 80, C_PANEL);                                /* the reverb */
    cv_rect(120, 4, 108, 1, ui.fx_page == 2 ? C_FX : C_RULE);
    cv_text(126, 8, &FONT_XS, "B  REVERB", ui.fx_page == 2 ? C_FX : C_LABEL);
    cv_text(126, 24, &FONT_M, "PLACA", C_TEXT);
    cv_text(126, 52, &FONT_XS, "CAIDA", C_LABEL);
    cv_rect(126, 74, 96, 3, C_RULE);
    cv_rect(126, 74, (int32_t)(epk_fx_get(EPK_FX_REV_DECAY) * 96.0f + 0.5f), 3, C_FX);
    cv_rect(232, 6, 4, 76, C_PANEL);                                  /* the returns' meter (medidor) */
    cv_rect(232, 6 + 76 - m, 4, m, C_TEXT);
    cv_text(4, 90, &FONT_XS, "ENVIOS", C_LABEL);                      /* each track's sends: A then B */
    for (t = 0; t < EPK_TRACKS; t++) {
        const int32_t x = 52 + t * 31, top = 92, h = 46;
        const int32_t a = (int32_t)(epk_param_get(t, EPK_P_SEND_A) * (float)h + 0.5f);
        const int32_t b = (int32_t)(epk_param_get(t, EPK_P_SEND_B) * (float)h + 0.5f);
        const uint16_t c = epk_audible(t) ? TRACK_C[t] : dim_colour(TRACK_C[t]);
        int_str(t + 1, v);
        cv_text(x - 9, top + 30, &FONT_XS, v, t == ui.track ? C_TEXT : TRACK_C[t]);
        cv_rect(x, top, 7, h, C_PANEL);
        cv_rect(x, top + h - a, 7, a, c);
        cv_rect(x + 9, top, 7, h, C_PANEL);
        cv_rect(x + 9, top + h - b, 7, b, dim_colour(c));
        if (t == ui.track)
            cv_rect(x, top + h + 2, 16, 1, C_TEXT);
    }
    cv_text(4, 104, &FONT_XS, "A  B", C_RULE);
}

static void draw_main(void)
{
    cv_begin(240, 144, C_BG);
    if (plat_ms() < ui.pop_until)
        draw_popup();
    else if (ui.view == VIEW_PATTERNS)
        draw_patterns();
    else if (ui.view == VIEW_STEPS)
        draw_steps();
    else if (ui.view == VIEW_FX)
        draw_fx();
    else if (ui.view == VIEW_ORBIT)
        draw_orbit();
    else
        draw_mixer();
    cv_blit(0, 24);
}

static void draw_card(int i, const char *name, const char *value, float bar, int locked)
{
    const int32_t cx = i * 60 + 2;
    cv_rect(cx, 18, 56, 52, C_PANEL);
    cv_text(cx + 4, 22, &FONT_XS, name, locked ? C_SOLO : C_LABEL);
    cv_text(cx + 4, 36, text_w(&FONT_M, value) > 50 ? &FONT_S : &FONT_M, value, C_TEXT);
    cv_rect(cx + 4, 63, 48, 3, C_RULE);
    if (bar >= 0.0f)
        cv_rect(cx + 4, 63, (int32_t)(bar * 48.0f + 0.5f), 3, locked ? C_SOLO : TRACK_C[ui.track]);
}

static void draw_foot(void)                       /* the page tabs and the four knobs (pestañas y knobs) */
{
    const uint16_t c = TRACK_C[ui.track];
    int i;
    int32_t x = 6;
    char v[16];
    cv_begin(240, 72, C_BG);
    cv_rect(0, 0, 240, 1, C_RULE);
    if (ui.btn >> B_HOME & 1u) {              /* HOME held: what it does with the rest (chuleta de HOME) */
        cv_text(6, 4, &FONT_XS, "HOME +", c);
        cv_text(6, 19, &FONT_XS, "SELECT: tempo        PISTA: mute", C_TEXT);
        cv_text(6, 32, &FONT_XS, "ARP: deshacer        K1-K4: ajuste fino", C_TEXT);
        cv_text(6, 45, &FONT_XS, "PRESETS: envio al eco   En MEZCLA, PISTA: solo", C_TEXT);
        cv_text(6, 58, &FONT_XS, "REC: freeze   Soltar solo: cambia de vista", C_LABEL);
        cv_blit(0, 168);
        return;
    }
    if (ui.view == VIEW_PATTERNS) {
        const int q = ui.bank == epk_bank_queued() ? epk_pattern_queued() : -1;
        const int same = ui.bank == epk_bank_current();
        v[0] = 0;
        str_cat(v, "BANCO ");
        v[6] = (char)('A' + ui.bank);
        v[7] = 0;
        cv_text(6, 5, &FONT_XS, q >= 0 ? "EN COLA" : v, q >= 0 ? C_SOLO : C_LABEL);
        cv_text(6, 18, &FONT_M, q >= 0 ? epk_pattern_name(ui.bank, q) : same
                ? epk_pattern_name(ui.bank, epk_pattern_current()) : "ELIGE PATRON", C_TEXT);
        cv_text(6, 44, &FONT_XS, "Negras: banco   Blancas: patron", C_LABEL);
        cv_text(6, 57, &FONT_XS, "ARP: patron al azar   HOME+ARP: deshacer", C_LABEL);
        cv_blit(0, 168);
        return;
    }
    if (ui.view == VIEW_FX) {                 /* the effects' pages and knobs (páginas y knobs de EFECTOS) */
        for (i = 0; i < NFXPAGES; i++)
            x = cv_text(x, 3, &FONT_XS, FX_PAGE_NAME[i], i == ui.fx_page ? C_FX : C_LABEL) + 10;
        cv_text(236 - text_w(&FONT_XS, "PRESETS+HOME: ENVIO"), 3, &FONT_XS, "PRESETS+HOME: ENVIO", C_RULE);
        for (i = 0; i < 4; i++) {
            const int p = FX_PAGE_PARAM[ui.fx_page][i];
            const int32_t cx = i * 60 + 2;
            cv_rect(cx, 18, 56, 52, C_PANEL);
            if (p < 0)
                continue;
            fx_str(p, v);
            if (text_w(&FONT_S, v) > 50)
                v[4] = 0;                                              /* PINGPONG -> PING on the small card */
            cv_text(cx + 4, 22, &FONT_XS, FX_NAME[p], C_LABEL);
            cv_text(cx + 4, 36, text_w(&FONT_M, v) > 50 ? &FONT_S : &FONT_M, v, C_TEXT);
            cv_rect(cx + 4, 63, 48, 3, C_RULE);
            cv_rect(cx + 4, 63, (int32_t)(epk_fx_get(p) * 48.0f + 0.5f), 3, C_FX);
        }
        cv_blit(0, 168);
        return;
    }
    if (ui.view == VIEW_STEPS && ui.hold_step >= 0) {
        cv_text(6, 3, &FONT_XS, "PASO MANTENIDO:  K1 PROB  K2 COND  K3 MICRO", c);
        micro_str(epk_step_micro(ui.track, ui.hold_step), v);
        draw_card(2, "MICRO", v, -1.0f, 0);
        int_str(ui.hold_step + 1, v);
        draw_card(0, "PROB", PROB_NAME[epk_step_prob(ui.track, ui.hold_step)],
                  (float)(4 - epk_step_prob(ui.track, ui.hold_step)) / 4.0f, 0);
        draw_card(1, "COND", COND_NAME[epk_step_cond(ui.track, ui.hold_step)], -1.0f, 0);
        draw_card(3, "PASO", v, -1.0f, 0);
        cv_blit(0, 168);
        return;
    }
    for (i = 0; i < NPAGES; i++)
        x = cv_text(x, 3, &FONT_XS, PAGE_NAME[i], i == ui.page ? c : C_LABEL) + 7;
    for (i = 0; i < 4; i++) {
        const int p = slot_param(ui.track, ui.page, i);
        const int32_t cx = i * 60 + 2;
        if (p < 0) {
            cv_rect(cx, 18, 56, 52, C_PANEL);
        } else {
            param_str(ui.track, p, v);
            draw_card(i, param_name(ui.track, p), v, epk_param_get(ui.track, p), (int)(ui.locked >> p & 1u));
        }
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
    for (t = 0; t < EPK_TRACKS; t++) {        /* a step moved: the orbit and the steps are redrawn (paso nuevo) */
        int s = epk_step((int)t);
        if (s != ui.steps_seen[t]) {
            ui.steps_seen[t] = s;
            if (ui.view == VIEW_ORBIT || (ui.view == VIEW_STEPS && (int)t == ui.track))
                ui.dirty_main = 1;
        }
    }
    if (epk_pattern(ui.track) != ui.mask_seen) {   /* the hits changed (cambiaron los golpes) */
        ui.mask_seen = epk_pattern(ui.track);
        ui.dirty_main = ui.dirty_foot = 1;
    }
    {                                         /* the pattern changed or one is queued (cambio / cola) */
        int cur = epk_pattern_current(), q = epk_pattern_queued(), bank = epk_bank_current();
        uint8_t blink = (uint8_t)((now / 250u) & 1u);
        if (cur != ui.pattern_seen || q != ui.queued_seen || bank != ui.bank_seen || (q >= 0 && blink != ui.blink)) {
            if (cur != ui.pattern_seen || bank != ui.bank_seen)
                ui.dirty_main = ui.dirty_foot = 1;
            ui.pattern_seen = cur;
            ui.queued_seen = q;
            ui.bank_seen = bank;
            ui.blink = blink;
            ui.dirty_head = 1;
            if (ui.view == VIEW_PATTERNS)
                ui.dirty_main = ui.dirty_foot = 1;
        }
    }
    if (ui.view == VIEW_FX && now - ui.meter_at >= 50u) {    /* the returns' meter (medidor de los retornos) */
        ui.meter_at = now;
        ui.fx_meter = ui.fx_meter * 0.6f + epk_fx_meter();
        ui.dirty_main = 1;
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
    const char *u = strchr(what, '_');
    if (!strcmp(what, "track"))
        *got = ui.track;
    else if (!strcmp(what, "view"))
        *got = ui.view;
    else if (!strcmp(what, "page"))
        *got = ui.page;
    else if (!strcmp(what, "pattern"))         /* 1..16 */
        *got = epk_pattern_current() + 1;
    else if (!strcmp(what, "queued"))          /* 1..16, 0 none */
        *got = epk_pattern_queued() + 1;
    else if (!strcmp(what, "bank"))            /* 0 = A */
        *got = epk_bank_current();
    else if (!strcmp(what, "octave"))
        *got = ui.octave;
    else if (!strcmp(what, "playing"))
        *got = epk_playing();
    else if (!strcmp(what, "tempo"))
        *got = (int)(epk_tempo() + 0.5f);
    else if (!strcmp(what, "freeze"))
        *got = epk_frozen();
    else if (!strncmp(what, "throw", 5))
        *got = epk_thrown(atoi(what + 5) - 1);
    else if (!strcmp(what, "fxpage"))
        *got = ui.fx_page;
    else if (!strncmp(what, "fx", 2) && what[2] >= '0' && what[2] <= '9') {   /* fxK: effect parameter K, x100 */
        const int k = atoi(what + 2);
        *got = epk_fx_steps(k) ? (int)(epk_fx_get(k) * (float)(epk_fx_steps(k) - 1) + 0.5f) : (int)(epk_fx_get(k) * 100.0f + 0.5f);
    } else if (!strncmp(what, "senda", 5))
        *got = (int)(epk_param_get(atoi(what + 5) - 1, EPK_P_SEND_A) * 100.0f + 0.5f);
    else if (!strncmp(what, "sendb", 5))
        *got = (int)(epk_param_get(atoi(what + 5) - 1, EPK_P_SEND_B) * 100.0f + 0.5f);
    else if (!strcmp(what, "amount"))
        *got = ui.amount;
    else if (!strcmp(what, "undo"))
        *got = epk_undo_ready();
    else if (!strncmp(what, "mute", 4))       /* muteN, soloN, stepN, levelN (x100), engineN, manualN, maskN,
                                               * hitsN, trigN, trigN_K (1 if at least K), probN_S, condN_S: track N */
        *got = epk_muted(atoi(what + 4) - 1);
    else if (!strncmp(what, "solo", 4))
        *got = epk_soloed(atoi(what + 4) - 1);
    else if (!strncmp(what, "step", 4))
        *got = epk_step(atoi(what + 4) - 1);
    else if (!strncmp(what, "level", 5))
        *got = (int)(epk_param_get(atoi(what + 5) - 1, EPK_P_LEVEL) * 100.0f + 0.5f);
    else if (!strncmp(what, "engine", 6))
        *got = epk_engine(atoi(what + 6) - 1);
    else if (!strncmp(what, "manual", 6))
        *got = epk_steps_manual(atoi(what + 6) - 1);
    else if (!strncmp(what, "mask", 4))
        *got = (int)(epk_pattern(atoi(what + 4) - 1) & 0xFFFFu);
    else if (!strncmp(what, "hits", 4))
        *got = (int)(epk_param_get(atoi(what + 4) - 1, EPK_P_HITS) * (float)epk_length(atoi(what + 4) - 1) + 0.5f);
    else if (!strncmp(what, "prob", 4) && u)
        *got = epk_step_prob(atoi(what + 4) - 1, atoi(u + 1));
    else if (!strncmp(what, "cond", 4) && u)
        *got = epk_step_cond(atoi(what + 4) - 1, atoi(u + 1));
    else if (!strncmp(what, "micro", 5) && u)
        *got = epk_step_micro(atoi(what + 5) - 1, atoi(u + 1));
    else if (!strncmp(what, "trig", 4)) {
        int n = (int)epk_triggers(atoi(what + 4) - 1);
        *got = u ? n >= atoi(u + 1) : n;
    } else
        return 0;
    return 1;
}
#endif
